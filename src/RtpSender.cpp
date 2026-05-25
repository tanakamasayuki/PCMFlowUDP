#include "RtpSender.h"

#include <string.h>

// PCMFlowUDP :: RtpSender implementation.
//
// Two emission paths:
//   * L16 (PT 10/11): writeFrames buffers PCM samples; when the buffer
//     reaches samplesPerPacket_ the packet is emitted with a fresh
//     header (sequence ++, timestamp += samplesPerPacket_).
//   * Encoded (other PT): writeEncoded sends one packet per call;
//     sequence ++, timestamp += tsIncrement_ (caller's choice).
//
// The marker bit is set on the first packet after begin() and on any
// packet for which setMarker(true) was called immediately before.

namespace
{
    uint32_t randomSsrc()
    {
        // Arduino random() is uint32-ish; combine two halves for a full
        // 32-bit value (esp32 random() returns 31-bit non-negative).
        const uint32_t hi = static_cast<uint32_t>(random(0, 0x7FFF));
        const uint32_t lo = static_cast<uint32_t>(random(0, 0x10000));
        return (hi << 17) ^ (lo) ^ static_cast<uint32_t>(micros());
    }

    uint16_t randomSeqStart()
    {
        return static_cast<uint16_t>(random(0, 0x10000));
    }

    uint32_t randomTsStart()
    {
        const uint32_t hi = static_cast<uint32_t>(random(0, 0x10000));
        const uint32_t lo = static_cast<uint32_t>(random(0, 0x10000));
        return (hi << 16) ^ lo;
    }

    // Default packetization for L16: ~20 ms at the configured clock,
    // capped by the byte budget per packet.
    uint16_t computeSamplesPerPacket(uint32_t clockRate, uint8_t channels)
    {
        const size_t bytesPerFrame = static_cast<size_t>(channels) * 2u;
        const size_t maxByPayload =
            pcmflowudp::kRtpMaxPayloadBytes / bytesPerFrame;
        const size_t maxByTime = (clockRate / 50u); // 20 ms
        size_t spp = (maxByTime < maxByPayload) ? maxByTime : maxByPayload;
        if (spp == 0)
            spp = 1;
        return static_cast<uint16_t>(spp);
    }
} // namespace

bool RtpSender::begin(IPAddress destIp, uint16_t destPort, uint32_t ssrc)
{
    dest_ = destIp;
    destPort_ = destPort;
    ssrc_ = (ssrc != 0) ? ssrc : randomSsrc();
    sequenceNumber_ = randomSeqStart();
    timestamp_ = randomTsStart();
    pendingFrames_ = 0;
    armMarker_ = true; // mark the first emitted packet
    ready_ = (destPort_ != 0);
    error_ = ready_ ? Error::None : Error::NotReady;
    return ready_;
}

void RtpSender::end()
{
    pendingFrames_ = 0;
    ready_ = false;
    error_ = Error::NotReady;
}

bool RtpSender::setPayloadType(uint8_t pt, uint32_t clockRate)
{
    if (pt > 0x7F || clockRate == 0)
    {
        error_ = Error::UnsupportedFormat;
        return false;
    }
    payloadType_ = pt;
    clockRate_ = clockRate;
    isL16_ = (pt == static_cast<uint8_t>(pcmflowudp::RtpPayloadType::L16Mono) ||
              pt == static_cast<uint8_t>(pcmflowudp::RtpPayloadType::L16Stereo));
    pendingFrames_ = 0;
    // L16 path computes samples-per-packet from clock+channels in setFormat();
    // for non-L16, samplesPerPacket_ is unused.
    if (!isL16_)
        samplesPerPacket_ = 0;
    return true;
}

bool RtpSender::setFormat(const PCMFormat &fmt)
{
    if (!fmt.isValid() || fmt.bitsPerSample != 16)
    {
        error_ = Error::UnsupportedFormat;
        return false;
    }
    const uint8_t pt =
        (fmt.channels == 2)
            ? static_cast<uint8_t>(pcmflowudp::RtpPayloadType::L16Stereo)
            : static_cast<uint8_t>(pcmflowudp::RtpPayloadType::L16Mono);
    if (!setPayloadType(pt, fmt.sampleRate))
        return false;
    format_ = fmt;
    isL16_ = true;
    samplesPerPacket_ = computeSamplesPerPacket(fmt.sampleRate, fmt.channels);
    pendingFrames_ = 0;
    return true;
}

