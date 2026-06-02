"""
Purpose:
    Verify that Core2 microphone samples are packetized as RTP/L16 mono.

Why manual:
    Requires a physical Core2 microphone. The RTP packet format is checked
    automatically; the operator should make a short sound during capture.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from net_helpers.raw_udp import RawUdp  # noqa: E402
from net_helpers.rtp import (  # noqa: E402
    RTP_PT_L16_MONO,
    be_bytes_to_pcm,
    parse_rtp_header,
)


def _rms(samples: list[int]) -> float:
    if not samples:
        return 0.0
    return (sum(s * s for s in samples) / len(samples)) ** 0.5


def test_core2_rtp_mic(dut):
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
            re.compile(rb"RTP-TX pt=11 rate=16000 channels=1 dest=\S+"),
            timeout=10,
        )

        for _ in range(10):
            received = udp.recv(timeout=2)
            if received is None:
                break
            packets.append(received[0])

    assert len(packets) >= 3, f"got {len(packets)} RTP packets, expected at least 3"

    last_seq = None
    last_ts = None
    max_rms = 0.0
    for data in packets:
        header = parse_rtp_header(data)
        assert header is not None, f"unparseable RTP packet: {data[:32].hex()}"
        assert header.payload_type == RTP_PT_L16_MONO
        assert header.ssrc == 0xC02E5002

        if last_seq is not None:
            assert header.sequence_number == (last_seq + 1) & 0xFFFF
            assert header.timestamp == last_ts + 320
        last_seq = header.sequence_number
        last_ts = header.timestamp

        samples = be_bytes_to_pcm(data[header.payload_offset:])
        assert len(samples) == 320
        max_rms = max(max_rms, _rms(samples))

    assert max_rms > 20.0, f"microphone signal looks silent: max_rms={max_rms:.2f}"
    dut.expect(re.compile(rb"RTP-TX packets=\d+ frames=320"), timeout=5)
