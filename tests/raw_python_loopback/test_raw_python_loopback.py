"""Python ↔ DUT RAW UDP round-trip.

The DUT sketch echoes any datagram back to its sender. Python sends a
known byte sequence, then asserts that the echo matches and that the
DUT also reported the receive over Serial.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from net_helpers.raw_udp import RawUdp  # noqa: E402


def test_raw_python_loopback(dut):
    match = dut.expect(re.compile(rb"DUT-READY port=(\d+)"), timeout=20)
    dut_port = int(match.group(1))

    payload = bytes([0x01, 0x02, 0xFF, 0x55, 0xAA, 0x00, 0x7F, 0x80])
    with RawUdp(bind_port=0) as udp:
        udp.send(payload, "127.0.0.1", dut_port)
        dut.expect(f"RX {len(payload)} bytes", timeout=5)

        received = udp.recv(timeout=3)
        assert received is not None, "no echo from DUT within timeout"
        data, addr = received
        assert data == payload, (
            f"echo mismatch: got {data.hex()} expected {payload.hex()}"
        )
        # The DUT echoes from a separate ephemeral TX socket, not from
        # the RX bind port — only assert that the source IP is local.
        assert addr[0] == "127.0.0.1", f"echo came from unexpected host: {addr}"
