"""End-to-end test for RawUdpSink + RawUdpStream over loopback UDP."""

import re


def test_raw_loopback(dut):
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=30)
    passed = int(match.group(1))
    total = int(match.group(2))
    assert passed == total, f"{passed}/{total} EXPECTs passed (see PASS/FAIL lines above)"
