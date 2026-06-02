"""
Purpose:
    Verify the Core2 real Wi-Fi RAW UDP path without involving audio hardware.

Why manual:
    Requires a physical M5Stack Core2 and a real Wi-Fi network. The test
    itself is automated after the board is connected and flashed.

Required hardware:
    - M5Stack Core2
    - USB-C cable
    - 2.4 GHz Wi-Fi AP

Setup:
    1. Set the Core2 serial port and WIFI_SSID / WIFI_PASSWORD in `.env`.
    2. Run: uv run --env-file .env pytest manual/core2_raw_udp_ping/core2_raw_udp_ping.py -v -s --profile m5stack_core2
"""

import re
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from net_helpers.raw_udp import RawUdp  # noqa: E402


def test_core2_raw_udp_ping(dut):
    """
    Expected pass:
        - DUT prints its Wi-Fi IP and UDP port.
        - Python sends a 64-byte payload.
        - DUT reports the same length and CRC32.
        - Python receives the same payload back as an ACK.
    """
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

    payload = bytes((i * 37 + 11) & 0xFF for i in range(64))
    expected_crc = zlib.crc32(payload) & 0xFFFFFFFF

    with RawUdp(bind_port=0) as udp:
        udp.send(payload, dut_ip, dut_port)

        rx = dut.expect(
            re.compile(rb"RAW-RX len=(\d+) crc=([0-9A-Fa-f]+) peer=(\S+)"),
            timeout=10,
        )
        assert int(rx.group(1)) == len(payload)
        assert int(rx.group(2), 16) == expected_crc

        received = udp.recv(timeout=5)
        assert received is not None, "no ACK from DUT within timeout"
        data, _ = received
        assert data == payload, (
            f"ACK mismatch: got {data.hex()} expected {payload.hex()}"
        )

        dut.expect("RAW-ACK sent", timeout=5)
