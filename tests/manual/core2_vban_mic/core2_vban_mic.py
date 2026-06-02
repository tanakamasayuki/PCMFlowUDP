"""
Purpose:
    Verify that Core2 microphone samples are packetized as VBAN PCM16.

Why manual:
    Requires a physical Core2 microphone. The packet format is checked
    automatically; the operator should make a short sound during capture.
"""

import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from net_helpers.raw_udp import RawUdp  # noqa: E402
from net_helpers.vban import (  # noqa: E402
    VBAN_HEADER_BYTES,
    VBAN_SUBCODEC_PCM16,
    le_bytes_to_pcm,
    parse_audio_header,
    sample_rate_index,
)


def _rms(samples: list[int]) -> float:
    if not samples:
        return 0.0
    return (sum(s * s for s in samples) / len(samples)) ** 0.5


def test_core2_vban_mic(dut):
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

    print("\nMake a short sound near the Core2 microphone during capture.")

    packets = []
    with RawUdp(bind_port=0) as udp:
        udp.send(b"start", dut_ip, dut_port)
        dut.expect(
            re.compile(rb"VBAN-TX stream=Core2Mic rate=16000 channels=1 dest=\S+"),
            timeout=10,
        )

        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline and len(packets) < 120:
            received = udp.recv(timeout=0.5)
            if received is None:
                continue
            packets.append(received[0])

    assert len(packets) >= 80, f"got {len(packets)} VBAN packets, expected at least 80"

    last_counter = -1
    max_rms = 0.0
    for data in packets:
        header = parse_audio_header(data)
        assert header is not None, f"unparseable VBAN packet: {data[:32].hex()}"
        assert header.sample_rate_index == sample_rate_index(16000)
        assert header.num_channels == 1
        assert header.num_samples == 256
        assert header.sub_codec == VBAN_SUBCODEC_PCM16
        assert header.stream_name == "Core2Mic"
        assert header.frame_counter > last_counter
        last_counter = header.frame_counter

        samples = le_bytes_to_pcm(data[VBAN_HEADER_BYTES:])
        assert len(samples) == 256
        max_rms = max(max_rms, _rms(samples))

    assert max_rms > 20.0, f"microphone signal looks silent: max_rms={max_rms:.2f}"
    dut.expect(re.compile(rb"VBAN-TX packets=\d+ frames=256"), timeout=5)
