"""
Purpose:
    Verify that Core2 receives RTP/L16 mono and hands it to M5Unified speaker output.

Why manual:
    RTP packet reception is checked automatically, but audible speaker output
    requires operator confirmation unless an external audio loopback is attached.
"""

import math
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from net_helpers.raw_udp import RawUdp  # noqa: E402
from net_helpers.rtp import (  # noqa: E402
    RTP_PT_L16_MONO,
    RtpHeader,
    build_packet,
    pcm_to_be_bytes,
)


def test_core2_rtp_speaker(dut):
    match = dut.expect(
        [
            re.compile(rb"DUT-READY ip=(\S+) port=(\d+)"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=90,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connection failed: {match.group(1).decode()}")

    dut_ip = match.group(1).decode()
    dut_port = int(match.group(2))

    sample_rate = 16000
    frames = 320
    packets = 250
    ssrc = 0xC02E5001
    samples = [
        int(round(9000 * math.sin(2 * math.pi * 1000 * i / sample_rate)))
        for i in range(frames)
    ]
    payload = pcm_to_be_bytes(samples)
    packet_interval = frames / sample_rate

    with RawUdp(bind_port=0) as udp:
        next_send = time.monotonic()
        for seq in range(packets):
            header = RtpHeader(
                marker=(seq == 0),
                payload_type=RTP_PT_L16_MONO,
                sequence_number=seq,
                timestamp=seq * frames,
                ssrc=ssrc,
            )
            udp.send(build_packet(header, payload), dut_ip, dut_port)
            next_send += packet_interval
            time.sleep(max(0, next_send - time.monotonic()))

        rx = dut.expect(
            re.compile(
                rb"RTP-RX pt=(\d+) seq=(\d+) ssrc=(\d+) frames=(\d+) "
                rb"packets=(\d+) drops=(\d+) s0=(-?\d+) s1=(-?\d+) s2=(-?\d+)"
            ),
            timeout=10,
        )

    assert int(rx.group(1)) == RTP_PT_L16_MONO
    assert int(rx.group(3)) == ssrc
    assert int(rx.group(4)) == frames
    assert int(rx.group(5)) >= 1
    assert int(rx.group(6)) == 0

    answer = input("Did you hear a 1 kHz tone from the Core2 speaker? [y/n] > ").strip().lower()
    if answer != "y":
        raise AssertionError("operator did not confirm Core2 speaker output")
