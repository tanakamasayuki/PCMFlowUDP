# Tests

> 日本語版: [README.ja.md](README.ja.md)

Automated test suite for PCMFlowUDP. Mirrors the conventions of the parent [PCMFlow test suite](https://github.com/tanakamasayuki/PCMFlow/tree/main/tests):

- [pytest-embedded](https://docs.espressif.com/projects/pytest-embedded/en/latest/) + Arduino CLI backend.
- Two profiles: `lang-ship:host` (logic verification, host-side UDP loopback, fast CI) and `esp32:esp32:esp32` (real hardware verification, footprint measurement).
- Per-feature subdirectory containing `<feature>.ino`, `sketch.yaml`, `test_<feature>.py`, and (where relevant) an `input/` directory of fixtures.
- Assertions use the `EXPECT_TRUE` / `EXPECT_EQ` / `EXPECT_NEAR` macros and the `TEST done N/M` Serial protocol.

The host profile pins **`lang-ship:host` 1.0.6 or newer**, which is the first release shipping the Berkeley-sockets-backed `WiFiUDP` / `IPAddress` used by the UDP loopback tests.

## Directory layout

- `smoke/` — Template smoke test (host profile). Verifies the test infrastructure itself and that PCMFlowUDP compiles against the chosen profile.
- *(planned)* `raw_loopback/` — Two `WiFiUDP` instances on `127.0.0.1` exchanging bytes via `RawUdpSink` → `RawUdpStream`.
- *(planned)* `vban_header/` — Byte-exact VBAN header encoding for known input configurations.
- *(planned)* `vban_loopback/` — PCM round-trip through `VbanSender` → `VbanReceiver`.
- *(planned)* `vban_interop/` — Decoding of captured packets from VB-Audio Voicemeeter / VBAN Receptor.

## ESP32 vs host

The two profiles are designed to run **the same `.ino` source** without `#ifdef ARDUINO_ARCH_ESP32` guards. The `lang-ship:host` core ships a thin Arduino-API stub layer (`WiFi.h`, `WiFiUdp.h`, `IPAddress.h`, `Stream.h`, …) so device-style code Just Works on a Linux process. In practice this means:

| API | ESP32 behavior | Host stub behavior |
|---|---|---|
| `Serial.begin(115200)` | real USB-CDC bridge; needs ~5 s to settle before output is visible to pytest | virtual stdout via the harness; ready immediately, but the 5 s `delay()` is kept for parity and is harmless |
| `WiFi.mode(WIFI_STA)` / `WiFi.begin(ssid, pw)` | real Wi-Fi join; returns once `WL_CONNECTED` (timeout-guarded) | no-op that immediately reports `WL_CONNECTED` |
| `WiFi.localIP()` | the address the AP / DHCP assigned, e.g. `192.168.13.152` | always `127.0.0.1` |
| `WiFiUDP::begin(port)` | binds an lwIP UDP socket | binds a BSD `SOCK_DGRAM` |
| `WiFiUDP::beginPacket(host, port)` + `endPacket()` | sends through the WiFi stack (real Ethernet frames) | `sendto()` on the BSD socket |
| `random()` | hardware RNG (ESP32) | `libc` `rand()` with a per-process seed |

### Code-style rules

- **Do not gate behavior with `#ifdef ARDUINO_ARCH_ESP32`** in test sketches or examples. The same source must run on both targets. If you find yourself wanting a guard, the right fix is usually to teach the host stub the missing API.
- **Always call `udp.begin(port)` before any send or receive.** ESP32's `WiFiUDP` is lenient (opens a socket lazily inside `beginPacket`) but every other Arduino UDP implementation — `EthernetUDP`, `WiFiNINA`, `WiFiS3`, and the `lang-ship:host` core — requires it. Use `udp.begin(0)` for an ephemeral local bind when you only need to send.
- **`Serial.begin()` is followed by `delay(5000);`** in every test sketch. ESP32's USB-Serial bridge can drop the first few hundred ms of output otherwise. Harmless on host.

### Tests that cannot run on ESP32

A handful of tests are intentionally limited to the `host` profile because their flow has no equivalent on real hardware. Their `sketch.yaml` does not declare an `esp32` profile, and the top-of-file comment in the `.ino` explains why.

| Test | Reason it is host-only |
|---|---|
| `raw_loopback/` | Two `WiFiUDP` instances in the same process exchange a UDP datagram over `127.0.0.1`. arduino-esp32's lwIP build does not enable the loopback interface (`LWIP_HAVE_LOOPIF`), so 127.0.0.1 cannot deliver. Even with `WiFi.localIP()` substituted, hairpin routing through the AP would be required (not portable). The on-wire RAW path is covered on real hardware by `raw_python_loopback/`. |
| `vban_loopback/` | Same root cause as `raw_loopback/`. On-wire VBAN coverage on real hardware lives in `vban_python_loopback/`. |
| `rtp_loopback/` | Same root cause as `raw_loopback/`. On-wire RTP coverage on real hardware lives in `rtp_python_loopback/`. |

If you ever need to add an ESP32-side equivalent, the right pattern is **two devices** (or device + host PC) talking via the LAN — the existing `*_python_loopback/` tests already do this with pytest playing the role of the second peer.

## Trademark note

The `vban_*` test directories exercise PCMFlowUDP's subset implementation of the VBAN wire protocol. "VBAN" is a protocol name owned by VB-Audio Software; see [../SPEC.md §13](../SPEC.md#13-license--trademarks) for the full notice.
