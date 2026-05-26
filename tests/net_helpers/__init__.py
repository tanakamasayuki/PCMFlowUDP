"""Python network helpers for PCMFlowUDP tests.

These modules provide an independent UDP send / receive surface that
pytest can use to exchange packets with a DUT (the .ino sketch built
for `lang-ship:host` or running on real ESP32 hardware).

Pure-Python: only standard library, no external deps. The helpers
deliberately encode / decode the wire formats themselves (RAW, VBAN,
RTP) so that they serve as an independent cross-check on the C++
implementation in `src/`.
"""
