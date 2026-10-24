# RtpVoipG711

> 日本語版: [README.ja.md](README.ja.md)

Send and decode G.711 packets over RTP.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [RtpVoipG711.ino](RtpVoipG711.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<WiFi.h>`, `<WiFiUdp.h>`, `<PCMFlow.h>`, `<PCMFlowUDP.h>`, `<PCMFlowG711.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

PCMFlowUDP example: RtpVoipG711

Streams microphone audio from an ESP32 to a PC SIP softphone (or
gst-launch / Wireshark) as an RTP/PCMU flow, RFC 3550 + RFC 3551 §4.5.14.

  8 kHz mono mic → G711Encoder (mu-law) → RtpSender::writeEncoded()

The receive direction (PC → ESP32) mirrors:

  RtpReceiver::readEncoded() → G711Decoder → PCMFlow → I2S DAC

Both directions share one WiFiUDP instance bound to the local port,
matching the typical RTP socket convention (one symmetric port per
flow).

Verify on the PC side with either:

  gst-launch-1.0 -v udpsrc port=5004 caps="application/x-rtp,media=audio,\
    encoding-name=PCMU,clock-rate=8000,payload=0" ! rtppcmudepay ! \
    mulawdec ! audioconvert ! autoaudiosink

  ...or any standards-compliant SIP softphone with the SDP offer set
  to PCMU on the configured port.

TODO: fill in the I2S microphone and speaker code for the target
board (M5 mic / INMP441, MAX98357A speaker, etc.). The structure
below shows the PCMFlowUDP-facing wiring; the codec wiring requires
the sibling library PCMFlowG711 to be installed alongside.

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `g_udp.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `g_audio.setOutputFormat(...)` — Choose a format supported by the next stage, not just the source format.
3. `g_audio.setInputSource(...)` — Attach a source that remains alive while the pipeline reads from it.
4. `g_enc.encode(...)` — Encode into sufficiently large storage and validate the returned result before consuming output.
5. `g_receiver.poll(...)` — Service pending input regularly; interpret its result before accessing new data.
6. `g_dec.decode(...)` — Validate incoming encoded length and format before using the decoded output.
7. `g_audio.pump()` — Advance decoding/conversion, then consume only the available returned PCM frames.

Review these application constants before adapting the sketch: `kRemoteIp`, `kRemotePort`, `kLocalPort`, `kSampleRate`, `kSamplesPerPacket`, `kSsrc`. Match pins, channels and capacities to your application.

## API usage and precautions

Set local Wi-Fi credentials and the receiving peer address/port. Start the UDP socket before protocol send/receive and poll incoming packets regularly. Raw UDP, VBAN and RTP have different framing and format rules; both sides must agree.

UDP does not guarantee delivery, ordering or retry, so a network send result is not a playback acknowledgement. The microphone/speaker portions in these examples include placeholders: silence transmission or received byte counts alone do not prove live audio. Replace those sections with hardware I/O while retaining sample/byte units and pacing.

RTP timestamps follow the payload clock; do not assume every codec uses the PCM sampling clock (G.722 is a notable case). Manage packet size, partial output and buffering explicitly.

The example sends zero-filled microphone placeholders and pumps the receive decoder, but it does not consume decoded PCM into a configured speaker. Add both input capture and a PCM drain/output stage. An RTP socket alone does not perform SIP call setup or SDP negotiation.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
PCMFlowUDP RtpVoipG711 starting
Local IP:
Ready
```

## Verify and troubleshoot

Send and decode G.711 packets over RTP and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
