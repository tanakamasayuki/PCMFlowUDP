"""Python ↔ DUT RAW UDP round-trip.

Works on both `host` and `esp32` profiles: the DUT reports its local
IP (127.0.0.1 on host, WiFi IP on ESP32) so the Python side can target
it without hard-coding the address.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from net_helpers.raw_udp import RawUdp  # noqa: E402


def test_raw_python_loopback(dut):
    match = dut.expect(
        re.compile(rb"DUT-READY ip=(\S+) port=(\d+)"), timeout=90
    )
    dut_ip = match.group(1).decode()
    dut_port = int(match.group(2))

    payload = bytes([0x01, 0x02, 0xFF, 0x55, 0xAA, 0x00, 0x7F, 0x80])
    with RawUdp(bind_port=0) as udp:
        udp.send(payload, dut_ip, dut_port)
        dut.expect(f"RX {len(payload)} bytes", timeout=10)

        received = udp.recv(timeout=5)
        assert received is not None, "no echo from DUT within timeout"
        data, _ = received
        assert data == payload, (
            f"echo mismatch: got {data.hex()} expected {payload.hex()}"
        )
