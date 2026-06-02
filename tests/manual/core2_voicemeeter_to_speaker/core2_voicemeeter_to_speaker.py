"""
Purpose:
    Verify VB-Audio Voicemeeter -> Core2 VBAN speaker interoperability.

Why manual:
    Requires Windows Voicemeeter configuration and human confirmation of speaker output.
"""

import re


def test_core2_voicemeeter_to_speaker(dut):
    match = dut.expect(
        [
            re.compile(rb"DUT-READY ip=(\S+) port=6980 stream=PcToCore2"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=90,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connection failed: {match.group(1).decode()}")

    core2_ip = match.group(1).decode()
    print(f"\nConfigure Voicemeeter VBAN Out to {core2_ip}:6980.")
    print("Expected stream name: PcToCore2, format: PCM16, 16000 Hz, mono.")
    print("Play a tone or audio file on the PC.")

    dut.expect(
        re.compile(rb"VBAN-RX stream=PcToCore2 rate=16000 channels=1 frames=\d+ packets=\d+ drops=0"),
        timeout=120,
    )

    answer = input("Did you hear the Voicemeeter audio from the Core2 speaker? [y/n] > ").strip().lower()
    if answer != "y":
        raise AssertionError("operator did not confirm Core2 speaker output")
