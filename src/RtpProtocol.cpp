#include "RtpProtocol.h"

// PCMFlowUDP :: RTP fixed-header encode / parse.
//
// All multi-byte fields are written / read in network byte order. The
// encoder always emits V=2, P=0, X=0, CC=0 — PCMFlowUDP does not need
// CSRCs or extension headers for its supported flows. The parser is
// lenient: it accepts packets with CSRCs / extensions, reports the
// payload offset via `RtpHeader::payloadOffset`, and lets the caller
// slice the payload from that offset.

namespace pcmflowudp
{
    bool encodeRtpHeader(const RtpHeader &in, uint8_t *out)
    {
        if (out == nullptr)
            return false;
        if (in.payloadType > 0x7F)
            return false;

        // Byte 0: V=2 (forced), P=0 (forced), X=0 (forced), CC=0 (forced).
        out[0] = 0x80; // 10_0_0_0000

        // Byte 1: M bit + 7-bit PT.
        out[1] = static_cast<uint8_t>((in.marker ? 0x80 : 0x00) |
                                      (in.payloadType & 0x7F));

        // Bytes 2..3: sequence number (BE).
        out[2] = static_cast<uint8_t>((in.sequenceNumber >> 8) & 0xFF);
        out[3] = static_cast<uint8_t>(in.sequenceNumber & 0xFF);

        // Bytes 4..7: timestamp (BE).
        out[4] = static_cast<uint8_t>((in.timestamp >> 24) & 0xFF);
        out[5] = static_cast<uint8_t>((in.timestamp >> 16) & 0xFF);
        out[6] = static_cast<uint8_t>((in.timestamp >> 8) & 0xFF);
        out[7] = static_cast<uint8_t>(in.timestamp & 0xFF);

        // Bytes 8..11: SSRC (BE).
        out[8] = static_cast<uint8_t>((in.ssrc >> 24) & 0xFF);
        out[9] = static_cast<uint8_t>((in.ssrc >> 16) & 0xFF);
        out[10] = static_cast<uint8_t>((in.ssrc >> 8) & 0xFF);
        out[11] = static_cast<uint8_t>(in.ssrc & 0xFF);
        return true;
    }

    RtpParseResult parseRtpHeader(const uint8_t *in, size_t len, RtpHeader &out)
    {
        if (in == nullptr || len < kRtpHeaderBytes)
            return RtpParseResult::TooShort;

        out.version = (in[0] >> 6) & 0x03;
        if (out.version != 2)
            return RtpParseResult::BadVersion;

        out.padding = (in[0] & 0x20) != 0;
        out.extension = (in[0] & 0x10) != 0;
        out.csrcCount = in[0] & 0x0F;

        out.marker = (in[1] & 0x80) != 0;
        out.payloadType = in[1] & 0x7F;

        out.sequenceNumber =
            (static_cast<uint16_t>(in[2]) << 8) | in[3];
        out.timestamp =
            (static_cast<uint32_t>(in[4]) << 24) |
            (static_cast<uint32_t>(in[5]) << 16) |
            (static_cast<uint32_t>(in[6]) << 8) |
            static_cast<uint32_t>(in[7]);
        out.ssrc =
            (static_cast<uint32_t>(in[8]) << 24) |
            (static_cast<uint32_t>(in[9]) << 16) |
            (static_cast<uint32_t>(in[10]) << 8) |
            static_cast<uint32_t>(in[11]);

        // CSRCs occupy `csrcCount * 4` bytes after the fixed header.
        size_t offset = kRtpHeaderBytes +
                        static_cast<size_t>(out.csrcCount) * 4u;
        if (len < offset)
            return RtpParseResult::TooShort;

        // Optional header extension (RFC 3550 §5.3.1):
        //   16 bits "defined by profile" + 16 bits length (in 32-bit
        //   words, exclusive of these 4 header bytes).
        if (out.extension)
        {
            if (len < offset + 4)
                return RtpParseResult::TooShort;
            const uint16_t extWords =
                (static_cast<uint16_t>(in[offset + 2]) << 8) | in[offset + 3];
            const size_t extBytes =
                4u + static_cast<size_t>(extWords) * 4u;
            if (len < offset + extBytes)
                return RtpParseResult::TooShort;
            offset += extBytes;
        }

        out.payloadOffset = offset;
        return RtpParseResult::Ok;
    }
} // namespace pcmflowudp
