#ifndef PCMFLOWUDP_RAWUDPSINK_H
#define PCMFLOWUDP_RAWUDPSINK_H

#include <Arduino.h>
#include <IPAddress.h>
#include <Udp.h>
#include <stdint.h>
#include <stddef.h>

#include "ByteSink.h"

// PCMFlowUDP :: RawUdpSink
//
// Sends caller-supplied bytes as UDP datagrams to a fixed destination.
// Implements PCMFlow's ByteSink interface so it can be composed with a
// codec sibling's output (e.g. G711Encoder bytes -> RawUdpSink).
//
// Datagram framing is set by flush(): write() accumulates into an
// internal buffer; flush() sends one UDP datagram with the accumulated
// payload and clears the buffer. Callers choose the chunk size by
// deciding when to call flush().

class RawUdpSink : public ByteSink
{
public:
    enum class Error : uint8_t
    {
        None,
        NotReady,
        SendFailed,
        BufferOverflow,
    };

    // Caller owns `udp` and keeps it alive for the lifetime of this
    // object. The destination is fixed at begin() time; if the caller
    // needs to change peers, call begin() again.
    explicit RawUdpSink(UDP &udp) : udp_(udp) {}
    ~RawUdpSink() override = default;

    RawUdpSink(const RawUdpSink &) = delete;
    RawUdpSink &operator=(const RawUdpSink &) = delete;

    // Open the sink. Does not bind a local port itself; the caller must
    // have called `udp.begin(port)` (any port, 0 is fine for ephemeral)
    // on the underlying UDP object beforehand. This matches Arduino's
    // documented WiFiUDP usage and is required by the host Arduino
    // core's WiFiUDP implementation, which will refuse to sendto()
    // until a local socket exists.
    bool begin(IPAddress destIp, uint16_t destPort);
    void end();

    // Provide a caller-owned scratch buffer used to assemble each
    // datagram. If unset, a default 1500-byte internal buffer is used.
    void setPacketBuffer(uint8_t *buf, size_t capacity);

    // ByteSink interface ------------------------------------------------
    size_t write(const void *src, size_t count) override;
    bool flush() override; // emits one UDP datagram with the accumulated bytes

    Error lastError() const { return error_; }

private:
    UDP &udp_;
    IPAddress dest_;
    uint16_t destPort_ = 0;
    uint8_t *buf_ = nullptr;
    size_t cap_ = 0;
    size_t used_ = 0;
    bool ready_ = false;
    Error error_ = Error::NotReady;

    static constexpr size_t kDefaultCapacity = 1500;
    uint8_t defaultBuf_[kDefaultCapacity] = {};
};

#endif // PCMFLOWUDP_RAWUDPSINK_H
