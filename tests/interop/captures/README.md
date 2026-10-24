# Interop captures

Drop raw UDP-payload `.bin` files (one packet per file) into this
directory. The interop test scans them automatically.

## Naming convention

The filename prefix selects the parser the test runs against:

| Prefix       | Parser                     | Source typically |
|--------------|----------------------------|------------------|
| `vban_*`     | `parseAudioHeader`         | Voicemeeter / VBAN Receptor audio packets |
| `vbansvc_*`  | `parseServiceHeader`       | VBAN Receptor identification / service packets |
| `rtp_*`      | `parseRtpHeader`           | gst-launch / ffmpeg / SIP softphone RTP packets |

Use descriptive suffixes so the test output is readable: e.g.
`vban_voicemeeter_48k_mono_pcm16_01.bin`, `rtp_gst_pcmu_01.bin`.

## How to capture

Use the packet-capture recipe below to save one UDP payload per file.
Keep the protocol, sample rate and channel count in the filename so the
interoperability test result can be traced to its source.

Quick recipe:

```bash
# Capture into a .pcapng with Wireshark on `udp port 6980` (VBAN)
# or `udp port 5004` (RTP).

# Extract one packet's UDP payload to a .bin:
tshark -r capture.pcapng -Y "udp.port == 6980" \
       -T fields -e udp.payload -c 1 | \
  xxd -r -p > vban_voicemeeter_01.bin
```

## What the test asserts

For each `.bin`, the test calls the matching parser and asserts that
it returns `Ok`. The point is to prove the C++ parser accepts wire
formats produced by independent implementations (catches symmetric
bugs that would otherwise pass the `*_loopback` tests).

If you want per-capture field assertions (e.g. "this packet should
report 48 kHz mono PCM16"), add them in `test_interop.py` keyed on
the filename — but a plain parse-success check is already a strong
interop signal.
