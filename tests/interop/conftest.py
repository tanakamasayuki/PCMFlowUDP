"""Regenerate input/captures.h from captures/*.bin before the sketch
is compiled. We do this at conftest import time (rather than inside a
fixture) so the header exists by the time pytest-embedded invokes
arduino-cli."""

import subprocess
import sys
from pathlib import Path

_here = Path(__file__).resolve().parent
subprocess.run(
    [sys.executable, str(_here / "tools" / "gen_capture_fixtures.py")],
    check=True,
    cwd=_here,
)
