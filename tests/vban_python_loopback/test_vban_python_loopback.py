"""Python → DUT VBAN audio packet interop.

Python constructs a VBAN audio packet from scratch (independent of the
C++ encoder) and sends it to the DUT. The DUT decodes and prints the
metadata plus the first three samples; Python asserts the values match
what it sent.

This is the cross-implementation check: a bug that's symmetric in the
C++ encoder + decoder would pass vban_loopback/ but fail here.
"""

import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from net_helpers.raw_udp import RawUdp                            # noqa: E402
from net_helpers.vban import (                                     # noqa: E402
    VbanAudioHeader,
    VBAN_SUBCODEC_PCM16,
    build_audio_packet,
    pcm_to_le_bytes,
    sample_rate_index,
)


def test_vban_python_to_dut(dut):
    match = dut.expect(re.compile(rb"DUT-READY port=(\d+)"), timeout=20)
    dut_port = int(match.group(1))

    # A short sine ramp so the first three samples are non-zero and
    # easy to read in the assertion line.
    sample_rate = 16000
    n = 64
    samples = [
        int(round(8000 * math.sin(2 * math.pi * 440 * i / sample_rate)))
        for i in range(n)
    ]
    header = VbanAudioHeader(
        sample_rate_index=sample_rate_index(sample_rate),
        num_samples=n,
        num_channels=1,
        sub_codec=VBAN_SUBCODEC_PCM16,
        stream_name="PyTest",
        frame_counter=0,
    )
    packet = build_audio_packet(header, pcm_to_le_bytes(samples))

    with RawUdp(bind_port=0) as udp:
        udp.send(packet, "127.0.0.1", dut_port)
        match = dut.expect(
            re.compile(
                rb"VBAN-RX rate=(\d+) channels=(\d+) frames=(\d+) "
                rb"s0=(-?\d+) s1=(-?\d+) s2=(-?\d+)"
            ),
            timeout=5,
        )

    assert int(match.group(1)) == sample_rate
    assert int(match.group(2)) == 1
    # DUT reads up to its 8-sample buffer; the full 64-sample drain
    # isn't the point of this interop check.
    assert int(match.group(3)) == 8
    assert int(match.group(4)) == samples[0]
    assert int(match.group(5)) == samples[1]
    assert int(match.group(6)) == samples[2]
