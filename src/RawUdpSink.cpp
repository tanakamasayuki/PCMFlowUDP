#include "RawUdpSink.h"

// PCMFlowUDP :: RawUdpSink implementation (skeleton).
//
// TODO: complete implementation against the SPEC. The current bodies
// are placeholders so the library compiles while the public surface is
// being settled.

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

size_t RawUdpSink::write(const void * /*src*/, size_t /*count*/)
{
    // TODO: append to buf_ up to cap_; auto-flush on overflow if desired.
    return 0;
}

bool RawUdpSink::flush()
{
    // TODO: udp_.beginPacket(dest_, destPort_); write(buf_, used_); endPacket();
    return false;
}
