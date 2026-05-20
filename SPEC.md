# PCMFlowUDP Specification

> 日本語版: [SPEC.ja.md](SPEC.ja.md)

## 1. Scope

**PCMFlowUDP** is an optional **UDP transport adapter** for [PCMFlow](https://github.com/tanakamasayuki/PCMFlow), aimed at streaming PCM (or codec-compressed) audio between Arduino-class devices and PCs over the local network.

It is the **first non-codec member** of the PCMFlow family. Unlike the codec siblings ([PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) / [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) / [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus)), PCMFlowUDP carries audio over the wire; it does not transform sample values.

Two carrier modes are supported:

- **RAW UDP** — payload is whatever the caller hands in (post-codec bytes, or raw PCM). No framing on the wire beyond UDP datagram boundaries. The simplest mode and the lowest-overhead bridge between PCMFlow and the network.
- **VBAN-interoperable subset** — implements a **subset of the [VBAN protocol](https://vb-audio.com/Voicemeeter/vban.htm)** sufficient to interoperate with VB-Audio's *VBAN Receptor*, *Voicemeeter*, and other VBAN-aware tools. Concretely: the **Audio sub-protocol** (PCM payloads) and a minimal **Service sub-protocol** (ping / identification). This is **not a full VBAN implementation** — MIDI, Serial, and most Service sub-types are out of scope (see §6 for the complete matrix). "VBAN" is used here in a descriptive sense to name the wire protocol; see §14 for the trademark notice.

Responsibility:

- **Send** audio frames over UDP, either as RAW datagrams or as well-formed VBAN packets.
- **Receive** audio frames from UDP, validating VBAN headers when in VBAN mode.
- **Bridge** to PCMFlow's `PCMSource` / `PCMSink` / `ByteStream` / `ByteSink` interfaces.

PCMFlowUDP is **transport-only**. It does not encode or decode audio; combine it with PCMFlow's built-in WAV/MP3/FLAC or a codec sibling for compressed transports.

## 2. Non-goals

- **Codec functionality** — owned by PCMFlow itself or codec siblings (G.711 / G.722 / Opus).
- **TCP / WebSocket / RTP** — out of scope. UDP only. RTP-over-UDP may be added later if a concrete user request appears (see §"Deferred features").
- **TLS / DTLS** — out of scope.
- **IPv6** — out of scope; Arduino's `IPAddress` is IPv4 only.
- **Jitter buffering / packet-loss concealment** — caller's responsibility (or PCMFlow's ring buffer absorbs it).
- **Audio device I/O** — owned by PCMFlow.
- **Concrete UDP stack** — PCMFlowUDP codes against the Arduino `UDP` abstract base class. `WiFiUDP`, `EthernetUDP`, etc. are injected by the caller, not pulled in by this library.

## 3. Primary use cases

### 3.1 ESP32 ↔ PC via VBAN

The motivating use case. An ESP32 with a microphone sends a 16 kHz mono audio stream to a Windows PC running VBAN Receptor; the PC plays it back as if it were a local audio source. Optionally, the reverse direction (PC mic → ESP32 speaker) runs over the same library.

### 3.2 ESP32 ↔ ESP32 over RAW UDP

Two ESP32 boards on the same LAN exchange audio over plain UDP. Lower overhead than VBAN (no 28-byte header), useful for high-channel-count or high-sample-rate streams where header overhead matters.

### 3.3 Codec + transport composition

PCMFlowG711's `G711Encoder` output → `RawUdpSink` for compressed VoIP-style packets. Or VBAN-compatible `VbanSink` with sub-codec field set to μ-law for VBAN PCMU streams.

## 4. Hardware support

PCMFlowUDP codes against the Arduino `UDP` abstract base class, so any board that ships an Arduino-compatible `UDP` implementation works:

| Class | Examples |
|---|---|
| **Primary** | ESP32 family (`WiFiUDP` / `EthernetUDP`), host (`lang-ship:host` 1.0.6+ provides `WiFiUDP` / `IPAddress` — see §10) |
| **Best-effort** | Arduino UNO R4 WiFi (`WiFiS3::WiFiUDP`), MKR WiFi 1010 / Nano 33 IoT (`WiFiNINA::WiFiUDP`), Raspberry Pi Pico W (arduino-pico), Teensy 4.1 (`NativeEthernet` / `QNEthernet`), Portenta H7, any board + W5500/ENC28J60 shield (`EthernetUDP`) |
| **Out of scope** | 8-bit AVR boards (insufficient RAM for VBAN frames), nRF52840 BLE-only boards (no IP stack) |

"Best-effort" means PCMFlowUDP is expected to compile and run, but the maintainer does not test on these targets. PRs welcome.

**Multicast is not supported.** The host platform's `UDP::beginMulticast()` is intentionally a no-op for Windows-portability reasons, so PCMFlowUDP does not depend on multicast on any platform. Discovery and VBAN-style fan-out use **broadcast** (`IPAddress(255,255,255,255)`) instead.

## 5. Public API

> **Status: draft.** Class names and method signatures are tentative; finalized in the headers under [src/](src/) once implementation begins.

Four classes, organized as two carrier types × two directions:

| | RAW mode | VBAN mode |
|---|---|---|
| Send (out to UDP) | `RawUdpSink` (implements `ByteSink`) | `VbanSender` (implements `PCMSink`) |
| Recv (in from UDP) | `RawUdpStream` (implements `ByteStream`) | `VbanReceiver` (implements `PCMSource`) |

### 5.1 Common construction pattern

All four classes take a `UDP&` reference (the user's `WiFiUDP` instance) and configuration:

```cpp
WiFiUDP wifi;
VbanSender sender(wifi);
sender.begin(IPAddress(192,168,1,100), 6980, "Stream1");
sender.setFormat({16000, 1, 16});  // 16 kHz mono 16-bit
```

Receivers bind a local port and (optionally) filter by stream name:

```cpp
WiFiUDP wifi;
VbanReceiver recv(wifi);
recv.begin(6980, "Stream1");
```

### 5.2 RAW mode

- `RawUdpSink::write(const void *src, size_t count)` — accumulates bytes; flushes a UDP datagram on `flush()` or when an internal buffer fills (caller sets the chunking).
- `RawUdpStream::read(void *dst, size_t count)` — pulls bytes from the most-recently-received datagram; advances to the next datagram on exhaustion.

Detailed signatures TBD in implementation.

### 5.3 VBAN mode

VBAN packets are 28-byte header + up to 1408 byte payload. Header carries: sample rate index, channel count, sub-protocol (Audio / Service), sub-codec (PCM / μ-law / A-law / Opus / ...), frame counter, 16-byte stream name.

- `VbanSender::writeFrames(const int16_t *pcm, size_t frames)` — packs samples into VBAN Audio packets and sends.
- `VbanReceiver::readFrames(int16_t *pcm, size_t maxFrames)` — pulls received audio and exposes it via `PCMSource`.

Service sub-protocol (ping / identification) is handled internally: `VbanReceiver` responds to incoming ping packets so that VBAN-aware peers can discover this device.

Detailed signatures TBD in implementation. Open design questions tracked in §"Open questions".

### 5.4 PCMFlow pipeline integration

- `VbanReceiver` is a `PCMSource` → plug into `PCMFlow::setInputSource()` for direct playback.
- `VbanSender` is a `PCMSink` → drives from `PCMFlow::writeFrames()` or a mic recording task.
- `RawUdpStream` / `RawUdpSink` are `ByteStream` / `ByteSink` → compose with codec siblings (e.g. G.711 decode of incoming RAW datagrams).

## 6. VBAN protocol scope

PCMFlowUDP implements a **subset of VBAN** — only the portions needed to send and receive audio with VBAN-aware tools. It is **not a full VBAN stack**. Specifically:

| VBAN sub-protocol | Status |
|---|---|
| Audio (0x00) | **Implemented** — PCM 16-bit LE primary; μ-law / A-law as sub-codec selectable. Other PCM bit-depths (8 / 24 / 32 / float) are deferred. |
| Serial (0x20) | Not implemented |
| MIDI (0x40) | Not implemented |
| Service (0x60) | **Implemented (ping + identification only)** — enough to appear in VBAN Receptor's source list and respond to discovery. Other Service sub-types are deferred. |

Sample rates: VBAN's canonical 21-entry table is supported on TX; RX accepts any rate the header declares but the audio path is configured at `begin()` time, so mismatches are surfaced via an error code rather than runtime resampling. (Use PCMFlow's resampler upstream if needed.)

## 7. Memory & footprint targets

> **Status: targets, not measurements.** Confirmed once implementation lands.

| Item | Target |
|------|--------|
| Flash (full library, RAW + VBAN, send + recv) | ≤ 12 KB |
| Flash (RAW only, one direction) | ≤ 3 KB |
| RAM, per `VbanSender` / `VbanReceiver` instance | ≤ 2 KB (including one 1500-byte packet buffer) |
| RAM, per `RawUdpSink` / `RawUdpStream` instance | ≤ 256 B + caller-supplied buffer |
| Per-call scratch | none (no dynamic allocation) |

VBAN's 28-byte header is small enough that header parsing is done in place. Packet buffers are caller-owned where possible.

## 8. Repository layout

```
PCMFlowUDP/
├─ README.md / README.ja.md
├─ SPEC.md   / SPEC.ja.md
├─ CHANGELOG.md
├─ LICENSE
├─ library.properties        # depends=PCMFlow
├─ library.json
├─ keywords.txt
├─ src/
│  ├─ PCMFlowUDP.h           # umbrella header
│  ├─ RawUdpSink.h/.cpp
│  ├─ RawUdpStream.h/.cpp
│  ├─ VbanSender.h/.cpp
│  ├─ VbanReceiver.h/.cpp
│  ├─ VbanProtocol.h         # header constants, sub-protocol / sub-codec enums
│  └─ pcmflowudp_version.h
├─ examples/
│  ├─ VbanMicToPc/           # ESP32 mic → VBAN Receptor on Windows
│  └─ EspToEspRaw/           # ESP32 ↔ ESP32 RAW UDP loopback
├─ tests/
│  ├─ README.md / README.ja.md
│  ├─ conftest.py
│  ├─ pyproject.toml
│  ├─ smoke/
│  ├─ raw_loopback/          # host-only, UDP sender → receiver same process
│  ├─ vban_header/           # byte-exact header encoding for known inputs
│  ├─ vban_loopback/         # round-trip PCM via VBAN encoder/decoder
│  └─ vban_interop/          # captured VBAN Receptor traces → decoder
├─ doc/
│  └─ sibling_library_brief.md
├─ tools/
│  └─ bump_version.py
└─ .github/workflows/
   └─ release.yml
```

## 9. Vendored upstream

**None for the protocol itself.** VBAN's wire format is publicly documented by VB-Audio; only the binary header layout is normative. PCMFlowUDP implements VBAN packet construction and parsing from the published specification, without reusing third-party code.

**No host-side shim is vendored** either: the `lang-ship:host` Arduino core (1.0.6+) provides a Berkeley-sockets-backed `WiFiUDP` / `IPAddress` implementation that is API-compatible with the ESP32 build. PCMFlowUDP simply `#include <WiFiUdp.h>` on both targets.

License hygiene: the shipped library (`src/`) is **MIT, single-author, no third-party attribution required**.

## 10. Testing

Same conventions as parent PCMFlow:

- pytest-embedded + Arduino CLI backend.
- Two profiles: `lang-ship:host` (logic, large fixtures) and `esp32:esp32:esp32` (real hardware verification).
- Per-feature test directory with `<feature>.ino`, `sketch.yaml`, `test_<feature>.py`.
- `EXPECT_TRUE / EXPECT_EQ / EXPECT_NEAR` macros, `TEST done N/M` Serial protocol.

PCMFlowUDP-specific test design:

| Test dir | Subject | Strategy |
|---|---|---|
| `vban_header/` | byte-exact VBAN header encoding | feed known config; assert each header byte against a reference table |
| `raw_loopback/` | RAW sink → stream round-trip | host: two `UDP` instances on `127.0.0.1`; assert byte equality |
| `vban_loopback/` | PCM → VBAN encode → VBAN decode → PCM | host: same as above; assert near-exact PCM (RAW PCM payload is bit-exact, μ-law / A-law payload uses ±quantization tolerance) |
| `vban_interop/` | parse real captures from VBAN Receptor / Voicemeeter | static `.bin` fixtures; assert audio samples and metadata |

**The `lang-ship:host` Arduino core (1.0.6+) ships a Berkeley-sockets-backed `WiFiUDP` / `IPAddress`** that matches the ESP32 API surface, so host tests use the same `#include <WiFiUdp.h>` as device sketches — no shim or vendoring needed inside this repo. Tests pin `platform: lang-ship:host (1.0.6)` or newer in `sketch.yaml`.

## 11. Versioning

SemVer (`major.minor.patch`) maintained in `library.properties`, `library.json`, and `src/pcmflowudp_version.h`. Independent of the PCMFlow version.

## 12. Deferred features

Captured here so they aren't lost; not in v0.1.x:

- **RTP-over-UDP** (RFC 3550). Standard for VoIP / WebRTC interop. Defer until a concrete user request appears.
- **VBAN MIDI / Serial / additional Service subtypes**. Not music-related core.
- **VBAN with Opus / AAC sub-codecs**. Requires a codec sibling at the same time; revisit after PCMFlowOpus stabilizes.
- **Multicast.** Will not be added: the host Arduino core does not support `beginMulticast()` (Windows-portability constraint), and PCMFlowUDP keeps a single behavior across platforms. Broadcast covers VBAN's needs.
- **Jitter buffer / PLC inside the receiver**. PCMFlow's ring buffer absorbs ~10s of ms of jitter; if a user reports specific dropouts, revisit.

## 13. Open questions

To be resolved during implementation, surfaced here so they aren't forgotten:

- Buffer ownership for `VbanSender` — caller-supplied packet buffer vs. one owned by the class. Trade-off: RAM control vs. ergonomics.
- Whether `VbanReceiver` should expose the VBAN frame counter / stream name to the caller (for de-duplication and multi-stream apps).
- Error reporting style — return code enum vs. `lastError()` accessor.
- How to surface "packet arrived from unexpected stream name" (drop silently, count, or callback).

## 14. License & trademarks

PCMFlowUDP: **MIT** ([LICENSE](LICENSE)). No vendored third-party code anywhere in this repo (`src/` is hand-written from the VBAN public specification; host-side UDP is provided externally by the `lang-ship:host` Arduino core, see §9 / §10).

**Trademark notice.** VBAN is a protocol developed and published by **VB-Audio Software**. The name "VBAN" — including its appearance in class names such as `VbanSender` / `VbanReceiver` — is used here in a **descriptive / nominative sense** to identify the wire protocol this library partially implements. PCMFlowUDP is **not affiliated with, endorsed by, or sponsored by VB-Audio Software**, and only a subset of the VBAN specification is supported (§6). Other product and company names mentioned (Voicemeeter, VBAN Receptor) are trademarks of their respective owners.
