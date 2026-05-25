#ifndef PCMFLOWUDP_RTPSENDER_H
#define PCMFLOWUDP_RTPSENDER_H

#include <Arduino.h>
#include <IPAddress.h>
#include <Udp.h>
#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"
#include "PCMSink.h"
#include "RtpProtocol.h"

// PCMFlowUDP :: RtpSender
//
// Emits RTP (RFC 3550) packets over UDP. Two complementary input paths:
//
//   * writeFrames() — implements PCMSink. Valid for L16 payload types
//     (PT 10 / 11). The sender packs PCM16 in network byte order,
//     advances the timestamp by the number of samples sent, and emits
//     packets at the natural packet size for the configured format.
//
//   * writeEncoded() — caller passes already-codec-encoded payload
//     bytes. One call == one RTP packet. The sender advances the
//     timestamp by the value set with setTimestampIncrement() (default
//     0, meaning the caller manages timestamps via setTimestamp()).
//
// Sequence number, SSRC, and (initial) timestamp are managed
// automatically. The marker bit is set on the first packet emitted
// after begin() and may be re-armed for any subsequent packet via
// setMarker(true).

class RtpSender : public PCMSink
{
public:
    enum class Error : uint8_t
    {
        None,
        NotReady,
        UnsupportedFormat,
        WrongPath,        // writeFrames called on non-L16 PT, or writeEncoded with no PT set
        SendFailed,
    };

    explicit RtpSender(UDP &udp) : udp_(udp) {}
    ~RtpSender() override = default;

    RtpSender(const RtpSender &) = delete;
    RtpSender &operator=(const RtpSender &) = delete;

    // Open the sender. SSRC is randomized if `ssrc == 0`.
    bool begin(IPAddress destIp, uint16_t destPort, uint32_t ssrc = 0);
    void end();

    // Configure the payload type explicitly. `clockRate` is the RTP
    // timestamp clock (e.g. 8000 for PCMU/PCMA/G722, 48000 for Opus).
    // For L16 use setFormat() instead — it derives PT and clock from
    // the PCMFormat.
    bool setPayloadType(uint8_t pt, uint32_t clockRate);

    // L16-path configuration. channels 1 → PT 11, channels 2 → PT 10;
    // bitsPerSample must be 16; sampleRate sets the RTP clock.
    bool setFormat(const PCMFormat &format);

    // Per-packet timestamp increment for writeEncoded(). Default 0 means
    // the caller manages the timestamp via setTimestamp(). Typical
    // values: 160 for 20 ms of G.711 at 8 kHz, 960 for 20 ms of Opus
    // at 48 kHz.
    void setTimestampIncrement(uint32_t inc) { tsIncrement_ = inc; }
    uint32_t timestampIncrement() const { return tsIncrement_; }

    // Arm the marker bit for the *next* packet emitted (cleared after).
    void setMarker(bool m) { armMarker_ = m; }

    // Override the SSRC after begin().
    void setSsrc(uint32_t ssrc) { ssrc_ = ssrc; }
    uint32_t ssrc() const { return ssrc_; }

    // Override the next outgoing timestamp.
    void setTimestamp(uint32_t ts) { timestamp_ = ts; }
    uint32_t timestamp() const { return timestamp_; }

    uint16_t sequenceNumber() const { return sequenceNumber_; }
    uint8_t payloadType() const { return payloadType_; }
    uint32_t clockRate() const { return clockRate_; }
    uint16_t samplesPerPacket() const { return samplesPerPacket_; }

    // Emit any pending partial L16 packet immediately.
    bool flush();

    // Send one RTP packet whose payload is the supplied bytes verbatim.
    // Caller supplies already-encoded codec bytes (PCMU / PCMA / G722 /
    // Opus / ...). Returns true on send success.
    bool writeEncoded(const uint8_t *bytes, size_t count);

    // PCMSink interface (L16 path) -------------------------------------
    const PCMFormat &format() const override { return format_; }
    size_t writeFrames(const void *in, size_t frameCount) override;
    bool isReady() const override { return ready_; }

    Error lastError() const { return error_; }

private:
    UDP &udp_;
    IPAddress dest_;
    uint16_t destPort_ = 0;

    PCMFormat format_{};
    uint8_t payloadType_ = 0xFF; // 0xFF = unset
    uint32_t clockRate_ = 0;
    uint16_t samplesPerPacket_ = 0;
    bool isL16_ = false;

    uint32_t ssrc_ = 0;
    uint16_t sequenceNumber_ = 0;
    uint32_t timestamp_ = 0;
    uint32_t tsIncrement_ = 0;
    bool armMarker_ = false;

    bool ready_ = false;
    Error error_ = Error::NotReady;

    // Packet scratch (header + payload).
    uint8_t packet_[pcmflowudp::kRtpMaxPacketBytes] = {};

    // Pending L16 samples waiting to fill a packet (host byte order
    // until emit, then converted to network byte order in-place).
    size_t pendingFrames_ = 0;

    void emitL16Packet();
    bool sendPacket(size_t headerPlusPayloadBytes);
};

#endif // PCMFLOWUDP_RTPSENDER_H
