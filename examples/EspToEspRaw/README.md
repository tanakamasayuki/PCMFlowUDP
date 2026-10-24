# EspToEspRaw

> 日本語版: [README.ja.md](README.ja.md)

Exchange raw PCM byte chunks over UDP.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [EspToEspRaw.ino](EspToEspRaw.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<WiFi.h>`, `<WiFiUdp.h>`, `<PCMFlow.h>`, `<PCMFlowUDP.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

PCMFlowUDP example: EspToEspRaw

Two ESP32 boards on the same LAN exchange raw PCM16 audio over plain
UDP. No protocol header on the wire — each UDP datagram is one chunk
of PCM samples in host byte order. Lowest overhead, useful for
proprietary device-to-device links where you control both ends.

The same firmware runs on both boards. Each board:

  mic   → RawUdpSink     (peer IP, fixed port)
  peer  → RawUdpStream   (this board's port) → PCMFlow → DAC

To use, flash this on two boards and set the peer IPs in
kPeerIp / kLocalPort accordingly (or use the broadcast IP
255.255.255.255 to skip configuration entirely).

TODO: fill in the I2S microphone / speaker bring-up for the target
hardware (M5Stack Core2, INMP441 + MAX98357A, etc.).

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `g_udp.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `g_sink.flush(...)` — Finish or transfer pending buffered work according to this component; do not assume it is instantaneous.
3. `g_stream.poll(...)` — Service pending input regularly; interpret its result before accessing new data.

Review these application constants before adapting the sketch: `kPeerIp`, `kPort`, `kSampleRate`, `kFramesPerChunk`. Match pins, channels and capacities to your application.

## API usage and precautions

Set local Wi-Fi credentials and the receiving peer address/port. Start the UDP socket before protocol send/receive and poll incoming packets regularly. Raw UDP, VBAN and RTP have different framing and format rules; both sides must agree.

UDP does not guarantee delivery, ordering or retry, so a network send result is not a playback acknowledgement. The microphone/speaker portions in these examples include placeholders: silence transmission or received byte counts alone do not prove live audio. Replace those sections with hardware I/O while retaining sample/byte units and pacing.

RTP timestamps follow the payload clock; do not assume every codec uses the PCM sampling clock (G.722 is a notable case). Manage packet size, partial output and buffering explicitly.

The sample microphone buffer contains silence and the receive bytes are not played. The loop has no real-time capture pacing while placeholders remain; add timing or a blocking capture cadence before treating it as a live audio sender.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
PCMFlowUDP EspToEspRaw starting
Local IP:
Ready
```

## Verify and troubleshoot

Exchange raw PCM byte chunks over UDP and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
