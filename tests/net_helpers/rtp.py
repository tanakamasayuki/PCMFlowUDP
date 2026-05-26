"""RTP fixed-header helper (RFC 3550 §5.1).

Builds and parses 12-byte RTP headers. CSRCs and header extensions are
not generated, but parsing reports the payload offset honoring them so
captures from external tools (gst-rtp, ffmpeg, SIP softphones) round-
trip through the C++ side without manual surgery.

For L16 payload types, PCM samples are big-endian on the wire (RFC 3551
§4.5.11 / §4.5.13); helpers below convert to/from host order.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Optional


RTP_HEADER_BYTES = 12

# RFC 3551 static payload types we recognize by name. Opus is dynamic
# per RFC 7587; the caller picks the PT (usually 96..127).
RTP_PT_PCMU = 0
RTP_PT_PCMA = 8
RTP_PT_G722 = 9
RTP_PT_L16_STEREO = 10
RTP_PT_L16_MONO = 11


@dataclass
class RtpHeader:
    version: int = 2
    padding: bool = False
    extension: bool = False
    csrc_count: int = 0
    marker: bool = False
    payload_type: int = 0
    sequence_number: int = 0
    timestamp: int = 0
    ssrc: int = 0
    # Set by parse_rtp_header(); ignored by encode_rtp_header().
    payload_offset: int = RTP_HEADER_BYTES


def encode_rtp_header(h: RtpHeader) -> bytes:
    """Encode the 12-byte fixed RTP header.

    Always emits V=2, P=0, X=0, CC=0 — PCMFlowUDP's parallel C++ encoder
    has the same restriction.
    """
    if not (0 <= h.payload_type <= 0x7F):
        raise ValueError("payload_type must be 0..127")
    b0 = 0x80  # V=2, P=0, X=0, CC=0
    b1 = (0x80 if h.marker else 0x00) | (h.payload_type & 0x7F)
    return struct.pack(
        ">BBHII",
        b0, b1,
        h.sequence_number & 0xFFFF,
        h.timestamp & 0xFFFFFFFF,
        h.ssrc & 0xFFFFFFFF,
    )


def parse_rtp_header(data: bytes) -> Optional[RtpHeader]:
    if len(data) < RTP_HEADER_BYTES:
        return None
    b0, b1, seq, ts, ssrc = struct.unpack(">BBHII", data[:RTP_HEADER_BYTES])
    version = (b0 >> 6) & 0x03
    if version != 2:
        return None
    h = RtpHeader(
        version=version,
        padding=bool(b0 & 0x20),
        extension=bool(b0 & 0x10),
        csrc_count=b0 & 0x0F,
        marker=bool(b1 & 0x80),
        payload_type=b1 & 0x7F,
        sequence_number=seq,
        timestamp=ts,
        ssrc=ssrc,
    )
    offset = RTP_HEADER_BYTES + h.csrc_count * 4
    if len(data) < offset:
        return None
    if h.extension:
        if len(data) < offset + 4:
            return None
        ext_words = struct.unpack(">H", data[offset + 2:offset + 4])[0]
        ext_bytes = 4 + ext_words * 4
        if len(data) < offset + ext_bytes:
            return None
        offset += ext_bytes
    h.payload_offset = offset
    return h


def build_packet(header: RtpHeader, payload: bytes) -> bytes:
    return encode_rtp_header(header) + payload


def pcm_to_be_bytes(samples: list[int]) -> bytes:
    """Pack a flat list of int16 samples into network-byte-order PCM16
    (the format expected for RTP L16 payload types)."""
    return struct.pack(f">{len(samples)}h", *samples)


def be_bytes_to_pcm(data: bytes) -> list[int]:
    """Inverse of `pcm_to_be_bytes`. `data` length must be even."""
    n = len(data) // 2
    return list(struct.unpack(f">{n}h", data[: n * 2]))
