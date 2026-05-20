#include "VbanReceiver.h"

#include <string.h>

// PCMFlowUDP :: VbanReceiver implementation (skeleton).
//
// TODO: complete VBAN header validation, audio-payload decoding, and
// Service-ping responder against the SPEC. The current bodies are
// placeholders so the library compiles while the public surface is
// being settled.

bool VbanReceiver::begin(uint16_t localPort, const char *streamName)
{
    localPort_ = localPort;
    memset(filterName_, 0, sizeof(filterName_));
    filterEnabled_ = (streamName != nullptr && streamName[0] != '\0');
    if (filterEnabled_)
    {
        const size_t n = strnlen(streamName, pcmflowudp::kVbanStreamNameBytes);
        memcpy(filterName_, streamName, n);
    }
    if (udp_.begin(localPort_) != 1)
    {
        error_ = Error::BindFailed;
        ready_ = false;
        return false;
    }
    queueHead_ = queueTail_ = 0;
    lastFrameCounter_ = 0;
    ready_ = true;
    error_ = Error::None;
    return true;
}

void VbanReceiver::end()
{
    udp_.stop();
    queueHead_ = queueTail_ = 0;
    ready_ = false;
    error_ = Error::NotReady;
}

bool VbanReceiver::poll()
{
    // TODO: udp_.parsePacket(); validate signature/sub-protocol; if
    // Audio, decode payload into queue_; if Service ping, respond.
    return false;
}

size_t VbanReceiver::readFrames(void * /*out*/, size_t /*frameCount*/)
{
    // TODO: pull from queue_ honoring format_.bytesPerFrame().
    return 0;
}