size_t RtpSender::writeFrames(const void *in, size_t frameCount)
{
    if (!ready_)
    {
        error_ = Error::NotReady;
        return 0;
    }
    if (!isL16_ || samplesPerPacket_ == 0 || format_.bytesPerFrame() == 0)
    {
        error_ = Error::WrongPath;
        return 0;
    }
    if (in == nullptr || frameCount == 0)
        return 0;

    const size_t bytesPerFrame = format_.bytesPerFrame();
    const uint8_t *src = static_cast<const uint8_t *>(in);
    size_t consumed = 0;
    while (consumed < frameCount)
    {
        const size_t roomFrames =
            static_cast<size_t>(samplesPerPacket_) - pendingFrames_;
        const size_t batch = ((frameCount - consumed) < roomFrames)
                                 ? (frameCount - consumed)
                                 : roomFrames;
        memcpy(packet_ + pcmflowudp::kRtpHeaderBytes +
                   pendingFrames_ * bytesPerFrame,
               src + consumed * bytesPerFrame,
               batch * bytesPerFrame);
        pendingFrames_ += batch;
        consumed += batch;
        if (pendingFrames_ >= samplesPerPacket_)
            emitL16Packet();
    }
    return consumed;
}

bool RtpSender::flush()
{
    if (!ready_)
    {
        error_ = Error::NotReady;
        return false;
    }
    if (pendingFrames_ == 0)
        return true;
    const Error pre = error_;
    emitL16Packet();
    return error_ == pre;
}

void RtpSender::emitL16Packet()
{
    if (pendingFrames_ == 0)
        return;

    // Convert pending samples to network byte order in place.
    int16_t *p = reinterpret_cast<int16_t *>(packet_ + pcmflowudp::kRtpHeaderBytes);
    const size_t totalSamples =
        pendingFrames_ * static_cast<size_t>(format_.channels);
    for (size_t i = 0; i < totalSamples; ++i)
    {
        const uint16_t v = static_cast<uint16_t>(p[i]);
        const uint8_t hi = static_cast<uint8_t>((v >> 8) & 0xFF);
        const uint8_t lo = static_cast<uint8_t>(v & 0xFF);
        uint8_t *b = reinterpret_cast<uint8_t *>(&p[i]);
        b[0] = hi;
        b[1] = lo;
    }

    pcmflowudp::RtpHeader h{};
    h.marker = armMarker_;
    h.payloadType = payloadType_;
    h.sequenceNumber = sequenceNumber_;
    h.timestamp = timestamp_;
    h.ssrc = ssrc_;
    pcmflowudp::encodeRtpHeader(h, packet_);

    const size_t payloadBytes = totalSamples * 2u;
    const bool ok = sendPacket(pcmflowudp::kRtpHeaderBytes + payloadBytes);

    if (ok)
    {
        ++sequenceNumber_;
        timestamp_ += static_cast<uint32_t>(pendingFrames_);
        armMarker_ = false;
    }
    pendingFrames_ = 0;
}

bool RtpSender::writeEncoded(const uint8_t *bytes, size_t count)
{
    if (!ready_)
    {
        error_ = Error::NotReady;
        return false;
    }
    if (payloadType_ == 0xFF)
    {
        error_ = Error::WrongPath;
        return false;
    }
    if (bytes == nullptr || count == 0 || count > pcmflowudp::kRtpMaxPayloadBytes)
    {
        error_ = Error::UnsupportedFormat;
        return false;
    }

    pcmflowudp::RtpHeader h{};
    h.marker = armMarker_;
    h.payloadType = payloadType_;
    h.sequenceNumber = sequenceNumber_;
    h.timestamp = timestamp_;
    h.ssrc = ssrc_;
    pcmflowudp::encodeRtpHeader(h, packet_);
    memcpy(packet_ + pcmflowudp::kRtpHeaderBytes, bytes, count);

    if (!sendPacket(pcmflowudp::kRtpHeaderBytes + count))
        return false;

    ++sequenceNumber_;
    timestamp_ += tsIncrement_;
    armMarker_ = false;
    return true;
}

bool RtpSender::sendPacket(size_t totalBytes)
{
    if (udp_.beginPacket(dest_, destPort_) != 1)
    {
        error_ = Error::SendFailed;
        return false;
    }
    udp_.write(packet_, totalBytes);
    if (udp_.endPacket() != 1)
    {
        error_ = Error::SendFailed;
        return false;
    }
    return true;
}
