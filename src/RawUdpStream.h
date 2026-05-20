#ifndef PCMFLOWUDP_RAWUDPSTREAM_H
#define PCMFLOWUDP_RAWUDPSTREAM_H

#include <Arduino.h>
#include <IPAddress.h>
#include <Udp.h>
#include <stdint.h>
#include <stddef.h>

#include "ByteStream.h"

// PCMFlowUDP :: RawUdpStream
//
// Receives UDP datagrams on a local port and exposes them through
// PCMFlow's ByteStream interface. One datagram at a time: read() pulls
// from the current packet; when the packet is exhausted, the next call
// to poll() (or read() with an internal poll) advances to the next
// queued datagram.
//
// The stream has no defined EOF (UDP is connectionless), so isEof()
// always returns false. Callers detect "no data right now" by a
// short / zero return from read().

class RawUdpStream : public ByteStream
{
public:
    enum class Error : uint8_t
    {
        None,
        NotReady,
        BindFailed,
    };

    // Caller owns `udp` and keeps it alive for the lifetime of this
    // object.
    explicit RawUdpStream(UDP &udp) : udp_(udp) {}
    ~RawUdpStream() override = default;

    RawUdpStream(const RawUdpStream &) = delete;
    RawUdpStream &operator=(const RawUdpStream &) = delete;

    // Bind to the given local UDP port for receive.
    bool begin(uint16_t localPort);
    void end();

    // Non-blocking pump. Returns true if a new datagram was queued by
    // this call. Safe to call as often as the caller likes.
    bool poll();

    // Sender of the most recently dequeued packet.
    IPAddress remoteIP() const { return remoteIp_; }
    uint16_t remotePort() const { return remotePort_; }

    // ByteStream interface ----------------------------------------------
    size_t read(void *dst, size_t count) override;
    bool isEof() const override { return false; }

    Error lastError() const { return error_; }

private:
    UDP &udp_;
    uint16_t localPort_ = 0;
    bool ready_ = false;
    Error error_ = Error::NotReady;

    IPAddress remoteIp_{};
    uint16_t remotePort_ = 0;
};

#endif // PCMFLOWUDP_RAWUDPSTREAM_H
