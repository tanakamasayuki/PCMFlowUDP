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

// Service-packet notification handed to the user-supplied callback. The
// pointers (`payload`, `udp`) are only valid for the duration of the
// callback — copy out anything the caller needs to retain.
struct VbanServicePacket
{
    pcmflowudp::VbanServiceHeader header;
    const uint8_t *payload = nullptr;
    size_t payloadBytes = 0;
    IPAddress fromIp;
    uint16_t fromPort = 0;
    // The receiver's UDP instance. Use beginPacket(fromIp, fromPort) /
    // write / endPacket to send a reply on the same socket.
    UDP *udp = nullptr;
};

using VbanServiceCallback = void (*)(const VbanServicePacket &pkt, void *userData);

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

    // Non-blocking pump. Returns true if at least one VBAN packet
    // (audio or service) was successfully processed by this call.
    // Audio packets feed the internal queue; service packets invoke
    // the user-supplied service callback (if set).
    bool poll();

    // Register a callback for VBAN Service-sub-protocol packets. The
    // callback fires inside poll() for any well-formed Service packet
    // (after VBAN signature + sub-protocol validation, before any
    // service-function-specific decoding — that is up to the callback).
    // Pass nullptr to unregister.
    void setServiceCallback(VbanServiceCallback cb, void *userData = nullptr)
    {
        serviceCb_ = cb;
        serviceUser_ = userData;
    }

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

    VbanServiceCallback serviceCb_ = nullptr;
    void *serviceUser_ = nullptr;

    // Decoded-PCM ring. Sized to hold ~2 packets at typical stereo
    // PCM16 / 256-sample/packet load (1024 byte payload each).
    static constexpr size_t kQueueBytes = 2048;
    uint8_t queue_[kQueueBytes] = {};
    size_t queueHead_ = 0;  // next byte to read
    size_t queueTail_ = 0;  // next byte to write
    size_t queueCount_ = 0; // bytes currently buffered
};

#endif // PCMFLOWUDP_VBANRECEIVER_H
