"""
Purpose:
    Verify GStreamer -> PCMFlowUDP -> PCMFlowG722 -> Core2 speaker interop.

Why manual:
    Requires an external RTP/G.722 sender (`gst-launch-1.0`) and human
    confirmation of decoded Core2 speaker output.
"""

import re


def test_core2_rtp_g722_gstreamer(dut):
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
    print("\nRun this command on the PC:")
    print(
        "gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true "
        "! audio/x-raw,format=S16LE,rate=16000,channels=1 "
        "! audioconvert "
        "! avenc_g722 "
        "! rtpg722pay pt=9 "
        f"! udpsink host={core2_ip} port=5004"
    )

    dut.expect(
        re.compile(rb"RTP-G722-RX pt=9 seq=\d+ ssrc=\d+ bytes=160 frames=320 packets=\d+ drops=0"),
        timeout=120,
    )

    answer = input("Did you hear a decoded 1 kHz G.722 tone from the Core2 speaker? [y/n] > ").strip().lower()
    if answer != "y":
        raise AssertionError("operator did not confirm Core2 speaker output")
