"""DUT → Python VBAN audio packet interop.

The DUT encodes 64 frames of a known PCM ramp into a VBAN audio packet
and sends it to the Python test. Python parses the wire bytes with an
independent implementation and asserts every header field plus a
byte-for-byte payload match.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from net_helpers.raw_udp import RawUdp                              # noqa: E402
from net_helpers.vban import (                                       # noqa: E402
    VBAN_HEADER_BYTES,
    VBAN_SUBCODEC_PCM16,
    le_bytes_to_pcm,
    parse_audio_header,
    sample_rate_index,
)


def test_vban_dut_to_python(dut):
    match = dut.expect(
        re.compile(rb"DUT-READY ip=(\S+) port=(\d+)"), timeout=90
    )
    dut_ip = match.group(1).decode()
    dut_port = int(match.group(2))

    with RawUdp(bind_port=0) as udp:
        # Hello packet — the contents don't matter, only the source IP/port,
        # which the DUT learns from remoteIP() / remotePort().
        udp.send(b"hello", dut_ip, dut_port)

        dut.expect("DUT-TX-OK", timeout=15)

        received = udp.recv(timeout=5)
        assert received is not None, "no VBAN packet from DUT"
        data, _addr = received

    header = parse_audio_header(data)
    assert header is not None, f"unparseable VBAN packet: {data[:32].hex()}"
    assert header.sample_rate_index == sample_rate_index(16000)
    assert header.num_channels == 1
    assert header.num_samples == 64
    assert header.sub_codec == VBAN_SUBCODEC_PCM16
    assert header.stream_name == "DUT-OUT"

    # Frame counter must be 0 on the first emitted packet.
    assert header.frame_counter == 0

    # Verify payload byte-for-byte against the DUT's deterministic ramp.
    samples = le_bytes_to_pcm(data[VBAN_HEADER_BYTES:])
    assert len(samples) == 64, f"got {len(samples)} samples, expected 64"
    for i, s in enumerate(samples):
        expected = i * 100 - 3000
        assert s == expected, f"sample {i}: got {s}, expected {expected}"
