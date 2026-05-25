#include "VbanSender.h"

#include <string.h>

// PCMFlowUDP :: VbanSender implementation.
//
// Accepts interleaved PCM16 frames via writeFrames(); accumulates into
// the packet scratch buffer (header + payload area); when one packet's
// worth of samples is queued, encodes the VBAN audio header and emits
// one UDP datagram.
//
// v0.1.x supports PCM16 only (mono / stereo). Sample rate must be one
// of the VBAN-table values. Other depths / codecs raise
// Error::UnsupportedFormat at setFormat() time.

namespace
{
    // VBAN encodes nbSample as N-1 in 1 byte: max 256 samples per
    // channel per packet. The packed payload must also fit the 1408-byte
    // VBAN max. We pick the largest power-of-two-ish samples-per-packet
    // that satisfies both, given the channel count.
    uint16_t computeSamplesPerPacket(uint8_t channels)
    {
        const size_t bytesPerFrame = static_cast<size_t>(channels) * 2u; // PCM16
        const size_t fitsInPayload = pcmflowudp::kVbanMaxPayloadBytes / bytesPerFrame;
        const size_t capped = (fitsInPayload < 256u) ? fitsInPayload : 256u;
        return static_cast<uint16_t>(capped);
    }
} // namespace

bool VbanSender::begin(IPAddress destIp, uint16_t destPort, const char *streamName)
{
    dest_ = destIp;
    destPort_ = destPort;
    setStreamName(streamName);
    frameCounter_ = 0;
    pendingFrames_ = 0;
    ready_ = (destPort_ != 0);
    error_ = ready_ ? Error::None : Error::NotReady;
    return ready_;
}

void VbanSender::end()
{
    pendingFrames_ = 0;
    ready_ = false;
    error_ = Error::NotReady;
}

bool VbanSender::setFormat(const PCMFormat &fmt)
{
    if (!fmt.isValid() || fmt.bitsPerSample != 16)
    {
        error_ = Error::UnsupportedFormat;
        return false;
    }
    const uint8_t idx = pcmflowudp::vbanSampleRateIndex(fmt.sampleRate);
    if (idx == pcmflowudp::kVbanSampleRateInvalid)
    {
        error_ = Error::UnsupportedFormat;
        return false;
    }
    format_ = fmt;
    sampleRateIndex_ = idx;
    samplesPerPacket_ = computeSamplesPerPacket(fmt.channels);
    pendingFrames_ = 0;
    return true;
}

void VbanSender::setStreamName(const char *name)
{
    memset(streamName_, 0, sizeof(streamName_));
    if (name == nullptr)
        return;
    const size_t n = strnlen(name, pcmflowudp::kVbanStreamNameBytes);
    memcpy(streamName_, name, n);
}

size_t VbanSender::writeFrames(const void *in, size_t frameCount)
{
    if (!ready_)
    {
        error_ = Error::NotReady;
        return 0;
    }
    if (samplesPerPacket_ == 0 || format_.bytesPerFrame() == 0)
    {
        error_ = Error::UnsupportedFormat;
        return 0;
    }
    if (in == nullptr || frameCount == 0)
        return 0;

    const size_t bytesPerFrame = format_.bytesPerFrame();
    const uint8_t *src = static_cast<const uint8_t *>(in);
    size_t consumed = 0;

    while (consumed < frameCount)
    {
        const size_t roomFrames = samplesPerPacket_ - pendingFrames_;
        const size_t batchFrames = ((frameCount - consumed) < roomFrames)
                                       ? (frameCount - consumed)
                                       : roomFrames;
        memcpy(packet_ + pcmflowudp::kVbanHeaderBytes +
                   pendingFrames_ * bytesPerFrame,
               src + consumed * bytesPerFrame,
               batchFrames * bytesPerFrame);
        pendingFrames_ += batchFrames;
        consumed += batchFrames;
        if (pendingFrames_ >= samplesPerPacket_)
            emitPacket();
    }
    return consumed;
}

bool VbanSender::flush()
{
    if (!ready_)
    {
        error_ = Error::NotReady;
        return false;
    }
    if (pendingFrames_ == 0)
        return true;
    const Error pre = error_;
    emitPacket();
    return error_ == pre; // unchanged means emit didn't set SendFailed
}

void VbanSender::emitPacket()
{
    if (pendingFrames_ == 0)
        return;

    pcmflowudp::VbanAudioHeader h{};
    h.sampleRateIndex = sampleRateIndex_;
    h.numSamples = static_cast<uint16_t>(pendingFrames_);
    h.numChannels = format_.channels;
    h.subCodec = pcmflowudp::VbanSubCodec::PCM16;
    memcpy(h.streamName, streamName_, sizeof(streamName_));
    h.frameCounter = frameCounter_++;
    pcmflowudp::encodeAudioHeader(h, packet_);

    const size_t payloadBytes = pendingFrames_ * format_.bytesPerFrame();
    const size_t totalBytes = pcmflowudp::kVbanHeaderBytes + payloadBytes;

    if (udp_.beginPacket(dest_, destPort_) != 1)
    {
        error_ = Error::SendFailed;
    }
    else
    {
        udp_.write(packet_, totalBytes);
        if (udp_.endPacket() != 1)
            error_ = Error::SendFailed;
    }
    pendingFrames_ = 0;
}
