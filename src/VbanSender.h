#ifndef PCMFLOWUDP_VBANSENDER_H
#define PCMFLOWUDP_VBANSENDER_H

#include <Arduino.h>
#include <IPAddress.h>
#include <Udp.h>
#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"
#include "PCMSink.h"
#include "VbanProtocol.h"

// PCMFlowUDP :: VbanSender
//
// Sends PCM frames as VBAN-audio-sub-protocol packets. Implements
// PCMFlow's PCMSink interface so it can be driven from the standard
// pipeline (e.g. PCMFlow::writeFrames -> VbanSender).
//
// Currently supported sub-codecs: PCM16 (primary); MuLaw / ALaw
// payloads are deferred. See SPEC §6 for the coverage matrix.
//
// This sender implements a subset of VBAN sufficient to be received by
// VB-Audio Voicemeeter / VBAN Receptor. PCMFlowUDP is not affiliated
// with VB-Audio Software; see SPEC §14 for the trademark notice.

class VbanSender : public PCMSink
{
public:
    enum class Error : uint8_t
    {
        None,
        NotReady,
        UnsupportedFormat,
        SendFailed,
    };

    explicit VbanSender(UDP &udp) : udp_(udp) {}
    ~VbanSender() override = default;

    VbanSender(const VbanSender &) = delete;
    VbanSender &operator=(const VbanSender &) = delete;

    // Open the sender.
    //   destIp / destPort : where to send (broadcast IP is allowed).
    //   streamName        : up to 16 ASCII bytes (longer is truncated).
    bool begin(IPAddress destIp, uint16_t destPort, const char *streamName);
    void end();

    // PCM layout. Must be called before any writeFrames(). Sample rate
    // must be one of the VBAN-table values; channel count 1..256; bits
    // 16 (other depths are deferred).
    bool setFormat(const PCMFormat &format);

    const char *streamName() const { return streamName_; }
    void setStreamName(const char *name);

    // Force the currently-pending samples out as one VBAN packet, even
    // if it is shorter than the full samplesPerPacket() worth. Useful
    // for end-of-stream and for callers whose chunk size doesn't align
    // with the internal packet size. Returns false on UDP send failure
    // or NotReady; an empty pending buffer is a no-op success.
    bool flush();

    // Number of PCM frames packed per VBAN audio packet at the current
    // format. Set automatically by setFormat() to min(256, 1408 / bytesPerFrame).
    uint16_t samplesPerPacket() const { return samplesPerPacket_; }

    // PCMSink interface -------------------------------------------------
    const PCMFormat &format() const override { return format_; }
    size_t writeFrames(const void *in, size_t frameCount) override;
    bool isReady() const override { return ready_; }

    Error lastError() const { return error_; }

private:
    UDP &udp_;
    IPAddress dest_;
    uint16_t destPort_ = 0;
    char streamName_[pcmflowudp::kVbanStreamNameBytes + 1] = {0};
    PCMFormat format_{};
    uint8_t sampleRateIndex_ = 0;
    uint16_t samplesPerPacket_ = 0;
    size_t pendingFrames_ = 0;
    uint32_t frameCounter_ = 0;
    bool ready_ = false;
    Error error_ = Error::NotReady;

    // Packet scratch (header + payload).
    uint8_t packet_[pcmflowudp::kVbanMaxPacketBytes] = {};

    // Pack pendingFrames_ frames currently sitting at packet_[28..] into a
    // VBAN audio packet and send it.
    void emitPacket();
};

#endif // PCMFLOWUDP_VBANSENDER_H
