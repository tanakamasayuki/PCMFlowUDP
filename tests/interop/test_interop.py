"""External-capture interop test.

Drop `.bin` files (one UDP payload per file) into `captures/` and the
matching parser is exercised. See `captures/README.md` for the naming
convention and capture recipes.

With zero captures the test skips so a fresh checkout doesn't fail
before you've had a chance to record any.
"""

import re

import pytest


def test_interop_captures(dut):
    match = dut.expect(re.compile(rb"INTEROP captures=(\d+)"), timeout=20)
    count = int(match.group(1))
    if count == 0:
        pytest.skip(
            "No interop captures yet — see "
            "tests/interop/captures/README.md for how to add them."
        )

    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=30)
    passed = int(match.group(1))
    total = int(match.group(2))
    assert passed == total, (
        f"{passed}/{total} interop captures parsed OK "
        f"(see PASS/FAIL lines above)"
    )
