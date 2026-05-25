#include "VbanProtocol.h"

#include <string.h>

// PCMFlowUDP :: VBAN sample-rate table.
//
// VBAN defines a fixed 21-entry sample-rate table indexed by the low
// 5 bits of header byte 4 (audio sub-protocol). The values below are
// from the published VBAN specification.

namespace pcmflowudp
{
    const uint32_t kVbanSampleRates[21] = {
        6000,
        12000,
        24000,
        48000,
        96000,
        192000,
        384000,
        8000,
        16000,
        32000,
        64000,
        128000,
        256000,
        512000,
        11025,
        22050,
        44100,
        88200,
        176400,
        352800,
        705600,
    };

    uint8_t vbanSampleRateIndex(uint32_t hz)
    {
        for (uint8_t i = 0; i < 21; ++i)
        {
            if (kVbanSampleRates[i] == hz)
                return i;
        }
        return kVbanSampleRateInvalid;
    }

    uint32_t vbanSampleRateHz(uint8_t index)
    {
        if (index >= 21)
            return 0;
        return kVbanSampleRates[index];
    }

    bool encodeAudioHeader(const VbanAudioHeader &in, uint8_t *out)
    {
        if (out == nullptr)
            return false;
        if (in.sampleRateIndex > 20)
            return false;
        if (in.numSamples < 1 || in.numSamples > 256)
            return false;
        if (in.numChannels < 1 || in.numChannels > 256)
            return false;

        memcpy(out, kVbanSignature, 4);
        out[4] = static_cast<uint8_t>(VbanSubProtocol::Audio) |
                 (in.sampleRateIndex & 0x1F);
        out[5] = static_cast<uint8_t>(in.numSamples - 1);
        out[6] = static_cast<uint8_t>(in.numChannels - 1);
        out[7] = static_cast<uint8_t>(in.subCodec);

        // Stream name is zero-padded; clamp at 16 bytes and don't write
        // a NUL when the name fills the field exactly.
        memset(out + 8, 0, kVbanStreamNameBytes);
        const size_t n = strnlen(in.streamName, kVbanStreamNameBytes);
        if (n > 0)
            memcpy(out + 8, in.streamName, n);

        // frameCounter is little-endian on the wire.
        out[24] = static_cast<uint8_t>(in.frameCounter & 0xFF);
        out[25] = static_cast<uint8_t>((in.frameCounter >> 8) & 0xFF);
        out[26] = static_cast<uint8_t>((in.frameCounter >> 16) & 0xFF);
        out[27] = static_cast<uint8_t>((in.frameCounter >> 24) & 0xFF);
        return true;
    }

    VbanParseResult parseAudioHeader(const uint8_t *in,
                                     size_t len,
                                     VbanAudioHeader &out)
    {
        if (in == nullptr || len < kVbanHeaderBytes)
            return VbanParseResult::TooShort;

        if (memcmp(in, kVbanSignature, 4) != 0)
            return VbanParseResult::BadSignature;

        const uint8_t subProto = in[4] & 0xE0;
        if (subProto != static_cast<uint8_t>(VbanSubProtocol::Audio))
            return VbanParseResult::NotAudio;

        const uint8_t srIndex = in[4] & 0x1F;
        if (srIndex > 20)
            return VbanParseResult::SampleRateInvalid;

        out.sampleRateIndex = srIndex;
        out.numSamples = static_cast<uint16_t>(in[5]) + 1;
        out.numChannels = static_cast<uint16_t>(in[6]) + 1;
        out.subCodec = static_cast<VbanSubCodec>(in[7]);

        memset(out.streamName, 0, sizeof(out.streamName));
        memcpy(out.streamName, in + 8, kVbanStreamNameBytes);
        // Ensure NUL termination even if all 16 bytes are non-zero.
        out.streamName[kVbanStreamNameBytes] = '\0';

        out.frameCounter =
            static_cast<uint32_t>(in[24]) |
            (static_cast<uint32_t>(in[25]) << 8) |
            (static_cast<uint32_t>(in[26]) << 16) |
            (static_cast<uint32_t>(in[27]) << 24);
        return VbanParseResult::Ok;
    }
} // namespace pcmflowudp
