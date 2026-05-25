# PCMFlowUDP Specification

> 日本語版: [SPEC.ja.md](SPEC.ja.md)

## 1. Scope

**PCMFlowUDP** is the **UDP transport adapter** for [PCMFlow](https://github.com/tanakamasayuki/PCMFlow). It streams PCM (or codec-compressed) audio between Arduino-class devices and PCs over the local network.

It is the **transport-only member** of the PCMFlow family. Unlike the codec siblings ([PCMFlowG711](https://github.com/tanakamasayuki/PCMFlowG711) / [PCMFlowG722](https://github.com/tanakamasayuki/PCMFlowG722) / [PCMFlowOpus](https://github.com/tanakamasayuki/PCMFlowOpus)), PCMFlowUDP carries audio over the wire; it does not transform sample values.

Three carrier modes are supported, each with a deliberately narrow scope (see §3.4 for the selection matrix):

- **RAW UDP** — payload is whatever the caller hands in. No framing on the wire beyond UDP datagram boundaries. The escape hatch for device-to-device proprietary protocols, telemetry, and low-level tests.
- **VBAN (PCM-only subset)** — implements the PCM portion of the [VBAN protocol](https://vb-audio.com/Voicemeeter/vban.htm), sufficient to interoperate with VB-Audio's *VBAN Receptor*, *Voicemeeter*, and other VBAN-aware tools. Concretely: the **Audio sub-protocol** with PCM payloads, and **Service sub-protocol** header detection with a user-supplied callback. **Not a full VBAN implementation** — see §6 for the coverage matrix. "VBAN" is used here in a descriptive sense to name the wire protocol; see §14 for the trademark notice.
- **RTP (RFC 3550)** — implements the subset of RTP needed to interoperate with VoIP / WebRTC / standard streaming tooling, with **codec-aware payload types** (PCMU / PCMA / G722 / L16 / Opus). PCMFlowUDP packetizes; codec siblings provide the bytes.

Responsibility:

- **Send** audio frames over UDP as RAW datagrams, well-formed VBAN packets, or standards-compliant RTP packets.
- **Receive** UDP datagrams and dispatch by carrier — RAW bytes, VBAN-validated PCM, RTP payload-typed bytes or PCM.
- **Bridge** to PCMFlow's `PCMSource` / `PCMSink` / `ByteStream` / `ByteSink` interfaces.

PCMFlowUDP is **transport-only**. It does not encode or decode audio; combine it with PCMFlow's built-in WAV/MP3/FLAC, or with a codec sibling for compressed payloads.

## 2. Non-goals

- **Codec functionality** — owned by PCMFlow itself or codec siblings (G.711 / G.722 / Opus).
- **TCP / WebSocket** — out of scope. UDP only.
- **TLS / DTLS / SRTP** — out of scope.
- **IPv6** — out of scope; Arduino's `IPAddress` is IPv4 only.
- **Multicast** — the host Arduino core does not provide `beginMulticast()` (Windows-portability constraint), and PCMFlowUDP keeps a single behavior across platforms. Broadcast covers all in-scope use cases.
- **Jitter buffering / packet-loss concealment** — caller's responsibility (or PCMFlow's ring buffer absorbs it).
- **Built-in VBAN Service responder payload** — the Service sub-protocol's *header* is parsed and dispatched to a user callback, but the *reply payload* (device identification structure) is not built into the library. VB-Audio does not publish a normative payload spec, and reverse-engineering from GPL sources would conflict with this library's clean-room MIT policy. Users implement responders out-of-tree against their own packet captures.
- **VBAN MIDI / Serial / additional Service subtypes**. Not music-related core.
- **VBAN sub-codecs other than PCM** (μ-law / A-law / Opus carried inside VBAN). Standardized codec-over-UDP interop is the role of RTP (§3.4); supporting the same codecs through both VBAN sub-codecs and RTP payload types would duplicate the matrix.
- **Audio device I/O** — owned by PCMFlow.
- **Concrete UDP stack** — PCMFlowUDP codes against the Arduino `UDP` abstract base class. `WiFiUDP`, `EthernetUDP`, etc. are injected by the caller, not pulled in by this library.

## 3. Primary use cases

### 3.1 ESP32 ↔ PC via VBAN

The motivating use case. An ESP32 with a microphone sends a 16 kHz mono PCM stream to a Windows PC running VBAN Receptor; the PC plays it back as if it were a local audio source. The reverse direction (PC mic → ESP32 speaker) runs over the same library.

### 3.2 ESP32 ↔ ESP32 over RAW UDP

Two ESP32 boards on the same LAN exchange audio over plain UDP with a caller-defined wire format. Lowest overhead (no protocol header), useful for proprietary device-to-device links and telemetry.

### 3.3 ESP32 ↔ VoIP / WebRTC via RTP

An ESP32 streams G.711 μ-law (PCMU, RTP payload type 0) to a SIP softphone or a `gst-launch-1.0` pipeline on a PC. The codec sibling (`G711Encoder`) produces bytes; `RtpSender` packetizes with the correct payload type, sequence number, timestamp, and SSRC. Symmetric for Opus, G.722, and L16 PCM.

### 3.4 Transport selection (codec × carrier)

Pick by what you need to interoperate with:

| Carrier | Purpose | PCM | G.711 / G.722 / Opus |
|---|---|---|---|
| **RAW** | BYO-protocol escape hatch — caller defines the wire format on top of UDP datagrams | ✓ (bytes) | ✓ (bytes; pair with codec sibling) |
| **VBAN** | Interop with VB-Audio Voicemeeter / VBAN Receptor | ✓ (PCM16) | not in scope — RTP is the codec-aware path |
| **RTP** | Interop with VoIP / WebRTC / standard streaming tooling | ✓ (L16, PT 10/11) | ✓ via standard payload types: PCMU(0), PCMA(8), G722(9), Opus(dynamic per RFC 7587) |

RAW is intentionally codec-agnostic; for standardized interop, **VBAN (PCM) or RTP (codec-aware)** is the right tool.

## 4. Hardware support

PCMFlowUDP codes against the Arduino `UDP` abstract base class, so any board that ships an Arduino-compatible `UDP` implementation works:

| Class | Examples |
|---|---|
| **Primary** | ESP32 family (`WiFiUDP` / `EthernetUDP`), host (`lang-ship:host` provides `WiFiUDP` / `IPAddress` — see §10) |
| **Best-effort** | Arduino UNO R4 WiFi (`WiFiS3::WiFiUDP`), MKR WiFi 1010 / Nano 33 IoT (`WiFiNINA::WiFiUDP`), Raspberry Pi Pico W (arduino-pico), Teensy 4.1 (`NativeEthernet` / `QNEthernet`), Portenta H7, any board + W5500/ENC28J60 shield (`EthernetUDP`) |
| **Out of scope** | 8-bit AVR boards (insufficient RAM for VBAN / RTP frames), nRF52840 BLE-only boards (no IP stack) |

"Best-effort" means PCMFlowUDP is expected to compile and run, but the maintainer does not test on these targets. PRs welcome.

## 5. Public API

Six classes, organized as three carrier types × two directions:

| | RAW | VBAN | RTP |
|---|---|---|---|
| Send | `RawUdpSink` (implements `ByteSink`) | `VbanSender` (implements `PCMSink`) | `RtpSender` (`PCMSink` for L16; `writeEncoded()` for codecs) |
| Recv | `RawUdpStream` (implements `ByteStream`) | `VbanReceiver` (implements `PCMSource`) | `RtpReceiver` (`PCMSource` for L16; `readEncoded()` for codecs) |

### 5.1 Common construction pattern

All six classes take a `UDP&` reference (the user's `WiFiUDP` instance) and configuration:

```cpp
WiFiUDP udp;
udp.begin(0);                              // ephemeral local bind, see §5.5

VbanSender sender(udp);
sender.begin(IPAddress(192,168,1,100), 6980, "Stream1");
sender.setFormat({16000, 1, 16});          // 16 kHz mono 16-bit
```

Receivers bind a local port and (where applicable) filter by stream name:

```cpp
WiFiUDP udp;
VbanReceiver recv(udp);
recv.begin(6980, "Stream1");
```

### 5.2 RAW

- `RawUdpSink::write(const void *src, size_t count)` — accumulates bytes; `flush()` sends one UDP datagram with the accumulated payload (caller decides chunking by deciding when to flush).
- `RawUdpStream::read(void *dst, size_t count)` — pulls bytes from the most-recently-received datagram; advances to the next on exhaustion (auto-polls if no datagram is held).

RAW does not understand PCM; it is byte-passthrough.

### 5.3 VBAN

VBAN packets are 28-byte header + up to 1408 byte payload. PCMFlowUDP handles the **Audio sub-protocol with PCM16 payloads** and the **Service sub-protocol via a user callback** (header detection only; reply payload is the user's concern — see §6, §2).

- `VbanSender::writeFrames(const int16_t *pcm, size_t frames)` — packs PCM16 samples into VBAN Audio packets and sends. Automatic packet sizing based on channel count.
- `VbanSender::flush()` — emits any pending partial packet immediately.
- `VbanReceiver::readFrames(int16_t *pcm, size_t maxFrames)` — pulls received PCM and exposes it via `PCMSource`.
- `VbanReceiver::setServiceCallback(cb, userData)` — registers a callback for Service-sub-protocol packets, invoked from `poll()` with parsed header, raw payload pointer, sender address, and the receiver's `UDP*` (for sending replies).

### 5.4 RTP

RTP packets are 12-byte header + payload. The header carries version, payload type, sequence number, timestamp, and SSRC. PCMFlowUDP handles the static payload types ([RFC 3551](https://datatracker.ietf.org/doc/html/rfc3551)) and Opus via RFC 7587 dynamic PT.

Supported payload types:

| PT | Codec | Clock | Notes |
|---|---|---|---|
| 0 | PCMU (G.711 μ-law) | 8000 Hz | Pair with `G711Encoder` / `G711Decoder` |
| 8 | PCMA (G.711 A-law) | 8000 Hz | Pair with `G711Encoder` / `G711Decoder` |
| 9 | G.722 | 8000 Hz (RTP timestamp quirk) | Pair with `G722Encoder` / `G722Decoder` |
| 10 | L16 stereo | configurable | PCM16 BE, network byte order |
| 11 | L16 mono | configurable | PCM16 BE, network byte order |
| 96..127 | Opus (dynamic) | 48000 Hz typical | Pair with `OpusEncoder` / `OpusDecoder` |

API:

- `RtpSender::setPayloadType(uint8_t pt, uint32_t clockRate)` — configures the codec slot. SSRC defaults to a random value at `begin()` time; override with `setSsrc()`.
- `RtpSender::writeFrames(const int16_t *pcm, size_t frames)` — L16 path: packs PCM16 in network byte order and sends. Valid only for PT 10 / 11.
- `RtpSender::writeEncoded(const uint8_t *bytes, size_t count)` — codec path: one call = one RTP packet with the supplied bytes as payload. Valid for PT 0 / 8 / 9 / dynamic.
- `RtpReceiver::readFrames(int16_t *pcm, size_t maxFrames)` — L16 path: returns PCM16 in host byte order.
- `RtpReceiver::readEncoded(uint8_t *bytes, size_t maxBytes)` — codec path: returns one packet's payload bytes.
- `RtpReceiver::payloadType()` / `sequenceNumber()` / `timestamp()` / `ssrc()` — last-packet metadata for application-side decoding / jitter analysis.

Timestamps and sequence numbers are incremented automatically by `RtpSender` per packet; the caller need not manage them.

### 5.5 Local-port binding contract

All six classes require the caller to have called `udp.begin(port)` (any port; 0 = ephemeral for send-only) before any other operation. This matches the documented Arduino `UDP` contract (`EthernetUDP`, `WiFiNINA`, `WiFiS3`, and the host core all enforce it; ESP32's `WiFiUDP` is lenient but the explicit call is portable). The example sketches and the README show this idiom.

### 5.6 PCMFlow pipeline integration

- `VbanReceiver` / `RtpReceiver` (L16 mode) → plug into `PCMFlow::setInputSource()` for direct playback.
- `VbanSender` / `RtpSender` (L16 mode) → drive from `PCMFlow::writeFrames()` or a mic recording task.
- `RawUdpStream` / `RawUdpSink` → compose with codec siblings (e.g. `G711Decoder` of incoming RAW datagrams).
- `RtpSender::writeEncoded()` / `RtpReceiver::readEncoded()` → compose with codec siblings for VoIP-style flows.

## 6. VBAN protocol scope

PCMFlowUDP implements only the portion of VBAN needed to exchange PCM audio with VBAN-aware tools, plus header-level Service detection.

| VBAN sub-protocol | Status |
|---|---|
| Audio (0x00) | **PCM only** — PCM16 LE is the primary path. Sample rates: VBAN's 21-entry table on TX; RX accepts any table-valid rate. μ-law / A-law / Opus / non-16-bit PCM via VBAN sub-codecs are out of scope (see §2 — RTP is the codec-aware path). |
| Serial (0x20) | Not implemented. |
| MIDI (0x40) | Not implemented. |
| Service (0x60) | **Header detection + user callback**. `parseServiceHeader()` and `VbanReceiver::setServiceCallback()` let the caller observe Service packets and send replies via the receiver's UDP socket. The library does not include a built-in responder payload (see §2). |

## 7. RTP protocol scope

PCMFlowUDP implements the subset of RFC 3550 needed for one-shot media streaming:

- Fixed 12-byte header (no CSRCs, no header extensions, no padding by default).
- Sequence number increments by 1 per packet, starting from a random value at `begin()` time.
- Timestamp increments by the number of samples per packet at the configured clock rate.
- SSRC is a fixed 32-bit value chosen at `begin()` time (randomized if not specified).
- Marker bit set on the first packet after `begin()` (and on caller request via `setMarker()`); otherwise zero.

Out of scope:
- **RTCP** (sender / receiver reports). RTP audio works without RTCP for many use cases; senders that absolutely need it can be added later as a separate `RtcpReporter` class without breaking the RTP classes.
- **SRTP / DTLS-SRTP** (§2).
- **Re-ordering / jitter buffer** (§2 — PCMFlow's ring buffer absorbs small jitter; application owns the rest).
- **Payload-format extensions** beyond what the static PTs require (e.g. Opus FEC negotiation lives in SDP, not in the RTP packet).

## 8. Memory & footprint targets

| Item | Target |
|------|--------|
| Flash (full library, RAW + VBAN + RTP, send + recv) | ≤ 20 KB |
| Flash (RAW only, one direction) | ≤ 3 KB |
| RAM, per `VbanSender` / `VbanReceiver` instance | ≤ 2 KB (including one 1500-byte packet buffer) |
| RAM, per `RtpSender` / `RtpReceiver` instance | ≤ 2 KB (including one 1500-byte packet buffer) |
| RAM, per `RawUdpSink` / `RawUdpStream` instance | ≤ 256 B + caller-supplied buffer |
| Per-call scratch | none (no dynamic allocation) |

Headers are encoded / decoded in place. Packet buffers are caller-owned where possible.

## 9. Repository layout

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
│  ├─ VbanProtocol.h/.cpp    # header constants, encode/parse
│  ├─ RtpSender.h/.cpp
│  ├─ RtpReceiver.h/.cpp
│  ├─ RtpProtocol.h/.cpp     # header constants, payload-type table, encode/parse
│  └─ pcmflowudp_version.h
├─ examples/
│  ├─ VbanMicToPc/           # ESP32 mic → VBAN Receptor on Windows
│  ├─ RtpVoipG711/           # ESP32 ↔ SIP softphone via RTP PCMU
│  └─ EspToEspRaw/           # ESP32 ↔ ESP32 RAW UDP loopback
├─ tests/
│  ├─ README.md / README.ja.md
│  ├─ conftest.py
│  ├─ pyproject.toml
│  ├─ smoke/
│  ├─ raw_loopback/          # RAW byte round-trip
│  ├─ vban_header/           # byte-exact VBAN header encode/parse
│  ├─ vban_loopback/         # PCM round-trip via VBAN
│  ├─ rtp_header/            # byte-exact RTP header encode/parse
│  ├─ rtp_loopback/          # L16 + encoded-payload round-trip via RTP
│  └─ interop/               # captured external traces (VBAN Receptor / gst-rtp) → decoder
├─ doc/
│  └─ sibling_library_brief.md
├─ tools/
│  └─ bump_version.py
└─ .github/workflows/
   └─ release.yml
```

## 10. Vendored upstream

**None.** VBAN's wire format is publicly documented by VB-Audio; RTP's is defined by RFC 3550 / 3551 / 7587. PCMFlowUDP implements both from the published specifications without reusing third-party code.

The host-side test target uses the `lang-ship:host` Arduino core, which provides a Berkeley-sockets-backed `WiFiUDP` / `IPAddress` matching the ESP32 API surface. The same `#include <WiFiUdp.h>` works on both targets; no shim is vendored inside this repo.

License hygiene: the shipped library (`src/`) is **MIT, single-author, no third-party attribution required**.

## 11. Testing

Same conventions as parent PCMFlow:

- pytest-embedded + Arduino CLI backend.
- Two profiles: `lang-ship:host` (logic, host-side UDP loopback, large fixtures, fast CI) and `esp32:esp32:esp32` (real hardware verification, footprint measurement).
- Per-feature test directory with `<feature>.ino`, `sketch.yaml`, `test_<feature>.py`.
- `EXPECT_TRUE` / `EXPECT_EQ` / `EXPECT_NEAR` macros, `TEST done N/M` Serial protocol.

PCMFlowUDP-specific test design:

| Test dir | Subject | Strategy |
|---|---|---|
| `raw_loopback/` | RAW sink → stream round-trip | host: two `WiFiUDP` instances on `127.0.0.1`; assert byte equality |
| `vban_header/` | byte-exact VBAN header encode/parse (Audio + Service) | feed known config; assert each header byte against a reference table |
| `vban_loopback/` | PCM → VBAN encode → VBAN decode → PCM, plus Service-callback round-trip | host: two `WiFiUDP` instances; bit-exact PCM check |
| `rtp_header/` | byte-exact RTP header encode/parse for each supported PT | feed known config; assert each header byte against a reference table |
| `rtp_loopback/` | L16 PCM round-trip + encoded-payload round-trip + sequence/timestamp continuity | host loopback; assert per-packet header fields and payload equality |
| `interop/` | parse captured external traces (VBAN Receptor, `gst-rtp` recorders) | static `.bin` fixtures; assert audio samples and metadata |

The host profile pins `lang-ship:host` 1.0.6 or newer (first release providing the Berkeley-sockets `WiFiUDP`).

## 12. Versioning

SemVer (`major.minor.patch`) maintained in `library.properties`, `library.json`, and `src/pcmflowudp_version.h`. Independent of the PCMFlow version.

## 13. License & trademarks

PCMFlowUDP: **MIT** ([LICENSE](LICENSE)). No vendored third-party code anywhere in this repo (`src/` is hand-written from the VBAN public specification and RFCs 3550 / 3551 / 7587; host-side UDP is provided externally by the `lang-ship:host` Arduino core, see §10 / §11).

**Trademark notice.** VBAN is a protocol developed and published by **VB-Audio Software**. The name "VBAN" — including its appearance in class names such as `VbanSender` / `VbanReceiver` — is used here in a **descriptive / nominative sense** to identify the wire protocol this library partially implements. PCMFlowUDP is **not affiliated with, endorsed by, or sponsored by VB-Audio Software**, and only a subset of the VBAN specification is supported (§6). Other product and company names mentioned (Voicemeeter, VBAN Receptor) are trademarks of their respective owners.
