# PCMFlowUDP

> 日本語版: [README.ja.md](README.ja.md)

**UDP transport adapter** for [PCMFlow](https://github.com/tanakamasayuki/PCMFlow). Stream PCM audio between Arduino-class devices and PCs over the local network — as **raw UDP datagrams**, as packets compatible with the **[VBAN protocol](https://vb-audio.com/Voicemeeter/vban.htm)** (PCM subset; interoperates with VB-Audio Voicemeeter and VBAN Receptor), or as **[RTP](https://datatracker.ietf.org/doc/html/rfc3550)** packets with standard codec payload types (PCMU / PCMA / G.722 / L16 / Opus).

A **clean-room MIT** implementation. No third-party source code is vendored anywhere in this repo; the entire library is single-author MIT.

See [SPEC.md](SPEC.md) for the full specification.

> **Trademark.** "VBAN" is a protocol name owned by VB-Audio Software. PCMFlowUDP is **not affiliated with VB-Audio**; the name is used here in a descriptive sense to identify the wire protocol this library partially implements. Only the PCM subset of VBAN is supported. See [SPEC §13](SPEC.md#13-license--trademarks) for details.

---

## What's inside

| Class | Direction | Carrier | PCMFlow interface |
|---|---|---|---|
| `RawUdpSink` | bytes → UDP datagram | RAW (caller-defined payload) | `ByteSink` |
| `RawUdpStream` | UDP datagram → bytes | RAW | `ByteStream` |
| `VbanSender` | PCM → UDP datagram | VBAN audio (PCM only) | `PCMSink` |
| `VbanReceiver` | UDP datagram → PCM | VBAN audio (PCM only) | `PCMSource` |
| `RtpSender` | PCM or encoded bytes → UDP datagram | RTP (RFC 3550) | `PCMSink` (L16) + `writeEncoded()` |
| `RtpReceiver` | UDP datagram → PCM or encoded bytes | RTP | `PCMSource` (L16) + `readEncoded()` |

All six classes are constructed around a caller-supplied Arduino `UDP` instance (typically `WiFiUDP`), so PCMFlowUDP pulls in no concrete WiFi / Ethernet stack of its own. The same code runs on ESP32 and on the `lang-ship:host` test target.

---

## Picking a carrier

Each transport has a deliberately narrow scope. Pick by what you need to interoperate with:

| Carrier | Use it when | PCM | G.711 / G.722 / Opus |
|---|---|---|---|
| **RAW** | You're defining your own wire format on top of UDP (device-to-device, telemetry, low-level tests) | ✓ bytes | ✓ bytes (pair with a codec sibling) |
| **VBAN** | You want VB-Audio Voicemeeter / VBAN Receptor on a PC to see your stream | ✓ | not in scope — RTP is the codec-aware path |
| **RTP** | You want VoIP / WebRTC / standard streaming interop with codec payload types | ✓ (L16) | ✓ (PCMU / PCMA / G722 / Opus) |

RAW is intentionally codec-agnostic; for standardized interop, use **VBAN (PCM)** or **RTP (codec-aware)**. See [SPEC §3.4](SPEC.md#34-transport-selection-codec--carrier) for the full discussion.

---

## PCMFlow family

PCMFlowUDP is the **transport-only member** of the PCMFlow family.

| | role |
|---|---|
| [PCMFlow](https://github.com/tanakamasayuki/PCMFlow) | parent (required); ring buffer / format conversion / WAV / MP3 / FLAC |
| [PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) | narrowband μ-law / A-law codec |
| [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) | wideband HD voice codec |
| [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus) | low-bitrate / fullband codec |
| **PCMFlowUDP** (this lib) | UDP transport (RAW + VBAN + RTP) |
| [PCMFlowDevice](https://github.com/tanakamasayuki/PCMFlowDevice) | board-specific audio I/O helpers |

Compose a codec sibling with PCMFlowUDP to send compressed audio over UDP. For example, `G711Encoder` bytes → `RtpSender::writeEncoded()` (with payload type set to PCMU) produces a standards-compliant VoIP stream that any SIP softphone can play.

For RTP/L16 receive playback, `RtpReceiver` includes a small internal PCM ring and can use caller-supplied storage via `setPcmBuffer()`. Standard profiles are exposed as `lowLatencyPcmBuffer()` (40/20 ms), `hardwareSpeakerPcmBuffer()` (40/40 ms), and `stableSpeakerPcmBuffer()` (80/40 ms). Device-specific speaker queue and buffer-lifetime management, such as M5Unified `Speaker.playRaw()` playback, is handled by PCMFlowDevice.

---

## Headline use case — ESP32 mic → PC (VBAN Receptor)

ESP32 captures from a microphone and streams to VBAN Receptor running on a Windows PC; the PC plays it like a local audio source.

```cpp
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlow.h>
#include <PCMFlowUDP.h>

WiFiUDP udp;
VbanSender sender(udp);

void setup() {
    WiFi.begin("ssid", "passphrase");
    while (WiFi.status() != WL_CONNECTED) delay(100);

    // Arduino UDP convention: call udp.begin() once before any send/recv.
    // Required on EthernetUDP, WiFiNINA, WiFiS3, and the lang-ship:host
    // core; ESP32's WiFiUDP is lenient but the explicit call is portable.
    // Use port 0 for an ephemeral local binding when sending only.
    udp.begin(0);

    sender.begin(IPAddress(192,168,1,100), 6980, "ESP32-Mic");
    sender.setFormat({16000, 1, 16});  // 16 kHz mono 16-bit
}

void loop() {
    int16_t mic[256];
    // ...fill `mic` from your I2S source...
    sender.writeFrames(mic, 256);
}
```

A fully-wired example sketch lives in [examples/VbanMicToPc/](examples/VbanMicToPc/). An RTP/PCMU SIP-softphone example is in [examples/RtpVoipG711/](examples/RtpVoipG711/).

---

## Hardware support

PCMFlowUDP codes against the Arduino `UDP` abstract base class, so any board with an Arduino-compatible UDP implementation works:

- **Primary**: ESP32 family (`WiFiUDP` / `EthernetUDP`), Linux host via [lang-ship:host](https://github.com/tanakamasayuki/lang-ship-arduino-core).
- **Best-effort** (no maintainer testing): Arduino UNO R4 WiFi, MKR WiFi 1010 / Nano 33 IoT, Raspberry Pi Pico W, Teensy 4.1 with Ethernet, Portenta H7, any board + W5500/ENC28J60 shield.
- **Out of scope**: 8-bit AVR (insufficient RAM for VBAN / RTP frames), BLE-only boards.

**Multicast is not supported** on any platform — the host Arduino core does not provide `beginMulticast()` (Windows-portability constraint). VBAN-style fan-out uses **broadcast** instead, which is the typical deployment anyway.

---

## License

MIT. See [LICENSE](LICENSE). No third-party vendored code in this repo. See [SPEC §13](SPEC.md#13-license--trademarks) for the trademark notice regarding VBAN / Voicemeeter / VBAN Receptor.
