"""End-to-end test for VbanSender + VbanReceiver over loopback UDP."""

import re


def test_vban_loopback(dut):
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=60)
    passed = int(match.group(1))
    total = int(match.group(2))
    assert passed == total, f"{passed}/{total} EXPECTs passed (see PASS/FAIL lines above)"
