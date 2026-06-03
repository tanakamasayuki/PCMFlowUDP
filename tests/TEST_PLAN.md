# PCMFlowUDP Hardware Test Plan

> Japanese: [TEST_PLAN.ja.md](TEST_PLAN.ja.md)

This document describes the M5Stack Core2 / CoreS3 hardware test plan for PCMFlowUDP.

The existing `tests/*_python_loopback/` tests already verify UDP interoperability between an ESP32 DUT and a Python peer. The Core2/CoreS3-specific value is in validating the M5Stack hardware-facing path: I2S speaker, microphone, LCD, buttons, battery operation, and long-running Wi-Fi behavior. Hardware tests therefore live under `tests/manual/` and are not collected by pytest automatically.

## Policy

- Each test uses `tests/manual/<name>/<name>.py`, `<name>.ino`, and `sketch.yaml`.
- Manual Python files do not use the `test_` prefix. Run them explicitly after preparing the required hardware.
- Always run manual tests with `-s` so serial logs, prompts, display checks, audio checks, and button instructions are visible.
- Use `dut.expect()` for anything software can judge. Use minimal `y` / `n` prompts only for audio, display, or physical controls.
- Wi-Fi credentials are passed from `.env` as `WIFI_SSID` / `WIFI_PASSWORD` build defines, matching the existing tests.
- M5Stack tests use `m5stack_core2` / `m5stack_cores3` profiles, separate from the generic `esp32` profile.

## Required Hardware

- M5Stack Core2 or CoreS3
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
sudo apt install ffmpeg gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav wireshark tshark
```

For VBAN real-application testing, use Windows with VBAN Receptor or Voicemeeter. Linux/macOS can still use pytest helpers and packet captures for VBAN, and GStreamer/ffmpeg for RTP.

## Network Assumptions

- Core2 and the PC are on the same L2 network.
- AP client isolation is disabled.
- The PC firewall allows UDP 6980 for VBAN and UDP 5004 for the RTP examples.
- Multicast is out of scope for this library. Use broadcast or an explicit IP address when VBAN fan-out is needed.
- Save packet captures as pcapng first. Use text output only as an inspection view extracted from the saved pcapng.
- When a failure needs to become a regression test, save the pcapng as a fixture under `tests/interop/captures/`.

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
uv run --env-file .env pytest manual/core2_rtp_g711_gstreamer/core2_rtp_g711_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_g722_gstreamer/core2_rtp_g722_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_opus_gstreamer/core2_rtp_opus_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

Use `--profile m5stack_cores3` for CoreS3. Each manual `sketch.yaml` should define both profiles:

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

  m5stack_cores3:
    fqbn: esp32:esp32:m5stack_cores3
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

Core2 uses `esp32:esp32:m5stack_core2`; CoreS3 uses `esp32:esp32:m5stack_cores3`. Device control uses `M5Unified`, with `M5GFX` pinned as the display dependency.

## Final Audio Parameters

| Path | Format | Sample rate | Channels | Chunk / packet | Reason |
|---|---|---:|---:|---:|---|
| VBAN PCM reference | PCM16 LE | 16000 Hz | 1 | 256 frames | Matches existing examples and tests, is in the VBAN table, and keeps Core2/Wi-Fi load modest |
| RTP L16 reference | PCM16 BE on wire, PT 11 | 16000 Hz | 1 | 320 frames (20 ms) | Matches `RtpSender` default packetization and is easy to drive from GStreamer |
| RTP PCMU reference | G.711 mu-law, PT 0 | 8000 Hz | 1 | 160 samples (20 ms) | Matches RFC 3551 static payload type behavior and the existing `RtpVoipG711` example |
| RTP G.722 reference | G.722, PT 9 | 16000 Hz audio / 8000 Hz RTP clock | 1 | 160 bytes (20 ms) | Matches the RFC 3551 G.722 timestamp convention and `PCMFlowG722`'s 16 kHz PCM API |
| RTP Opus reference | Opus, dynamic PT 96 | 48000 Hz RTP clock | 1 | 20 ms packet | Verifies GStreamer RTP/Opus into the `PCMFlowOpus` decode path |
| VBAN / RTP stereo extension | PCM16 | 48000 Hz | 2 | 64-256 frames | Optional real-application compatibility check, not part of the first Core2 implementation |

Codec interop covers G711, G722, and Opus. Detailed codec correctness belongs to each codec library's own automated tests; this repo verifies that external RTP payloads can be received, decoded, and played.

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
| `core2_rtp_gstreamer/` | Verify Core2 playback of RTP/L16 sent by GStreamer | Tool logs + DUT stats + audio check | Added |
| `core2_rtp_g711_gstreamer/` | Verify Core2 G711 decode/playback of RTP/PCMU sent by GStreamer | Tool logs + DUT stats + audio check | Added |
| `core2_rtp_g722_gstreamer/` | Verify Core2 G722 decode/playback of RTP/G.722 sent by GStreamer | Tool logs + DUT stats + audio check | Added |
| `core2_rtp_opus_gstreamer/` | Verify DUT Opus decode/playback of RTP/Opus dynamic PT sent by GStreamer | Build size + runtime heap + audio check | Added |
| `core2_stability/` | Run UDP audio for 30 minutes and check drops, heap, and Wi-Fi state | Serial stats | Added |

## Test Details

When keeping packet capture evidence, save pcapng files with these per-test names. Inspect text summaries from the saved pcapng with `tshark -r ... -T fields`.

| Test | Capture filter | Saved file |
|---|---|---|
| `core2_raw_udp_ping/` | `udp` | `core2_raw_udp_ping.pcapng` |
| `core2_vban_speaker/` | `udp port 6980` | `core2_vban_speaker.pcapng` |
| `core2_vban_mic/` | `udp port 6980` | `core2_vban_mic.pcapng` |
| `core2_vban_receptor/` | `udp port 6980` | `core2_vban_receptor.pcapng` |
| `core2_voicemeeter_to_speaker/` | `udp port 6980` | `core2_voicemeeter_to_speaker.pcapng` |
| `core2_rtp_speaker/` | `udp port 5004` | `core2_rtp_speaker.pcapng` |
| `core2_rtp_mic/` | `udp port 5004` | `core2_rtp_mic.pcapng` |
| `core2_rtp_gstreamer/` | `udp port 5004` | `core2_rtp_gstreamer.pcapng` |
| `core2_rtp_g711_gstreamer/` | `udp port 5004` | `core2_rtp_g711_gstreamer.pcapng` |
| `core2_rtp_g722_gstreamer/` | `udp port 5004` | `core2_rtp_g722_gstreamer.pcapng` |
| `core2_rtp_opus_gstreamer/` | `udp port 5004` | `core2_rtp_opus_gstreamer.pcapng` |
| `core2_stability/` | `udp port 6980 or udp port 5004` | `core2_stability.pcapng` |

### core2_smoke

Purpose:
Verify that the Core2 manual test foundation works.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_smoke/core2_smoke.py -v -s --profile m5stack_core2
```

