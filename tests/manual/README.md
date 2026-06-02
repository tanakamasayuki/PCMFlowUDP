# PCMFlowUDP Manual Tests

> Japanese: [README.ja.md](README.ja.md)

This directory contains tests that require real hardware or human observation, such as M5Stack Core2 audio, display, buttons, and real Wi-Fi behavior.

Byte-level UDP protocol interoperability is covered by `tests/*_python_loopback/`. The manual tests here focus on Core2 speaker output, microphone input, LCD display, buttons, and long-running Wi-Fi operation.

See [../TEST_PLAN.md](../TEST_PLAN.md) for the detailed plan.

## Running Tests

Manual test Python files do not use the `test_` prefix, so pytest does not collect them automatically. Prepare the required hardware and run them explicitly:

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

Always include `-s`; serial logs, operator prompts, and test instructions must be visible in the terminal.

## Recommended Environment

Use pytest + Python helpers for reference checks, and run real-application interoperability as separate manual tests.

| Purpose | Recommended tools |
|---|---|
| Core2 flash / serial / automated checks | `uv run pytest`, `pytest-embedded`, Arduino CLI |
| VBAN real-application receive | VB-Audio VBAN Receptor |
| VBAN real-application send | VB-Audio Voicemeeter |
| RTP real-application send/receive | GStreamer `gst-launch-1.0`, `ffmpeg`, `ffplay`, VLC |
| Packet capture | Wireshark, `tshark` |

On Linux:

```sh
sudo apt install ffmpeg gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad wireshark tshark
```

For VBAN real-application testing, use a Windows PC with VB-Audio VBAN Receptor / Voicemeeter. Keep Core2 and the PC on the same LAN, and allow UDP 6980 for VBAN and UDP 5004 for the RTP examples in the PC firewall.

## Planned Tests

| Test | Description | Required hardware | Status |
|---|---|---|---|
| `core2_smoke/` | Core2 build, flash, serial, LCD, buttons, and Wi-Fi | M5Stack Core2 | Added |
| `core2_raw_udp_ping/` | RAW UDP round trip between Python and Core2 | M5Stack Core2 + Wi-Fi AP | Added |
| `core2_vban_speaker/` | Python sends VBAN PCM16 to the Core2 speaker | M5Stack Core2 + Wi-Fi AP | Added |
| `core2_vban_mic/` | Python receives VBAN PCM16 from the Core2 mic | M5Stack Core2 + Wi-Fi AP | Added |
| `core2_vban_receptor/` | VBAN Receptor / Voicemeeter receives and plays Core2 mic audio | M5Stack Core2 + Windows PC + VBAN Receptor / Voicemeeter | Added |
| `core2_voicemeeter_to_speaker/` | Core2 speaker plays a VBAN stream sent by Voicemeeter | M5Stack Core2 + Windows PC + Voicemeeter | Added |
| `core2_rtp_speaker/` | Python sends RTP audio to the Core2 speaker | M5Stack Core2 + Wi-Fi AP | Added |
| `core2_rtp_mic/` | Python receives RTP audio from the Core2 mic | M5Stack Core2 + Wi-Fi AP | Added |
| `core2_rtp_gstreamer/` | Core2 interoperates with GStreamer, ffmpeg, VLC, or another RTP tool | M5Stack Core2 + Linux PC or RTP-capable software | Added |
| `core2_stability/` | 30-minute UDP audio stream with drop, heap, and Wi-Fi checks | M5Stack Core2 + Wi-Fi AP | Added |

## Real-Application Command Examples

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

Receive RTP/L16 from Core2 and play it on the PC:

```sh
gst-launch-1.0 -v udpsrc port=5004 \
  caps="application/x-rtp,media=audio,clock-rate=16000,encoding-name=L16,channels=1,payload=11" \
  ! rtpL16depay \
  ! audioconvert \
  ! autoaudiosink
```

Packet capture:

```sh
tshark -i any -f "udp port 6980 or udp port 5004" -Y "udp.port == 6980 || rtp || udp.port == 5004"
```

Shortened stability run:

```sh
CORE2_STABILITY_SECONDS=60 uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

## Judgment Policy

- Use pytest automation for packet format, sequence, timestamp, RMS, heap, and other software-observable data.
- Use operator confirmation for speaker audio, LCD display, and physical buttons.
- Operator prompts must state a concrete expected result and accept `y` / `n`.
- Prefer one test function per `.py` file to avoid state leakage after flashing.
