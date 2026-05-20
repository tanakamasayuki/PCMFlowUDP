#include "VbanSender.h"

#include <string.h>

// PCMFlowUDP :: VbanSender implementation (skeleton).
//
// TODO: complete VBAN header encoding and packet emission against the
// SPEC. The current bodies are placeholders so the library compiles
// while the public surface is being settled.

bool VbanSender::begin(IPAddress destIp, uint16_t destPort, const char *streamName)
{
    dest_ = destIp;
    destPort_ = destPort;
    setStreamName(streamName);
    frameCounter_ = 0;
    ready_ = (destPort_ != 0);
    error_ = ready_ ? Error::None : Error::NotReady;
    return ready_;
}

void VbanSender::end()
{
    ready_ = false;
    error_ = Error::NotReady;
}

bool VbanSender::setFormat(const PCMFormat & /*fmt*/)
{
    // TODO: validate against VBAN sample-rate table and supported bit depths.
    return false;
}

void VbanSender::setStreamName(const char *name)
{
    memset(streamName_, 0, sizeof(streamName_));
    if (name == nullptr)
        return;
    const size_t n = strnlen(name, pcmflowudp::kVbanStreamNameBytes);
    memcpy(streamName_, name, n);
}

size_t VbanSender::writeFrames(const void * /*in*/, size_t /*frameCount*/)
{
    // TODO: pack samples into one or more VBAN audio packets, increment
    // frameCounter_, and udp_.sendto().
    return 0;
}