Pass conditions:
- Serial prints `CORE2-READY ip=<addr>`.
- The LCD shows the IP address.
- Pressing A/B/C prints `BUTTON A` / `BUTTON B` / `BUTTON C`.

### core2_raw_udp_ping

Purpose:
Verify the minimum `RawUdpStream` / `RawUdpSink` path on a real Core2 Wi-Fi network.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_raw_udp_ping/core2_raw_udp_ping.py -v -s --profile m5stack_core2
```

Pass conditions:
- Python receives an ACK payload identical to the sent payload.
- DUT Serial prints `RAW-RX len=64 crc=<expected>`.

### core2_vban_speaker

Purpose:
Verify VBAN receive, PCM16 decode, and Core2 speaker output.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_vban_speaker/core2_vban_speaker.py -v -s --profile m5stack_core2
```

Pass conditions:
- DUT prints `VBAN-RX rate=16000 channels=1 packets=<n> drops=0`.
- The operator confirms the 5-second tone and answers `y` to the pytest prompt.

### core2_vban_mic

Purpose:
Verify that Python can receive Core2 microphone input as VBAN PCM16.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_vban_mic/core2_vban_mic.py -v -s --profile m5stack_core2
```

Pass conditions:
- Python receives at least 100 packets in 5 seconds.
- The frame counter increases monotonically.
- RMS exceeds the silence threshold at least once.
- DUT prints `VBAN-TX packets=<n>`.

### core2_vban_receptor

Purpose:
Verify that VBAN Receptor or Voicemeeter can receive and play Core2 microphone input.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_vban_receptor/core2_vban_receptor.py -v -s --profile m5stack_core2
```

Pass conditions:
- DUT keeps printing `VBAN-TX packets=<n> drops=0`.
- VBAN Receptor / Voicemeeter shows the `Core2Mic` stream.
- The app meter reacts to input and the PC plays the Core2 microphone audio.

### core2_voicemeeter_to_speaker

Purpose:
Verify that Core2 receives a VBAN PCM stream from Voicemeeter and plays it through the speaker.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_voicemeeter_to_speaker/core2_voicemeeter_to_speaker.py -v -s --profile m5stack_core2
```

Pass conditions:
- DUT prints `VBAN-RX stream=PcToCore2 rate=<rate> channels=<n> packets=<n> drops=0`.
- The operator hears PC-side audio from the Core2 speaker.

### core2_rtp_speaker

Purpose:
Verify RTP receive, payload decode, and Core2 speaker output.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_speaker/core2_rtp_speaker.py -v -s --profile m5stack_core2
```

Pass conditions:
- DUT prints `RTP-RX packets=<n> drops=0`.
- The operator confirms the 5-second tone and answers `y` to the pytest prompt.

### core2_rtp_mic

Purpose:
Verify that Python can receive Core2 microphone input as RTP payloads.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_mic/core2_rtp_mic.py -v -s --profile m5stack_core2
```

Pass conditions:
- Python receives at least 100 packets in 5 seconds.
- Sequence numbers increase monotonically.
- Timestamp increments match the payload sample count.
- RMS exceeds the silence threshold at least once.

### core2_rtp_gstreamer

Purpose:
Verify Core2 playback of RTP/L16 mono sent by GStreamer.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_gstreamer/core2_rtp_gstreamer.py -v -s --profile m5stack_core2
```

