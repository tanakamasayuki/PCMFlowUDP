#ifndef PCMFLOWUDP_H
#define PCMFLOWUDP_H

// Umbrella header for PCMFlowUDP.
//
// Including this single header gives the user the public API surface of
// the UDP transport adapter for PCMFlow:
//
//   - RawUdpSink   : ByteSink   -> UDP datagrams (caller-defined payload)
//   - RawUdpStream : UDP recv   -> ByteStream    (caller-defined payload)
//   - VbanSender   : PCMSink    -> UDP datagrams (VBAN audio sub-protocol)
//   - VbanReceiver : UDP recv   -> PCMSource     (VBAN audio sub-protocol)
//
// The two Vban* classes implement a subset of the VBAN wire protocol
// sufficient to interoperate with VB-Audio Voicemeeter and VBAN
// Receptor. PCMFlowUDP is not affiliated with VB-Audio Software; see
// SPEC.md "License & trademarks".
//
// All four classes are constructed around an externally-supplied
// Arduino `UDP` instance (typically `WiFiUDP`), so PCMFlowUDP does not
// pull in any concrete WiFi / Ethernet stack of its own.

#include "pcmflowudp_version.h"

#include "VbanProtocol.h"
#include "RtpProtocol.h"
#include "RawUdpSink.h"
#include "RawUdpStream.h"
#include "VbanSender.h"
#include "VbanReceiver.h"

#endif // PCMFLOWUDP_H
