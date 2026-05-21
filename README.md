# PCMFlowUDP

> 日本語版: [README.ja.md](README.ja.md)

Optional **UDP transport adapter** for [PCMFlow](https://github.com/tanakamasayuki/PCMFlow). Stream PCM audio between Arduino-class devices and PCs over the local network — either as **raw UDP datagrams** or as packets compatible with the **[VBAN protocol](https://vb-audio.com/Voicemeeter/vban.htm)** (audio sub-protocol subset; interoperates with VB-Audio Voicemeeter and VBAN Receptor).

A **clean-room MIT** implementation. No third-party source code is vendored anywhere in this repo; the entire library is single-author MIT.

See [SPEC.md](SPEC.md) for the full specification.

> **Trademark.** "VBAN" is a protocol name owned by VB-Audio Software. PCMFlowUDP is **not affiliated with VB-Audio**; the name is used here in a descriptive sense to identify the wire protocol this library partially implements. Only a **subset** of VBAN is supported (audio + service ping). See SPEC §14 for details.

---

## What's inside

| Class | Direction | Carrier | PCMFlow interface |
|---|---|---|---|
| `RawUdpSink` | bytes → UDP datagram | RAW (caller-defined payload) | `ByteSink` |
| `RawUdpStream` | UDP datagram → bytes | RAW | `ByteStream` |
| `VbanSender` | PCM → UDP datagram | VBAN audio sub-protocol | `PCMSink` |
| `VbanReceiver` | UDP datagram → PCM | VBAN audio sub-protocol | `PCMSource` |

All four classes are constructed around a caller-supplied Arduino `UDP` instance (typically `WiFiUDP`), so PCMFlowUDP pulls in no concrete WiFi / Ethernet stack of its own. The same code runs on ESP32 and on the `lang-ship:host` (1.0.6+) test target.

---

## PCMFlow family

PCMFlowUDP is the **first non-codec member** of the PCMFlow family — a transport, not a codec.

| | role |
|---|---|
| [PCMFlow](https://github.com/tanakamasayuki/PCMFlow) | parent (required); ring buffer / format conversion / WAV / MP3 / FLAC |
| [PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) | narrowband μ-law / A-law codec |
| [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) | wideband HD voice codec |
| [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus) | low-bitrate / fullband codec |
| **PCMFlowUDP** (this lib) | UDP transport (RAW + VBAN subset) |

Compose a codec sibling with PCMFlowUDP to send compressed audio over UDP — for example `G711Encoder` → `RawUdpSink`, or use `VbanSender` with the VBAN μ-law sub-codec for PCMU streams.

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

A fully-wired example sketch lives in [examples/VbanMicToPc/](examples/VbanMicToPc/).

---

## Hardware support

PCMFlowUDP codes against the Arduino `UDP` abstract base class, so any board with an Arduino-compatible UDP implementation works:

- **Primary**: ESP32 family (`WiFiUDP` / `EthernetUDP`), Linux host via [lang-ship:host](https://github.com/tanakamasayuki/lang-ship-arduino-core) 1.0.6+.
- **Best-effort** (no maintainer testing): Arduino UNO R4 WiFi, MKR WiFi 1010 / Nano 33 IoT, Raspberry Pi Pico W, Teensy 4.1 with Ethernet, Portenta H7, any board + W5500/ENC28J60 shield.
- **Out of scope**: 8-bit AVR (insufficient RAM for VBAN frames), BLE-only boards.

**Multicast is not supported** on any platform — the host Arduino core does not provide `beginMulticast()` (Windows-portability constraint). VBAN-style fan-out uses **broadcast** instead, which is the typical VBAN deployment anyway.

---

## License

MIT. See [LICENSE](LICENSE). No third-party vendored code in this repo. See [SPEC §14](SPEC.md#14-license--trademarks) for the trademark notice regarding VBAN / Voicemeeter / VBAN Receptor.
