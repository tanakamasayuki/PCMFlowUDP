#include "VbanReceiver.h"

#include <string.h>

// PCMFlowUDP :: VbanReceiver implementation.
//
// poll() pulls one UDP datagram, validates the VBAN signature and
// sub-protocol, and (for Audio / PCM16) copies the payload into an
// internal ring buffer. readFrames() pops samples from the ring in
// PCMSource form.
//
// Drop policy on ring overflow: drop-oldest. Real-time audio prefers
// "lose old samples" over "grow latency".
//
// v0.1.x decodes PCM16 only. Other sub-codecs are detected and
// reported via lastError() = FormatUnsupported; the packet is dropped.

namespace
{
    // Append `n` bytes from `src` into the ring; discard oldest bytes
    // first if there is no room.
    void ringPush(uint8_t *queue, size_t cap,
                  size_t &head, size_t &tail, size_t &count,
                  const uint8_t *src, size_t n)
    {
        if (n >= cap)
        {
            // New packet alone overruns the ring: keep only the most
            // recent `cap` bytes.
            src += (n - cap);
            n = cap;
            head = 0;
            tail = 0;
            count = 0;
        }
        if (count + n > cap)
        {
            const size_t drop = count + n - cap;
            head = (head + drop) % cap;
            count -= drop;
        }
        for (size_t i = 0; i < n; ++i)
        {
            queue[tail] = src[i];
            tail = (tail + 1) % cap;
        }
        count += n;
    }

    size_t ringPop(uint8_t *queue, size_t cap,
                   size_t &head, size_t &count,
                   uint8_t *dst, size_t n)
    {
        const size_t take = (n < count) ? n : count;
        for (size_t i = 0; i < take; ++i)
        {
            dst[i] = queue[head];
            head = (head + 1) % cap;
        }
        count -= take;
        return take;
    }
} // namespace

bool VbanReceiver::begin(uint16_t localPort, const char *streamName)
{
    localPort_ = localPort;
    memset(filterName_, 0, sizeof(filterName_));
    filterEnabled_ = (streamName != nullptr && streamName[0] != '\0');
    if (filterEnabled_)
    {
        const size_t n = strnlen(streamName, pcmflowudp::kVbanStreamNameBytes);
        memcpy(filterName_, streamName, n);
    }
    if (udp_.begin(localPort_) != 1)
    {
        error_ = Error::BindFailed;
        ready_ = false;
        return false;
    }
    queueHead_ = queueTail_ = queueCount_ = 0;
    lastFrameCounter_ = 0;
    format_ = PCMFormat{};
    memset(currentStream_, 0, sizeof(currentStream_));
    ready_ = true;
    error_ = Error::None;
    return true;
}

void VbanReceiver::end()
{
    udp_.stop();
    queueHead_ = queueTail_ = queueCount_ = 0;
    ready_ = false;
    error_ = Error::NotReady;
}

bool VbanReceiver::poll()
{
    if (!ready_)
        return false;
    const int n = udp_.parsePacket();
    if (n <= 0)
        return false;

    uint8_t buf[pcmflowudp::kVbanMaxPacketBytes];
    const size_t toRead = (static_cast<size_t>(n) < sizeof(buf))
                              ? static_cast<size_t>(n)
                              : sizeof(buf);
    const int got = udp_.read(buf, toRead);
    if (got < static_cast<int>(pcmflowudp::kVbanHeaderBytes))
        return false;

    pcmflowudp::VbanAudioHeader h{};
    const pcmflowudp::VbanParseResult r =
        pcmflowudp::parseAudioHeader(buf, static_cast<size_t>(got), h);
    if (r != pcmflowudp::VbanParseResult::Ok)
        return false;

    if (filterEnabled_ &&
        strncmp(h.streamName, filterName_, pcmflowudp::kVbanStreamNameBytes) != 0)
    {
        return false;
    }

    if (h.subCodec != pcmflowudp::VbanSubCodec::PCM16)
    {
        error_ = Error::FormatUnsupported;
        return false;
    }

    // Lock format on the first accepted packet. Subsequent packets are
    // accepted only if the rate / channel count still match; mismatches
    // are dropped (caller can detect by lack of progress).
    const uint32_t rate = pcmflowudp::vbanSampleRateHz(h.sampleRateIndex);
    if (rate == 0 || h.numChannels < 1 || h.numChannels > 2)
    {
        error_ = Error::FormatUnsupported;
        return false;
    }
    if (format_.sampleRate == 0)
    {
        format_.sampleRate = rate;
        format_.channels = static_cast<uint8_t>(h.numChannels);
        format_.bitsPerSample = 16;
    }
    else if (format_.sampleRate != rate ||
             format_.channels != static_cast<uint8_t>(h.numChannels))
    {
        return false;
    }

    const size_t payloadBytes =
        static_cast<size_t>(got) - pcmflowudp::kVbanHeaderBytes;
    const size_t expectedPayload =
        static_cast<size_t>(h.numSamples) * format_.bytesPerFrame();
    const size_t usable =
        (payloadBytes < expectedPayload) ? payloadBytes : expectedPayload;
    // Trim partial frames at the tail.
    const size_t aligned = usable - (usable % format_.bytesPerFrame());

    ringPush(queue_, kQueueBytes, queueHead_, queueTail_, queueCount_,
             buf + pcmflowudp::kVbanHeaderBytes, aligned);

    memcpy(currentStream_, h.streamName, sizeof(currentStream_));
    lastFrameCounter_ = h.frameCounter;
    return true;
}

size_t VbanReceiver::readFrames(void *out, size_t frameCount)
{
    if (!ready_ || out == nullptr || frameCount == 0)
        return 0;
    if (format_.bytesPerFrame() == 0)
        return 0;

    const size_t bytesPerFrame = format_.bytesPerFrame();
    const size_t wantBytes = frameCount * bytesPerFrame;
    // Round available down to a frame boundary so we never return a
    // half-frame.
    const size_t availFrames = queueCount_ / bytesPerFrame;
    const size_t takeFrames =
        (frameCount < availFrames) ? frameCount : availFrames;
    const size_t takeBytes = takeFrames * bytesPerFrame;

    if (takeBytes == 0)
        return 0;
    (void)wantBytes;

    return ringPop(queue_, kQueueBytes, queueHead_, queueCount_,
                   static_cast<uint8_t *>(out), takeBytes) /
           bytesPerFrame;
}
