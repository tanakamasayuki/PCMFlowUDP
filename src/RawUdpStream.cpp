#include "RawUdpStream.h"

// PCMFlowUDP :: RawUdpStream implementation (skeleton).
//
// TODO: complete implementation against the SPEC. The current bodies
// are placeholders so the library compiles while the public surface is
// being settled.

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
    // TODO: call udp_.parsePacket(); if > 0, capture remoteIP/Port and
    // make the bytes available to read().
    return false;
}

size_t RawUdpStream::read(void * /*dst*/, size_t /*count*/)
{
    // TODO: pull from the currently-held datagram; auto-poll() at the
    // start if no datagram is held.
    return 0;
}
