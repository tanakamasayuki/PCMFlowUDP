#include "RawUdpSink.h"

#include <string.h>

// PCMFlowUDP :: RawUdpSink implementation.
//
// Bytes from write() accumulate in the caller-supplied (or default)
// packet buffer; flush() emits one UDP datagram with the accumulated
// payload and resets the buffer. A short write() return indicates the
// buffer filled before all caller bytes were accepted; the caller is
// expected to flush() and re-issue the remainder.

bool RawUdpSink::begin(IPAddress destIp, uint16_t destPort)
{
    dest_ = destIp;
    destPort_ = destPort;
    if (buf_ == nullptr)
    {
        buf_ = defaultBuf_;
        cap_ = kDefaultCapacity;
    }
    used_ = 0;
    ready_ = (destPort_ != 0);
    error_ = ready_ ? Error::None : Error::NotReady;
    return ready_;
}

void RawUdpSink::end()
{
    used_ = 0;
    ready_ = false;
    error_ = Error::NotReady;
}

void RawUdpSink::setPacketBuffer(uint8_t *buf, size_t capacity)
{
    buf_ = buf;
    cap_ = capacity;
    used_ = 0;
}

size_t RawUdpSink::write(const void *src, size_t count)
{
    if (!ready_)
    {
        error_ = Error::NotReady;
        return 0;
    }
    if (src == nullptr || count == 0)
        return 0;

    const size_t space = (cap_ > used_) ? (cap_ - used_) : 0;
    const size_t n = (count < space) ? count : space;
    if (n > 0)
        memcpy(buf_ + used_, src, n);
    used_ += n;
    if (n < count)
        error_ = Error::BufferOverflow;
    return n;
}

bool RawUdpSink::flush()
{
    if (!ready_)
    {
        error_ = Error::NotReady;
        return false;
    }
    if (used_ == 0)
        return true; // nothing to send is not an error

    if (udp_.beginPacket(dest_, destPort_) != 1)
    {
        error_ = Error::SendFailed;
        used_ = 0;
        return false;
    }
    udp_.write(buf_, used_);
    const int sent = udp_.endPacket();
    used_ = 0;
    if (sent != 1)
    {
        error_ = Error::SendFailed;
        return false;
    }
    return true;
}
