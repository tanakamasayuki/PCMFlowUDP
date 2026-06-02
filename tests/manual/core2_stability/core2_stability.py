"""
Purpose:
    Run a sustained Python -> Core2 VBAN stream and check packet, heap, RSSI,
    and Wi-Fi stability statistics.

Why manual:
    Requires a physical Core2 and a real Wi-Fi environment. The run is long
    by default because the AP and RF environment are part of the test.
"""

import math
import os
import re
import sys
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


def _build_packet(seq: int, sample_rate: int = 16000, frames: int = 256) -> bytes:
    samples = [
        int(round(8000 * math.sin(2 * math.pi * 1000 * i / sample_rate)))
        for i in range(frames)
    ]
    header = VbanAudioHeader(
        sample_rate_index=sample_rate_index(sample_rate),
        num_samples=frames,
        num_channels=1,
        sub_codec=VBAN_SUBCODEC_PCM16,
        stream_name="Stable",
        frame_counter=seq,
    )
    return build_audio_packet(header, pcm_to_le_bytes(samples))


def test_core2_stability(dut):
    duration_s = int(os.getenv("CORE2_STABILITY_SECONDS", "1800"))
    sample_rate = 16000
    frames = 256
    interval_s = frames / sample_rate

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

    stats_re = re.compile(
        rb"STABILITY-STAT ms=(\d+) packets=(\d+) bad=(\d+) frames=(\d+) "
        rb"heap=(\d+) minHeap=(\d+) rssi=(-?\d+) wifi=(\d+)"
    )

    sent = 0
    last_stats = None
    deadline = time.monotonic() + duration_s
    next_send = time.monotonic()

    with RawUdp(bind_port=0) as udp:
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_send:
                udp.send(_build_packet(sent, sample_rate, frames), dut_ip, dut_port)
                sent += 1
                next_send += interval_s

            try:
                match = dut.expect(stats_re, timeout=0.05)
                last_stats = match
            except Exception:
                pass

    assert last_stats is not None, "no stability stats received"

    packets = int(last_stats.group(2))
    bad = int(last_stats.group(3))
    frames_seen = int(last_stats.group(4))
    heap = int(last_stats.group(5))
    min_heap = int(last_stats.group(6))
    rssi = int(last_stats.group(7))
    wifi = int(last_stats.group(8))

    assert wifi == 3, f"Wi-Fi not connected at end: status={wifi}"
    assert bad == 0, f"bad packets observed: {bad}"
    assert packets >= int(sent * 0.90), f"received {packets}/{sent} packets"
    assert frames_seen >= packets * frames
    assert heap > 0 and min_heap > 0
    assert min_heap >= int(heap * 0.80), f"heap drift too large: heap={heap} min={min_heap}"
    assert rssi > -90, f"RSSI too weak: {rssi}"

    print(f"\nSTABILITY done sent={sent} received={packets} bad={bad} min_heap={min_heap} rssi={rssi}")
