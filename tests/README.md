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

## Trademark note

The `vban_*` test directories exercise PCMFlowUDP's subset implementation of the VBAN wire protocol. "VBAN" is a protocol name owned by VB-Audio Software; see [../SPEC.md §14](../SPEC.md#14-license--trademarks) for the full notice.
