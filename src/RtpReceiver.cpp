#include "RtpReceiver.h"

#include <string.h>

// PCMFlowUDP :: RtpReceiver implementation.

namespace
{
    void ringPush(uint8_t *q, size_t cap, size_t &head, size_t &tail,
                  size_t &count, const uint8_t *src, size_t n)
    {
        if (n >= cap)
        {
            src += (n - cap);
            n = cap;
            head = tail = count = 0;
        }
        if (count + n > cap)
        {
            const size_t drop = count + n - cap;
            head = (head + drop) % cap;
            count -= drop;
        }
        for (size_t i = 0; i < n; ++i)
        {
            q[tail] = src[i];
            tail = (tail + 1) % cap;
        }
        count += n;
    }

    size_t ringPop(uint8_t *q, size_t cap, size_t &head, size_t &count,
                   uint8_t *dst, size_t n)
    {
        const size_t take = (n < count) ? n : count;
        for (size_t i = 0; i < take; ++i)
        {
            dst[i] = q[head];
            head = (head + 1) % cap;
        }
        count -= take;
        return take;
    }
} // namespace

bool RtpReceiver::begin(uint16_t localPort)
{
    localPort_ = localPort;
    if (udp_.begin(localPort_) != 1)
    {
        error_ = Error::BindFailed;
        ready_ = false;
        return false;
    }
    queueHead_ = queueTail_ = queueCount_ = 0;
    encodedBytes_ = 0;
    encodedReady_ = false;
    lastPt_ = 0xFF;
    ready_ = true;
    error_ = Error::None;
    return true;
}

void RtpReceiver::end()
{
    udp_.stop();
    queueHead_ = queueTail_ = queueCount_ = 0;
    encodedBytes_ = 0;
    encodedReady_ = false;
    ready_ = false;
    error_ = Error::NotReady;
}

bool RtpReceiver::setFormat(const PCMFormat &fmt)
{
    if (!fmt.isValid() || fmt.bitsPerSample != 16)
    {
        error_ = Error::BadHeader;
        return false;
    }
    format_ = fmt;
    return true;
}

bool RtpReceiver::poll()
{
    if (!ready_)
        return false;
    const int n = udp_.parsePacket();
    if (n <= 0)
        return false;

    static uint8_t buf[pcmflowudp::kRtpMaxPacketBytes];
    const size_t toRead = (static_cast<size_t>(n) < sizeof(buf))
                              ? static_cast<size_t>(n)
                              : sizeof(buf);
    const int got = udp_.read(buf, toRead);
    if (got < static_cast<int>(pcmflowudp::kRtpHeaderBytes))
        return false;

    pcmflowudp::RtpHeader h{};
    const pcmflowudp::RtpParseResult r =
        pcmflowudp::parseRtpHeader(buf, static_cast<size_t>(got), h);
    if (r != pcmflowudp::RtpParseResult::Ok)
    {
        error_ = Error::BadHeader;
        return false;
    }

    lastPt_ = h.payloadType;
    lastSeq_ = h.sequenceNumber;
    lastTs_ = h.timestamp;
    lastSsrc_ = h.ssrc;
    lastMarker_ = h.marker;

    const size_t payloadStart = h.payloadOffset;
    const size_t payloadBytes = static_cast<size_t>(got) - payloadStart;

    if (isPcm())
    {
        // Lock format from wire on first L16 packet (or honor caller's
        // setFormat). PT 10 = stereo, PT 11 = mono.
        const uint8_t channels =
            (h.payloadType == static_cast<uint8_t>(pcmflowudp::RtpPayloadType::L16Stereo)) ? 2u : 1u;
        if (format_.sampleRate == 0)
        {
            // Sample rate is not on the wire; default a reasonable
            // value if caller didn't set one. 8000 Hz is the safest
            // assumption for an unknown L16 stream (matches PCMU/PCMA
            // companion timing).
            format_.sampleRate = 8000;
        }
        format_.channels = channels;
        format_.bitsPerSample = 16;

        const size_t bytesPerFrame = format_.bytesPerFrame();
        const size_t aligned = payloadBytes - (payloadBytes % bytesPerFrame);

        // BE -> host byte swap into a scratch then push to ring.
        static uint8_t scratch[pcmflowudp::kRtpMaxPayloadBytes];
        for (size_t i = 0; i + 1 < aligned; i += 2)
        {
            scratch[i] = buf[payloadStart + i + 1];
            scratch[i + 1] = buf[payloadStart + i];
        }
        ringPush(queue_, kQueueBytes, queueHead_, queueTail_, queueCount_,
                 scratch, aligned);
    }
    else
    {
        // Non-L16: hold one packet's payload. Drop-newest semantics
        // for the *application-visible* hold (most recently arrived
        // packet wins on contention between poll() calls).
        const size_t cap = sizeof(encoded_);
        encodedBytes_ = (payloadBytes < cap) ? payloadBytes : cap;
        memcpy(encoded_, buf + payloadStart, encodedBytes_);
        encodedReady_ = true;
    }
    return true;
}

size_t RtpReceiver::readFrames(void *out, size_t frameCount)
{
    if (!ready_ || out == nullptr || frameCount == 0)
        return 0;
    if (format_.bytesPerFrame() == 0)
        return 0;

    const size_t bytesPerFrame = format_.bytesPerFrame();
    const size_t availFrames = queueCount_ / bytesPerFrame;
    const size_t takeFrames =
        (frameCount < availFrames) ? frameCount : availFrames;
    if (takeFrames == 0)
        return 0;
    return ringPop(queue_, kQueueBytes, queueHead_, queueCount_,
                   static_cast<uint8_t *>(out), takeFrames * bytesPerFrame) /
           bytesPerFrame;
}

size_t RtpReceiver::readEncoded(uint8_t *bytes, size_t maxBytes)
{
    if (!encodedReady_ || bytes == nullptr || maxBytes < encodedBytes_)
        return 0;
    memcpy(bytes, encoded_, encodedBytes_);
    const size_t n = encodedBytes_;
    encodedReady_ = false;
    encodedBytes_ = 0;
    return n;
}
