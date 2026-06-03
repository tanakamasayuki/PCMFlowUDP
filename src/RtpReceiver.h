#ifndef PCMFLOWUDP_RTPRECEIVER_H
#define PCMFLOWUDP_RTPRECEIVER_H

#include <Arduino.h>
#include <IPAddress.h>
#include <Udp.h>
#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"
#include "PCMSource.h"
#include "RtpProtocol.h"

// PCMFlowUDP :: RtpReceiver
//
// Receives RTP packets on a local UDP port. Two complementary output
// paths, chosen by the inbound packet's payload type:
//
//   * For L16 PTs (10 / 11), and an optional caller-declared dynamic L16
//     PT, the payload is converted from network to host byte order and
//     queued into an internal PCM ring buffer. readFrames() pops samples
//     via PCMSource.
//
//   * For non-L16 PTs, the most recent packet's payload bytes are
//     kept in a single-packet hold and exposed via readEncoded().
//     Calling poll() again before readEncoded() drops the previous
//     held payload (drop-newest at the per-packet level is left to
//     the kernel via SO_RCVBUF; the application sees the most recent
//     packet that arrived between polls).
//
// Last-packet metadata (payload type, sequence, timestamp, SSRC,
// marker) is always available via the accessor methods.

class RtpReceiver : public PCMSource
{
public:
    enum class Error : uint8_t
    {
        None,
        NotReady,
        BindFailed,
        BadHeader,
    };

    explicit RtpReceiver(UDP &udp) : udp_(udp) {}
    ~RtpReceiver() override = default;

    RtpReceiver(const RtpReceiver &) = delete;
    RtpReceiver &operator=(const RtpReceiver &) = delete;

    bool begin(uint16_t localPort);
    void end();

    // Optionally pre-declare the expected L16 format so format() returns
    // it before any packet has arrived. If the first incoming L16 packet
    // disagrees, the receiver re-locks to the wire's channel count
    // (PT 10 = stereo, 11 = mono) and uses `sampleRate` from setFormat()
    // since the wire does not carry it explicitly.
    bool setFormat(const PCMFormat &format);

    // Treat a dynamic RTP payload type as L16 PCM with the given channel
    // count. This is useful for tools such as GStreamer, which commonly
    // send 16 kHz L16 as dynamic PT 96 instead of static PT 11.
    bool setDynamicL16PayloadType(uint8_t payloadType, uint8_t channels);

    // Non-blocking pump. Returns true if exactly one RTP packet was
    // successfully read and dispatched (L16 to the PCM ring, others to
    // the single-packet encoded hold).
    bool poll();

    // Last-packet metadata.
    uint8_t payloadType() const { return lastPt_; }
    uint16_t sequenceNumber() const { return lastSeq_; }
    uint32_t timestamp() const { return lastTs_; }
    uint32_t ssrc() const { return lastSsrc_; }
    bool marker() const { return lastMarker_; }
    bool isPcm() const
    {
        return lastPt_ == static_cast<uint8_t>(pcmflowudp::RtpPayloadType::L16Mono) ||
               lastPt_ == static_cast<uint8_t>(pcmflowudp::RtpPayloadType::L16Stereo) ||
               (dynamicL16Channels_ != 0 && lastPt_ == dynamicL16Pt_);
    }

    // Encoded path: copy the most recent non-L16 packet's payload into
    // `bytes`. Returns the number of bytes written (0 if no fresh
    // encoded packet is held or `maxBytes` is too small to hold the
    // whole payload — the hold is not partially consumed).
    size_t readEncoded(uint8_t *bytes, size_t maxBytes);

    // True if a fresh encoded packet is held and not yet read.
    bool hasEncoded() const { return encodedReady_; }

    // PCMSource interface (L16 path) -----------------------------------
    const PCMFormat &format() const override { return format_; }
    size_t readFrames(void *out, size_t frameCount) override;
    bool isEof() const override { return false; }
    bool isReady() const override { return ready_; }

    Error lastError() const { return error_; }

private:
    UDP &udp_;
    uint16_t localPort_ = 0;
    bool ready_ = false;
    Error error_ = Error::NotReady;

    PCMFormat format_{};
    uint8_t dynamicL16Pt_ = 0xFF;
    uint8_t dynamicL16Channels_ = 0;

    // Last-packet metadata.
    uint8_t lastPt_ = 0xFF;
    uint16_t lastSeq_ = 0;
    uint32_t lastTs_ = 0;
    uint32_t lastSsrc_ = 0;
    bool lastMarker_ = false;

    // L16 ring (PCM in host byte order).
    static constexpr size_t kQueueBytes = 2048;
    uint8_t queue_[kQueueBytes] = {};
    size_t queueHead_ = 0;
    size_t queueTail_ = 0;
    size_t queueCount_ = 0;

    // Encoded-path single-packet hold.
    uint8_t encoded_[pcmflowudp::kRtpMaxPayloadBytes] = {};
    size_t encodedBytes_ = 0;
    bool encodedReady_ = false;
};

#endif // PCMFLOWUDP_RTPRECEIVER_H
