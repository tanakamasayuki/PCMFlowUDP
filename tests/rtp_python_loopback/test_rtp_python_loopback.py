"""Python → DUT RTP L16-mono packet interop.

Python builds an RTP L16-mono packet (PT 11) with a known sine ramp
and sends it to the DUT. The DUT decodes via RtpReceiver and prints
the parsed header fields plus the first three samples; Python asserts
the values are exact.

Cross-implementation check: independent of the C++ encoder.
"""

import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from net_helpers.raw_udp import RawUdp                            # noqa: E402
from net_helpers.rtp import (                                      # noqa: E402
    RTP_PT_L16_MONO,
    RtpHeader,
    build_packet,
    pcm_to_be_bytes,
)


def test_rtp_python_to_dut(dut):
    match = dut.expect(
        re.compile(rb"DUT-READY ip=(\S+) port=(\d+)"), timeout=90
    )
    dut_ip = match.group(1).decode()
    dut_port = int(match.group(2))

    sample_rate = 8000
    n = 64
    samples = [
        int(round(8000 * math.sin(2 * math.pi * 440 * i / sample_rate)))
        for i in range(n)
    ]
    header = RtpHeader(
        marker=True,
        payload_type=RTP_PT_L16_MONO,
        sequence_number=0xABCD,
        timestamp=0x10000,
        ssrc=0xDEADBEEF,
    )
    packet = build_packet(header, pcm_to_be_bytes(samples))

    with RawUdp(bind_port=0) as udp:
        udp.send(packet, dut_ip, dut_port)
        match = dut.expect(
            re.compile(
                rb"RTP-RX pt=(\d+) seq=(\d+) ssrc=(\d+) frames=(\d+) "
                rb"s0=(-?\d+) s1=(-?\d+) s2=(-?\d+)"
            ),
            timeout=10,
        )

    assert int(match.group(1)) == RTP_PT_L16_MONO
    assert int(match.group(2)) == 0xABCD
    assert int(match.group(3)) == 0xDEADBEEF
    # The DUT reads up to its small 8-sample buffer; it doesn't have to
    # drain the whole 64-sample packet for this interop check.
    assert int(match.group(4)) == 8
    assert int(match.group(5)) == samples[0]
    assert int(match.group(6)) == samples[1]
    assert int(match.group(7)) == samples[2]
