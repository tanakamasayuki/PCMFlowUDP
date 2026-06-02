# PCMFlowUDP Hardware Test Plan

> Japanese: [TEST_PLAN.ja.md](TEST_PLAN.ja.md)

This document describes the M5Stack Core2 hardware test plan for PCMFlowUDP.

The existing `tests/*_python_loopback/` tests already verify UDP interoperability between an ESP32 DUT and a Python peer. The Core2-specific value is in validating the hardware-facing path: I2S speaker, microphone, LCD, buttons, battery operation, and long-running Wi-Fi behavior. Core2 tests therefore live under `tests/manual/` and are not collected by pytest automatically.

## Policy

- Each test uses `tests/manual/<name>/<name>.py`, `<name>.ino`, and `sketch.yaml`.
- Manual Python files do not use the `test_` prefix. Run them explicitly after preparing the required hardware.
- Always run manual tests with `-s` so serial logs, prompts, display checks, audio checks, and button instructions are visible.
- Use `dut.expect()` for anything software can judge. Use minimal `y` / `n` prompts only for audio, display, or physical controls.
- Wi-Fi credentials are passed from `.env` as `WIFI_SSID` / `WIFI_PASSWORD` build defines, matching the existing tests.
- Core2 tests use the `m5stack_core2` profile, separate from the generic `esp32` profile.

## Required Hardware

- M5Stack Core2
- USB-C cable
- 2.4 GHz Wi-Fi AP
- PC running pytest
- PC-side VBAN/RTP/RAW UDP tools
- Optional: external microphone or audio loopback device for more objective audio checks

## Recommended Tools

For Core2 manual tests, first use pytest + Python helpers to lock down packet-level expectations, then run real application interoperability tests. Real applications alone tend to make failures harder to diagnose because the result depends on UI state and what a human hears.

| Purpose | Recommended tool | Role |
|---|---|---|
| Reference checks | `uv run pytest` + `tests/net_helpers/` | Known packet TX/RX, RMS, sequence, timestamp, and format checks |
| VBAN receive | VB-Audio VBAN Receptor | Core2 mic to PC playback |
| VBAN send | VB-Audio Voicemeeter | PC audio to Core2 speaker |
| RTP send/receive | GStreamer `gst-launch-1.0` | Explicit RTP payload tests such as L16, PCMU, PCMA, and G.722 |
| RTP quick send | `ffmpeg` | Send sine waves or audio files as RTP/UDP |
| RTP quick receive | `ffplay` / VLC | Listen to RTP audio from Core2 |
| Packet inspection | Wireshark / `tshark` | Capture VBAN on UDP 6980 and RTP on UDP 5004 |

On Linux development machines:

```sh
sudo apt install ffmpeg gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad wireshark tshark
```

For VBAN real-application testing, use Windows with VBAN Receptor or Voicemeeter. Linux/macOS can still use pytest helpers and packet captures for VBAN, and GStreamer/ffmpeg for RTP.

## Network Assumptions

- Core2 and the PC are on the same L2 network.
- AP client isolation is disabled.
- The PC firewall allows UDP 6980 for VBAN and UDP 5004 for the RTP examples.
- Multicast is out of scope for this library. Use broadcast or an explicit IP address when VBAN fan-out is needed.
- Start packet capture with `udp port 6980 or udp port 5004`; when a failure needs to become a regression test, save the capture as a fixture under `tests/interop/captures/`.

## Running Tests

```sh
cd tests
uv run --env-file .env pytest manual/core2_smoke/core2_smoke.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_raw_udp_ping/core2_raw_udp_ping.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_vban_speaker/core2_vban_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_vban_mic/core2_vban_mic.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_vban_receptor/core2_vban_receptor.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_voicemeeter_to_speaker/core2_voicemeeter_to_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_speaker/core2_rtp_speaker.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_mic/core2_rtp_mic.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_gstreamer/core2_rtp_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

Each Core2 `sketch.yaml` should define:

```yaml
profiles:
  m5stack_core2:
    fqbn: esp32:esp32:m5stack_core2
    platforms:
      - platform: esp32:esp32 (3.3.8)
        platform_index_url: https://espressif.github.io/arduino-esp32/package_esp32_index.json
    libraries:
      - dir: ../../../
      - PCMFlow (0.2.1)
      - M5Unified (0.2.16)
      - M5GFX (0.2.22)

