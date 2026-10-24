# VbanMicToPc

> 日本語版: [README.ja.md](README.ja.md)

Send PCM packets using the VBAN format.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [VbanMicToPc.ino](VbanMicToPc.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<WiFi.h>`, `<WiFiUdp.h>`, `<PCMFlow.h>`, `<PCMFlowUDP.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

PCMFlowUDP example: VbanMicToPc

Headline use case. An ESP32 board with a microphone captures 16 kHz
mono audio and streams it to VB-Audio Voicemeeter / VBAN Receptor
running on a PC on the same LAN. The PC plays it as if it were a
local audio source.

PCMFlowUDP implements a subset of the VBAN wire protocol sufficient
for this use case; see ../../SPEC.md §6 for the coverage matrix and
§14 for the trademark notice. PCMFlowUDP is not affiliated with
VB-Audio Software.

TODO: fill in the I2S microphone code for the target board (M5 mic,
INMP441, etc.). The structure below shows the PCMFlowUDP-facing
wiring; the mic source is intentionally left as a placeholder so
this sketch is portable to whatever capture hardware is on hand.

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `g_udp.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `g_sender.writeFrames(...)` — Check the number accepted and retain any unconsumed tail; a frame is not a byte.

Review these application constants before adapting the sketch: `kDestIp`, `kDestPort`, `kSampleRate`, `kFramesPerChunk`. Match pins, channels and capacities to your application.

## API usage and precautions

Set local Wi-Fi credentials and the receiving peer address/port. Start the UDP socket before protocol send/receive and poll incoming packets regularly. Raw UDP, VBAN and RTP have different framing and format rules; both sides must agree.

UDP does not guarantee delivery, ordering or retry, so a network send result is not a playback acknowledgement. The microphone/speaker portions in these examples include placeholders: silence transmission or received byte counts alone do not prove live audio. Replace those sections with hardware I/O while retaining sample/byte units and pacing.

RTP timestamps follow the payload clock; do not assume every codec uses the PCM sampling clock (G.722 is a notable case). Manage packet size, partial output and buffering explicitly.

Set the destination IP, port and stream name to match the receiving PC. The microphone section is a placeholder; successful datagram transmission can still be only silence.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
PCMFlowUDP VbanMicToPc starting
Local IP:
Streaming
```

## Verify and troubleshoot

Send PCM packets using the VBAN format and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
