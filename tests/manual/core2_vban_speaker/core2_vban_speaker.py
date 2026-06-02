"""
Purpose:
    Verify that Core2 receives VBAN PCM16 and hands it to M5Unified speaker output.

Why manual:
    Packet reception is checked automatically, but audible speaker output
    requires operator confirmation unless an external audio loopback is attached.
"""

import math
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from net_helpers.raw_udp import RawUdp  # noqa: E402
from net_helpers.vban import (  # noqa: E402
    VbanAudioHeader,
    VBAN_SUBCODEC_PCM16,
    build_audio_packet,
    pcm_to_le_bytes,
    sample_rate_index,
)


def test_core2_vban_speaker(dut):
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
    frames = 256
    packets = 320
    with RawUdp(bind_port=0) as udp:
        for seq in range(packets):
            samples = [
                int(round(9000 * math.sin(2 * math.pi * 1000 * i / sample_rate)))
                for i in range(frames)
            ]
            header = VbanAudioHeader(
                sample_rate_index=sample_rate_index(sample_rate),
                num_samples=frames,
                num_channels=1,
                sub_codec=VBAN_SUBCODEC_PCM16,
                stream_name="PyTone",
                frame_counter=seq,
            )
            udp.send(build_audio_packet(header, pcm_to_le_bytes(samples)), dut_ip, dut_port)
            time.sleep(frames / sample_rate)

        rx = dut.expect(
            re.compile(
                rb"VBAN-RX rate=(\d+) channels=(\d+) frames=(\d+) "
                rb"packets=(\d+) drops=(\d+) s0=(-?\d+) s1=(-?\d+) s2=(-?\d+)"
            ),
            timeout=10,
        )

    assert int(rx.group(1)) == sample_rate
    assert int(rx.group(2)) == 1
    assert int(rx.group(3)) == frames
    assert int(rx.group(4)) >= 1
    assert int(rx.group(5)) == 0

    answer = input("Did you hear a 1 kHz tone from the Core2 speaker? [y/n] > ").strip().lower()
    if answer != "y":
        raise AssertionError("operator did not confirm Core2 speaker output")
