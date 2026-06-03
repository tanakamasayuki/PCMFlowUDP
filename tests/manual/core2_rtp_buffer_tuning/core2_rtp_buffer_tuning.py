"""
Purpose:
    Tune RTP/L16 receive prebuffer and playback chunk sizes on Core2.

Why manual:
    The operator changes presets with Core2 buttons and judges audible
    continuity while the sketch reports packet and playback-risk stats.
"""

import re


def test_core2_rtp_buffer_tuning(dut):
    match = dut.expect(
        [
            re.compile(rb"DUT-READY ip=(\S+) port=5004"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=90,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connection failed: {match.group(1).decode()}")

    core2_ip = match.group(1).decode()
    gst_command = (
        "gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true "
        "! volume volume=0.5 "
        "! audioconvert "
        "! audio/x-raw,format=S16BE,rate=16000,channels=1 "
        "! rtpL16pay "
        f"! udpsink host={core2_ip} port=5004"
    )
    print("\n=== Start this GStreamer command on the PC and keep it running ===", flush=True)
    print(gst_command, flush=True)
    print("\nReadable form:", flush=True)
    print(
        "gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true \\\n"
        "  ! volume volume=0.5 \\\n"
        "  ! audioconvert \\\n"
        "  ! audio/x-raw,format=S16BE,rate=16000,channels=1 \\\n"
        "  ! rtpL16pay \\\n"
        f"  ! udpsink host={core2_ip} port=5004",
        flush=True,
    )
    print("\nCore2 controls:")
    print("  Button A: next preset")
    print("  Button B: previous preset")
    print("  Button C: restart current preset and reset stats")
    print("\nPresets:")
    print("  p0-low      initial=1 packet  chunk=1 packet  initial latency=20 ms")
    print("  p1-voip     initial=2 packets chunk=1 packet  initial latency=40 ms")
    print("  p2-balanced initial=2 packets chunk=2 packets initial latency=40 ms")
    print("  p3-safe     initial=3 packets chunk=2 packets initial latency=60 ms")
    print("  p4-stable   initial=4 packets chunk=2 packets initial latency=80 ms")
    print("\nProcedure:")
    print("  1. Start the GStreamer command and keep it running.")
    print("  2. Wait until TUNE ... state=RUNNING appears.")
    print("  3. While GStreamer is still running, press A/B to switch presets.")
    print("  4. Each preset switch intentionally stops playback, clears local buffers,")
    print("     enters PREFILLING, then resumes RUNNING. Ignore that transition gap.")
    print("  5. Listen only after RUNNING appears for the selected preset.")
    print("  6. Press C to retest the same preset from a clean prefill.")
    print("  7. Stop GStreamer only after you finish comparing presets.")
    print("\nWhat to watch:")
    print("  Main judgment: your ears, drop=0, empty=0, and wait=0 or stable.")
    print("  late_delta= is advisory only; it can increase even when playback sounds stable.")
    print("\nRecord one row per preset:")
    print("  preset, pass/fail/borderline, audible gaps after RUNNING,")
    print("  seconds to become stable after restart, drop, empty, wait, final TUNE line, notes")
    print("Watch TUNE lines and listen for gaps. Press Enter here when done.", flush=True)

    dut.expect(re.compile(rb"TUNE\s+.*state=(PREFILLING|RUNNING)"), timeout=120)
    input("Press Enter after buffer tuning is complete > ")
