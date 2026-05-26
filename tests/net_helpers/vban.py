"""VBAN audio sub-protocol helper.

Builds and parses 28-byte VBAN audio headers per the VB-Audio published
spec. PCM payloads are little-endian (the VBAN convention) so we keep
samples in host-native byte order on the wire when running on a
little-endian host.

This is an independent re-implementation of the same wire format the
C++ side encodes — useful as a cross-check that the C++ implementation
isn't reading its own internally-consistent-but-non-standard output.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import Optional


VBAN_SIGNATURE = b"VBAN"
VBAN_HEADER_BYTES = 28
VBAN_STREAM_NAME_BYTES = 16

# Sub-protocol selectors live in the high 3 bits of byte 4.
VBAN_SUBPROTOCOL_AUDIO = 0x00
VBAN_SUBPROTOCOL_SERIAL = 0x20
VBAN_SUBPROTOCOL_MIDI = 0x40
VBAN_SUBPROTOCOL_SERVICE = 0x60

# Audio sub-codec values in byte 7.
VBAN_SUBCODEC_PCM8U = 0x00
VBAN_SUBCODEC_PCM16 = 0x01
VBAN_SUBCODEC_MULAW = 0x10
VBAN_SUBCODEC_ALAW = 0x11

VBAN_SAMPLE_RATES = (
    6000, 12000, 24000, 48000, 96000, 192000, 384000,
    8000, 16000, 32000, 64000, 128000, 256000, 512000,
    11025, 22050, 44100, 88200, 176400, 352800, 705600,
)


def sample_rate_index(hz: int) -> int:
    """Return the VBAN-table index for `hz`, or raise ValueError."""
    try:
        return VBAN_SAMPLE_RATES.index(hz)
    except ValueError:
        raise ValueError(f"sample rate {hz} Hz is not in the VBAN table")


@dataclass
class VbanAudioHeader:
    sample_rate_index: int = 0
    num_samples: int = 1     # 1..256
    num_channels: int = 1    # 1..256
    sub_codec: int = VBAN_SUBCODEC_PCM16
    stream_name: str = ""
    frame_counter: int = 0


def encode_audio_header(h: VbanAudioHeader) -> bytes:
    if not (0 <= h.sample_rate_index <= 20):
        raise ValueError("sample_rate_index out of range")
    if not (1 <= h.num_samples <= 256):
        raise ValueError("num_samples out of range")
    if not (1 <= h.num_channels <= 256):
        raise ValueError("num_channels out of range")

    name = h.stream_name.encode("ascii")[:VBAN_STREAM_NAME_BYTES]
    name = name.ljust(VBAN_STREAM_NAME_BYTES, b"\x00")

    buf = bytearray(VBAN_HEADER_BYTES)
    buf[0:4] = VBAN_SIGNATURE
    buf[4] = VBAN_SUBPROTOCOL_AUDIO | (h.sample_rate_index & 0x1F)
    buf[5] = h.num_samples - 1
    buf[6] = h.num_channels - 1
    buf[7] = h.sub_codec & 0xFF
    buf[8:24] = name
    buf[24:28] = struct.pack("<I", h.frame_counter & 0xFFFFFFFF)
    return bytes(buf)


def parse_audio_header(data: bytes) -> Optional[VbanAudioHeader]:
    """Return the parsed header, or None if `data` is not a valid
    VBAN audio packet (bad signature, non-audio sub-protocol, or too short).
    """
    if len(data) < VBAN_HEADER_BYTES or data[0:4] != VBAN_SIGNATURE:
        return None
    if (data[4] & 0xE0) != VBAN_SUBPROTOCOL_AUDIO:
        return None
    name = data[8:24].rstrip(b"\x00").decode("ascii", errors="replace")
    return VbanAudioHeader(
        sample_rate_index=data[4] & 0x1F,
        num_samples=data[5] + 1,
        num_channels=data[6] + 1,
        sub_codec=data[7],
        stream_name=name,
        frame_counter=struct.unpack("<I", data[24:28])[0],
    )


def build_audio_packet(header: VbanAudioHeader, pcm_le_bytes: bytes) -> bytes:
    """Compose a complete VBAN audio packet (header + raw PCM payload).

    The payload is taken verbatim — `pcm_le_bytes` must already be in
    the wire's little-endian byte order for the configured sub-codec.
    Use `pcm_to_le_bytes()` to convert a list of int16 samples.
    """
    return encode_audio_header(header) + pcm_le_bytes


def pcm_to_le_bytes(samples: list[int]) -> bytes:
    """Pack a flat list of int16 samples into VBAN little-endian PCM16."""
    return struct.pack(f"<{len(samples)}h", *samples)


def le_bytes_to_pcm(data: bytes) -> list[int]:
    """Inverse of `pcm_to_le_bytes`. `data` length must be even."""
    n = len(data) // 2
    return list(struct.unpack(f"<{n}h", data[: n * 2]))
