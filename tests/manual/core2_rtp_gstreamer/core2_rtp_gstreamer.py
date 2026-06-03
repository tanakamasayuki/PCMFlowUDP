"""
Purpose:
    Verify GStreamer -> Core2 RTP/L16 speaker interoperability.

Why manual:
    Requires an external RTP sender (`gst-launch-1.0`) and human confirmation
    of Core2 speaker output.
"""

import re


def test_core2_rtp_gstreamer(dut):
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
    print("\nRun this RTP/L16 command on the PC:")
    print("GStreamer commonly emits 16 kHz L16 as dynamic PT 96; the DUT accepts that for this test.")
    print(
        "gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true "
        "! audio/x-raw,format=S16BE,rate=16000,channels=1 "
        "! rtpL16pay "
        f"! udpsink host={core2_ip} port=5004"
    )

    dut.expect(
        re.compile(rb"RTP-GST-RX pt=96 seq=\d+ ssrc=\d+ frames=320 packets=\d+ drops=0"),
        timeout=120,
    )

    answer = input("Did you hear a 1 kHz tone from the Core2 speaker? [y/n] > ").strip().lower()
    if answer != "y":
        raise AssertionError("operator did not confirm Core2 speaker output")
