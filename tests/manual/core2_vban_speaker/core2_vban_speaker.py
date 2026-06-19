"""
Purpose:
    Verify that Core2 receives VBAN PCM16 and hands it to M5Unified speaker output.

Why manual:
    Packet reception is checked automatically, but audible speaker output
    requires operator confirmation unless an external audio loopback is attached.
"""

import math
import re
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

from net_helpers.raw_udp import RawUdp  # noqa: E402
from net_helpers.vban import (  # noqa: E402
    VbanAudioHeader,
    VBAN_SUBCODEC_PCM16,
    build_audio_packet,
    pcm_to_le_bytes,
    sample_rate_index,
)


def test_core2_vban_speaker(dut):
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

    sample_rate = 16000
    frames = 256
    interval = frames / sample_rate
    wavetable = [
        int(round(9000 * math.sin(2 * math.pi * 1000 * i / sample_rate)))
        for i in range(frames)
    ]

    stop_sender = threading.Event()
    sender_error = []

    def send_tone():
        seq = 0
        next_send = time.monotonic()
        try:
            with RawUdp(bind_port=0) as udp:
                while not stop_sender.is_set():
                    header = VbanAudioHeader(
                        sample_rate_index=sample_rate_index(sample_rate),
                        num_samples=frames,
                        num_channels=1,
                        sub_codec=VBAN_SUBCODEC_PCM16,
                        stream_name="PyTone",
                        frame_counter=seq,
                    )
                    payload = build_audio_packet(header, pcm_to_le_bytes(wavetable))
                    now = time.monotonic()
                    if now < next_send:
                        time.sleep(next_send - now)
                    udp.send(payload, dut_ip, dut_port)
                    next_send += interval
                    seq = (seq + 1) & 0xFFFFFFFF
        except Exception as exc:  # pragma: no cover - manual-test diagnostics
            sender_error.append(exc)

    sender = threading.Thread(target=send_tone, daemon=True)
    sender.start()

    try:
        rx = dut.expect(
            re.compile(
                rb"VBAN-RX rate=(\d+) channels=(\d+) frames=(\d+) "
                rb"packets=(\d+) drops=(\d+) waits=\d+ chunks=\d+ gaps=\d+ "
                rb"s0=(-?\d+) s1=(-?\d+) s2=(-?\d+)"
            ),
            timeout=10,
        )

        assert int(rx.group(1)) == sample_rate
        assert int(rx.group(2)) == 1
        assert int(rx.group(3)) > 0
        assert int(rx.group(4)) >= 1
        assert int(rx.group(5)) == 0

        answer = input("Did you hear a continuous 1 kHz tone from the Core2 speaker? [y/n] > ").strip().lower()
    finally:
        stop_sender.set()
        sender.join(timeout=2)

    if sender_error:
        raise AssertionError(f"VBAN sender failed: {sender_error[0]}")
    if answer != "y":
        raise AssertionError("operator did not confirm Core2 speaker output")