default_profile: m5stack_core2
```

Core2 uses the fixed FQBN `esp32:esp32:m5stack_core2`. Device control uses `M5Unified`, with `M5GFX` pinned as the display dependency.

## Final Audio Parameters

| Path | Format | Sample rate | Channels | Chunk / packet | Reason |
|---|---|---:|---:|---:|---|
| VBAN PCM reference | PCM16 LE | 16000 Hz | 1 | 256 frames | Matches existing examples and tests, is in the VBAN table, and keeps Core2/Wi-Fi load modest |
| RTP L16 reference | PCM16 BE on wire, PT 11 | 16000 Hz | 1 | 320 frames (20 ms) | Matches `RtpSender` default packetization and is easy to drive from GStreamer |
| RTP PCMU reference | G.711 mu-law, PT 0 | 8000 Hz | 1 | 160 samples (20 ms) | Matches RFC 3551 static payload type behavior and the existing `RtpVoipG711` example |
| VBAN / RTP stereo extension | PCM16 | 48000 Hz | 2 | 64-256 frames | Optional real-application compatibility check, not part of the first Core2 implementation |

The first Core2 manual tests require mono only. Stereo, 48 kHz, Opus, and G.722 are follow-up interoperability checks.

Speaker tests are officially judged by human confirmation for now. An external audio loopback device may add RMS, peak, or frequency checks later, but it is not required.

## Test Matrix

| Test | Purpose | Judgment | Status |
|---|---|---|---|
| `core2_smoke/` | Verify Core2 build, flash, serial, LCD, buttons, and Wi-Fi | `dut.expect()` + button checks | Added |
| `core2_raw_udp_ping/` | Python sends a RAW UDP packet to the DUT and receives an ACK | Fully automated | Added |
| `core2_vban_speaker/` | Python sends VBAN PCM16 sine wave packets to the Core2 speaker | Packet checks + human audio check | Added |
| `core2_vban_mic/` | Python receives non-silent VBAN PCM16 from the Core2 mic | Python RMS/peak checks | Added |
| `core2_vban_receptor/` | Core2 mic streams to VBAN Receptor / Voicemeeter | DUT stats + app UI/audio check | Added |
| `core2_voicemeeter_to_speaker/` | Voicemeeter sends VBAN stream to the Core2 speaker | DUT stats + audio check | Added |
| `core2_rtp_speaker/` | Core2 receives RTP payload and plays it through the speaker | Packet checks + human audio check | Added |
| `core2_rtp_mic/` | Python receives RTP payload from the Core2 mic | Python sequence/timestamp/RMS checks | Added |
| `core2_rtp_gstreamer/` | Verify interoperability with GStreamer, ffmpeg, VLC, or similar RTP tools | Tool logs + DUT stats + audio check | Planned |
| `core2_stability/` | Run UDP audio for 30 minutes and check drops, heap, and Wi-Fi state | Serial stats | Planned |

## Real-Application Commands

Send RTP/L16 mono from PC to Core2:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16BE,rate=16000,channels=1 \
  ! rtpL16pay pt=11 \
  ! udpsink host=<core2-ip> port=5004
```

Send RTP/PCMU from PC to Core2:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,rate=8000,channels=1 \
  ! mulawenc \
  ! rtppcmupay pt=0 \
  ! udpsink host=<core2-ip> port=5004
```

Send an audio file as RTP/PCMU:

```sh
ffmpeg -re -i input.wav -ac 1 -ar 8000 -c:a pcm_mulaw -f rtp rtp://<core2-ip>:5004
```

Receive RTP/L16 from Core2 and play it on the PC:

```sh
gst-launch-1.0 -v udpsrc port=5004 \
  caps="application/x-rtp,media=audio,clock-rate=16000,encoding-name=L16,channels=1,payload=11" \
  ! rtpL16depay \
  ! audioconvert \
  ! autoaudiosink
```

Receive RTP/PCMU from Core2 and play it on the PC:

```sh
gst-launch-1.0 -v udpsrc port=5004 \
  caps="application/x-rtp,media=audio,clock-rate=8000,encoding-name=PCMU,payload=0" \
  ! rtppcmudepay \
  ! mulawdec \
  ! audioconvert \
  ! autoaudiosink
```

Capture VBAN/RTP packets:

```sh
tshark -i any -f "udp port 6980 or udp port 5004" -Y "udp.port == 6980 || rtp || udp.port == 5004"
```

## Implementation Order

1. Add `tests/manual/README.md` and `tests/manual/README.ja.md`.
2. Add `core2_smoke/` for flash, serial, LCD, buttons, and Wi-Fi.
3. Add `core2_raw_udp_ping/` to stabilize the non-audio UDP hardware path.
4. Add `core2_vban_speaker/` and `core2_vban_mic/`.
5. Add `core2_vban_receptor/` and `core2_voicemeeter_to_speaker/`.
6. Add `core2_rtp_speaker/` and `core2_rtp_mic/`.
7. Add `core2_rtp_gstreamer/`.
8. Add `core2_stability/`.

## Decisions

- Core2 FQBN: `esp32:esp32:m5stack_core2`.
- Core2 device library: `M5Unified`.
- Core2 profile pins `M5Unified (0.2.16)` and `M5GFX (0.2.22)`.
- Required Core2 manual audio format: `16000 Hz / mono / PCM16`.
- VBAN reference chunk: `256 frames`.
- RTP/L16 reference packet: `16000 Hz / 20 ms / 320 frames / PT 11`.
- RTP/PCMU reference packet: `8000 Hz / 20 ms / 160 samples / PT 0`.
- Speaker tests are officially human-judged. External audio loopback automation is optional.