Use the RTP/L16 GStreamer command in [Real-Application Commands](#real-application-commands) to send audio to the DUT. GStreamer commonly emits 16 kHz L16 as dynamic PT 96, and this test accepts that PT as L16.

### core2_rtp_g711_gstreamer

Purpose:
Verify Core2 G711 decode/playback of RTP/PCMU sent by GStreamer.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_g711_gstreamer/core2_rtp_g711_gstreamer.py -v -s --profile m5stack_core2
```

Use the RTP/PCMU GStreamer or ffmpeg command in [Real-Application Commands](#real-application-commands) to send audio to the DUT.

### core2_rtp_g722_gstreamer

Purpose:
Verify Core2 G722 decode/playback of RTP/G.722 sent by GStreamer.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_g722_gstreamer/core2_rtp_g722_gstreamer.py -v -s --profile m5stack_core2
```

Use the RTP/G.722 GStreamer command in [Real-Application Commands](#real-application-commands) to send audio to the DUT.

### core2_rtp_opus_gstreamer

Purpose:
Verify DUT Opus decode/playback of RTP/Opus dynamic PT 96 sent by GStreamer.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_rtp_opus_gstreamer/core2_rtp_opus_gstreamer.py -v -s --profile m5stack_core2
```

Use the RTP/Opus GStreamer command in [Real-Application Commands](#real-application-commands) to send audio to the DUT.

### core2_stability

Purpose:
Verify that a long-running UDP audio stream does not cause heap leaks, packet drops, or unrecoverable Wi-Fi disconnects.

Run:

```sh
cd tests
uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

Short smoke run:

```sh
cd tests
CORE2_STABILITY_SECONDS=60 uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

Pass conditions:
- No `WIFI_ERROR` appears during the 30-minute run.
- Drop rate stays below 0.1%.
- Minimum free heap does not keep falling by more than 10% from the initial value.
- DUT prints `STABILITY done`.

## Real-Application Commands

Send RTP/L16 mono from PC to Core2:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16BE,rate=16000,channels=1 \
  ! rtpL16pay \
  ! udpsink host=<core2-ip> port=5004
```

Send RTP/PCMU from PC to Core2:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,rate=8000,channels=1 \
  ! audioconvert \
  ! mulawenc \
  ! rtppcmupay pt=0 \
  ! udpsink host=<core2-ip> port=5004
```

Send RTP/G.722 from PC to DUT:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16LE,rate=16000,channels=1 \
  ! audioconvert \
  ! avenc_g722 \
  ! rtpg722pay pt=9 \
  ! udpsink host=<dut-ip> port=5004
```

Send RTP/Opus from PC to DUT:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16LE,rate=48000,channels=1 \
  ! audioconvert \
  ! opusenc bitrate=24000 frame-size=20 audio-type=voice \
  ! rtpopuspay pt=96 \
  ! udpsink host=<dut-ip> port=5004
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

Save pcapng:

```sh
tshark -i any -f "udp port 6980 or udp port 5004" -w core2_stability.pcapng
```

Inspect as text:

```sh
tshark -r core2_stability.pcapng -Y "udp.port == 6980 || rtp || udp.port == 5004" \
  -T fields -e frame.time_relative -e ip.src -e ip.dst -e udp.srcport -e udp.dstport -e udp.length -e rtp.p_type -e rtp.seq -e rtp.timestamp
```

## Implementation Order

1. Add `tests/manual/README.md` and `tests/manual/README.ja.md`.
2. Add `core2_smoke/` for flash, serial, LCD, buttons, and Wi-Fi.
3. Add `core2_raw_udp_ping/` to stabilize the non-audio UDP hardware path.
4. Add `core2_vban_speaker/` and `core2_vban_mic/`.
5. Add `core2_vban_receptor/` and `core2_voicemeeter_to_speaker/`.
6. Add `core2_rtp_speaker/` and `core2_rtp_mic/`.
7. Add `core2_rtp_gstreamer/` and codec-specific GStreamer tests.
8. Add `core2_stability/`.

## Decisions

- Core2 FQBN: `esp32:esp32:m5stack_core2`; CoreS3 FQBN: `esp32:esp32:m5stack_cores3`.
- Core2/CoreS3 device library: `M5Unified`.
- Core2/CoreS3 profiles pin `M5Unified (0.2.16)` and `M5GFX (0.2.22)`.
- Required baseline PCM audio format: `16000 Hz / mono / PCM16`.
- VBAN reference chunk: `256 frames`.
- RTP/L16 reference packet: `16000 Hz / 20 ms / 320 frames / PT 11`.
- RTP/PCMU reference packet: `8000 Hz / 20 ms / 160 samples / PT 0`.
- RTP/G.722 reference packet: `PT 9 / 20 ms / 160 bytes`.
- RTP/Opus reference packet: `PT 96 / 48000 Hz RTP clock / 20 ms`.
- Speaker tests are officially human-judged. External audio loopback automation is optional.
