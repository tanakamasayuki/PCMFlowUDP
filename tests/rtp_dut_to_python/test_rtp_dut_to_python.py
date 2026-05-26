"""DUT → Python RTP L16-mono packet interop.

The DUT encodes 64 frames of a known PCM ramp into one RTP packet
(PT 11, L16 mono) with a known SSRC and starting timestamp/sequence.
Python parses with an independent RTP implementation and asserts every
header field plus a byte-for-byte payload match (after BE→host
conversion).
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from net_helpers.raw_udp import RawUdp                                # noqa: E402
from net_helpers.rtp import (                                          # noqa: E402
    RTP_PT_L16_MONO,
    be_bytes_to_pcm,
    parse_rtp_header,
)


def test_rtp_dut_to_python(dut):
    match = dut.expect(
        re.compile(rb"DUT-READY ip=(\S+) port=(\d+)"), timeout=90
    )
    dut_ip = match.group(1).decode()
    dut_port = int(match.group(2))

    with RawUdp(bind_port=0) as udp:
        udp.send(b"hello", dut_ip, dut_port)
        dut.expect("DUT-TX-OK", timeout=15)

        received = udp.recv(timeout=5)
        assert received is not None, "no RTP packet from DUT"
        data, _addr = received

    header = parse_rtp_header(data)
    assert header is not None, f"unparseable RTP packet: {data[:32].hex()}"
    assert header.version == 2
    assert header.payload_type == RTP_PT_L16_MONO
    assert header.ssrc == 0x13572468
    assert header.sequence_number == 0x2222
    assert header.timestamp == 0x10000000
    assert header.marker is True  # first packet after begin()
    assert header.payload_offset == 12

    samples = be_bytes_to_pcm(data[header.payload_offset:])
    assert len(samples) == 64
    for i, s in enumerate(samples):
        expected = i * 100 - 3000
        assert s == expected, f"sample {i}: got {s}, expected {expected}"
