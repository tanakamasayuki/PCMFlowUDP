#include "RawUdpStream.h"

// PCMFlowUDP :: RawUdpStream implementation.
//
// One datagram is held at a time inside the underlying `UDP` instance
// (Arduino's WiFiUDP / EthernetUDP all keep the most recent packet in
// an internal buffer between parsePacket() and the next read()).
// poll() advances to the next datagram and captures the sender
// address; read() copies from the currently-held packet and
// auto-polls if the previous packet has been fully consumed.

bool RawUdpStream::begin(uint16_t localPort)
{
    localPort_ = localPort;
    if (udp_.begin(localPort_) != 1)
    {
        error_ = Error::BindFailed;
        ready_ = false;
        return false;
    }
    ready_ = true;
    error_ = Error::None;
    return true;
}

void RawUdpStream::end()
{
    udp_.stop();
    ready_ = false;
    error_ = Error::NotReady;
}

bool RawUdpStream::poll()
{
    if (!ready_)
        return false;
    const int n = udp_.parsePacket();
    if (n <= 0)
        return false;
    remoteIp_ = udp_.remoteIP();
    remotePort_ = udp_.remotePort();
    return true;
}

size_t RawUdpStream::read(void *dst, size_t count)
{
    if (!ready_ || dst == nullptr || count == 0)
        return 0;

    if (udp_.available() <= 0)
    {
        if (!poll())
            return 0;
    }
    const int got = udp_.read(static_cast<unsigned char *>(dst), count);
    return (got > 0) ? static_cast<size_t>(got) : 0;
}
