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
uv run --env-file .env pytest manual/core2_rtp_g711_gstreamer/core2_rtp_g711_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_g722_gstreamer/core2_rtp_g722_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_rtp_opus_gstreamer/core2_rtp_opus_gstreamer.py -v -s --profile m5stack_core2
uv run --env-file .env pytest manual/core2_stability/core2_stability.py -v -s --profile m5stack_core2
```

Always include `-s`; serial logs, operator prompts, and test instructions must be visible in the terminal. Use `--profile m5stack_cores3` for M5Stack CoreS3.

## Recommended Environment

Use pytest + Python helpers for reference checks, and run real-application interoperability as separate manual tests.

| Purpose | Recommended tools |
|---|---|
| Core2 flash / serial / automated checks | `uv run pytest`, `pytest-embedded`, Arduino CLI |
| VBAN real-application receive | VB-Audio VBAN Receptor |
| VBAN real-application send | VB-Audio Voicemeeter, VBAN Talkie |
| RTP real-application send/receive | GStreamer `gst-launch-1.0`, `ffmpeg`, `ffplay`, VLC |
| Packet capture | Wireshark, `tshark` |

On Linux:

```sh
sudo apt install ffmpeg gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav wireshark tshark
```

On Windows, use MSYS2 UCRT64 and winget:

```sh
pacman -Syu

pacman -S \
  mingw-w64-ucrt-x86_64-ffmpeg \
  mingw-w64-ucrt-x86_64-gstreamer \
  mingw-w64-ucrt-x86_64-gst-plugins-base \
  mingw-w64-ucrt-x86_64-gst-plugins-good \
  mingw-w64-ucrt-x86_64-gst-plugins-bad \
  mingw-w64-ucrt-x86_64-gst-libav
```

```powershell
winget install WiresharkFoundation.Wireshark
winget install VBurel.VBAN.Receptor
winget install VBurel.VBAN.Talkie
```

Verify the installation:

```sh
gst-launch-1.0 --version
gst-inspect-1.0 rtppcmupay
gst-inspect-1.0 rtpg722pay
gst-inspect-1.0 rtpopuspay
gst-inspect-1.0 opusenc
gst-inspect-1.0 avenc_g722
ffmpeg -version
```

```powershell
winget list VBAN
```

For VBAN real-application testing, use a Windows PC with VB-Audio VBAN Receptor / Voicemeeter / VBAN Talkie. `core2_voicemeeter_to_speaker/` is named for Voicemeeter, but VBAN Talkie's sender can be used as a substitute check. Keep Core2/CoreS3 and the PC on the same LAN, and allow UDP 6980 for VBAN and UDP 5004 for the RTP examples in the PC firewall.

## Planned Tests

| Test | Description | Required hardware | Status |
|---|---|---|---|
| `core2_smoke/` | Core2/CoreS3 build, flash, serial, LCD, buttons, and Wi-Fi | M5Stack Core2 or CoreS3 | Added |
| `core2_raw_udp_ping/` | RAW UDP round trip between Python and the DUT | M5Stack Core2/CoreS3 + Wi-Fi AP | Added |
| `core2_vban_speaker/` | Python sends VBAN PCM16 to the DUT speaker | M5Stack Core2/CoreS3 + Wi-Fi AP | Added |
| `core2_vban_mic/` | Python receives VBAN PCM16 from the DUT mic | M5Stack Core2/CoreS3 + Wi-Fi AP | Added |
| `core2_vban_receptor/` | VBAN Receptor / Voicemeeter receives and plays DUT mic audio | M5Stack Core2/CoreS3 + Windows PC + VBAN Receptor / Voicemeeter | Added |
| `core2_voicemeeter_to_speaker/` | DUT speaker plays a VBAN stream sent by Voicemeeter | M5Stack Core2/CoreS3 + Windows PC + Voicemeeter | Added |
| `core2_rtp_speaker/` | Python sends RTP audio to the DUT speaker | M5Stack Core2/CoreS3 + Wi-Fi AP | Added |
| `core2_rtp_mic/` | Python receives RTP audio from the DUT mic | M5Stack Core2/CoreS3 + Wi-Fi AP | Added |
| `core2_rtp_gstreamer/` | DUT plays RTP/L16 sent by GStreamer | M5Stack Core2/CoreS3 + Linux PC | Added |
| `core2_rtp_g711_gstreamer/` | DUT decodes and plays RTP/PCMU sent by GStreamer | M5Stack Core2/CoreS3 + Linux PC | Added |
| `core2_rtp_g722_gstreamer/` | DUT decodes and plays RTP/G.722 sent by GStreamer | M5Stack Core2/CoreS3 + Linux PC | Added |
| `core2_rtp_opus_gstreamer/` | DUT decodes and plays RTP/Opus sent by GStreamer | M5Stack Core2/CoreS3 + Linux PC | Added |
| `core2_stability/` | 30-minute UDP audio stream with drop, heap, and Wi-Fi checks | M5Stack Core2/CoreS3 + Wi-Fi AP | Added |

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
  ! audioconvert \
  ! mulawenc \
  ! rtppcmupay pt=0 \
  ! udpsink host=<core2-ip> port=5004
```

Send RTP/G.722 from PC to Core2:

```sh
gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \
  ! audio/x-raw,format=S16LE,rate=16000,channels=1 \
  ! audioconvert \
  ! avenc_g722 \
  ! rtpg722pay pt=9 \
  ! udpsink host=<core2-ip> port=5004
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

Receive RTP/L16 from Core2 and play it on the PC:

```sh
gst-launch-1.0 -v udpsrc port=5004 \
  caps="application/x-rtp,media=audio,clock-rate=16000,encoding-name=L16,channels=1,payload=11" \
  ! rtpL16depay \
  ! audioconvert \
  ! autoaudiosink
```

Save packet captures as pcapng and inspect text summaries only when needed. See `tests/TEST_PLAN.md` for per-test file names.

Save pcapng:

```sh
tshark -i any -f "udp port 6980 or udp port 5004" -w core2_stability.pcapng
```

Inspect as text:

```sh
tshark -r core2_stability.pcapng -Y "udp.port == 6980 || rtp || udp.port == 5004" \
  -T fields -e frame.time_relative -e ip.src -e ip.dst -e udp.srcport -e udp.dstport -e udp.length -e rtp.p_type -e rtp.seq -e rtp.timestamp
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
