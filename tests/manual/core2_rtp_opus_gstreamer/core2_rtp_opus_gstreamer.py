"""
Purpose:
    Verify GStreamer -> PCMFlowUDP -> PCMFlowOpus -> Core2/CoreS3 speaker interop.

Why manual:
    Requires an external RTP/Opus sender (`gst-launch-1.0`) and human
    confirmation of decoded speaker output.
"""

import re


def test_core2_rtp_opus_gstreamer(dut):
    match = dut.expect(
        [
            re.compile(rb"DUT-READY ip=(\S+) port=5004 heap=(\d+)"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=90,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connection failed: {match.group(1).decode()}")

    core_ip = match.group(1).decode()
    print("\nRun this command on the PC:")
    print(
        "gst-launch-1.0 -v audiotestsrc wave=sine freq=1000 is-live=true "
        "! audio/x-raw,format=S16LE,rate=48000,channels=1 "
        "! audioconvert "
        "! opusenc bitrate=24000 frame-size=20 audio-type=voice "
        "! rtpopuspay pt=96 "
        f"! udpsink host={core_ip} port=5004"
    )

    dut.expect(
        re.compile(rb"RTP-OPUS-RX pt=96 seq=\d+ ssrc=\d+ bytes=\d+ frames=960 packets=\d+ drops=0 heap=\d+"),
        timeout=120,
    )

    answer = input("Did you hear a decoded 1 kHz Opus tone from the speaker? [y/n] > ").strip().lower()
    if answer != "y":
        raise AssertionError("operator did not confirm speaker output")
