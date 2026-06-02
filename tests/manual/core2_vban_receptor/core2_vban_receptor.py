"""
Purpose:
    Verify Core2 mic -> VB-Audio VBAN Receptor / Voicemeeter interoperability.

Why manual:
    Requires Windows VBAN software and human confirmation of app UI and audio output.
"""

import re


def test_core2_vban_receptor(dut):
    match = dut.expect(
        [
            re.compile(rb"DUT-READY ip=(\S+) stream=Core2Mic dest=255\.255\.255\.255:6980"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=90,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connection failed: {match.group(1).decode()}")

    print("\nEnable VBAN In on VBAN Receptor or Voicemeeter.")
    print("Expected stream: Core2Mic, 16000 Hz, mono, PCM16, UDP 6980.")
    print("Make a short sound near the Core2 microphone.")

    dut.expect(re.compile(rb"VBAN-TX packets=\d+ frames=256 stream=Core2Mic"), timeout=15)

    answer = input("Does the VBAN app show Core2Mic meter activity and audible mic output? [y/n] > ").strip().lower()
    if answer != "y":
        raise AssertionError("operator did not confirm VBAN Receptor / Voicemeeter receive")
