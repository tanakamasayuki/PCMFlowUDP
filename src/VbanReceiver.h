#ifndef PCMFLOWUDP_VBANRECEIVER_H
#define PCMFLOWUDP_VBANRECEIVER_H

#include <Arduino.h>
#include <IPAddress.h>
#include <Udp.h>
#include <stdint.h>
#include <stddef.h>

#include "PCMFormat.h"
#include "PCMSource.h"
#include "VbanProtocol.h"

// PCMFlowUDP :: VbanReceiver
//
// Receives VBAN audio-sub-protocol packets on a local UDP port and
// exposes the decoded samples through PCMFlow's PCMSource interface.
// A minimal Service-ping responder is included so that VBAN-aware
// peers can discover this device; other Service sub-types and
// MIDI / Serial sub-protocols are silently ignored.
//
// Stream-name filtering is optional: if a name is given to begin(),
// packets carrying a different name are dropped. Pass nullptr or an
// empty string to accept any stream name.
//
// This receiver implements a subset of VBAN; see SPEC §6 for the
// coverage matrix and §14 for the trademark notice.

class VbanReceiver : public PCMSource
{
public:
    enum class Error : uint8_t
    {
        None,
        NotReady,
        BindFailed,
        FormatUnsupported, // arriving header declares a sub-codec we cannot decode
    };

    explicit VbanReceiver(UDP &udp) : udp_(udp) {}
    ~VbanReceiver() override = default;

    VbanReceiver(const VbanReceiver &) = delete;
    VbanReceiver &operator=(const VbanReceiver &) = delete;

    // Bind to `localPort` and optionally filter by stream name (nullptr
    // = accept any).
    bool begin(uint16_t localPort, const char *streamName = nullptr);
    void end();

    // Non-blocking pump. Returns true if at least one VBAN audio packet
    // was decoded into the internal queue by this call. Service-ping
    // responses are also sent from inside this call.
    bool poll();

    // Most recently received header summary (valid after poll() returns
    // true at least once).
    const char *currentStreamName() const { return currentStream_; }
    uint32_t lastFrameCounter() const { return lastFrameCounter_; }

    // PCMSource interface -----------------------------------------------
    const PCMFormat &format() const override { return format_; }
    size_t readFrames(void *out, size_t frameCount) override;
    bool isEof() const override { return false; }
    bool isReady() const override { return ready_; }

    Error lastError() const { return error_; }

private:
    UDP &udp_;
    uint16_t localPort_ = 0;
    char filterName_[pcmflowudp::kVbanStreamNameBytes + 1] = {0};
    bool filterEnabled_ = false;
    bool ready_ = false;
    Error error_ = Error::NotReady;

    PCMFormat format_{};
    char currentStream_[pcmflowudp::kVbanStreamNameBytes + 1] = {0};
    uint32_t lastFrameCounter_ = 0;

    // Decoded-PCM ring (skeleton: sized to a generous single packet).
    static constexpr size_t kQueueBytes = 2048;
    uint8_t queue_[kQueueBytes] = {};
    size_t queueHead_ = 0;
    size_t queueTail_ = 0;
};

#endif // PCMFLOWUDP_VBANRECEIVER_H
