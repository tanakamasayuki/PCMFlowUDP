#ifndef PCMFLOWUDP_VBANPROTOCOL_H
#define PCMFLOWUDP_VBANPROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// PCMFlowUDP :: VBAN wire-protocol constants
//
// Constants and enums describing the portions of the VBAN protocol that
// PCMFlowUDP interoperates with. This is a subset of the full VBAN
// specification (audio sub-protocol + minimal service ping); see SPEC
// §6 for the full coverage matrix.
//
// VBAN is a protocol developed by VB-Audio Software. The constants and
// names below describe the on-the-wire format and are used here in a
// descriptive sense; see SPEC §14 for the trademark notice.

namespace pcmflowudp
{
    // VBAN packet header is exactly 28 bytes. The first 4 bytes are the
    // ASCII signature "VBAN"; the remaining 24 bytes encode protocol,
    // sub-codec, sample-rate index, channel count, frame counter, and a
    // 16-byte stream name.
    static constexpr size_t kVbanHeaderBytes = 28;
    static constexpr size_t kVbanMaxPayloadBytes = 1408;
    static constexpr size_t kVbanMaxPacketBytes =
        kVbanHeaderBytes + kVbanMaxPayloadBytes;
    static constexpr size_t kVbanStreamNameBytes = 16;
    static constexpr uint8_t kVbanSignature[4] = {'V', 'B', 'A', 'N'};

    // High 3 bits of header byte 4 select the sub-protocol.
    // PCMFlowUDP implements only Audio (0x00) and a minimal Service
    // (0x60) responder.
    enum class VbanSubProtocol : uint8_t
    {
        Audio = 0x00,
        Serial = 0x20,  // not implemented
        MIDI = 0x40,    // not implemented
        Service = 0x60, // ping / identification only
    };

    // For audio sub-protocol: low 5 bits of header byte 7 select the
    // sample-format / codec on the wire.
    enum class VbanSubCodec : uint8_t
    {
        PCM8U = 0x00,
        PCM16 = 0x01,   // primary path
        PCM24 = 0x02,   // not implemented in v0.1.x
        PCM32 = 0x03,   // not implemented in v0.1.x
        Float32 = 0x04, // not implemented in v0.1.x
        // 0x05..0x0F reserved / vendor-specific
        MuLaw = 0x10,
        ALaw = 0x11,
        // others omitted
    };

    // VBAN's 21-entry sample-rate table. Header byte 4 low 5 bits index
    // into this. Definitions are part of the published VBAN spec.
    extern const uint32_t kVbanSampleRates[21];
    static constexpr uint8_t kVbanSampleRateInvalid = 0xFF;

    // Look up the table index for a given sample rate. Returns
    // kVbanSampleRateInvalid if the rate is not in the table.
    uint8_t vbanSampleRateIndex(uint32_t hz);

    // Inverse lookup.
    uint32_t vbanSampleRateHz(uint8_t index);

    // -------------------------------------------------------------------
    // VBAN audio-sub-protocol header (28 bytes).
    //
    // Wire layout (little-endian where multi-byte):
    //   [0..3]   'V','B','A','N'         (signature)
    //   [4]      (subProtocol << 5) | (sampleRateIndex & 0x1F)
    //   [5]      numSamples  - 1         (per channel; wire value 0..255 == 1..256)
    //   [6]      numChannels - 1         (wire value 0..255 == 1..256)
    //   [7]      subCodec                (PCM16 = 0x01, MuLaw = 0x10, etc.)
    //   [8..23]  streamName              (zero-padded ASCII, 16 bytes)
    //   [24..27] frameCounter            (uint32_t little-endian)
    //
    // On parse, numSamples / numChannels are returned as the application
    // values (1..256), not the wire's "minus one" form.
    struct VbanAudioHeader
    {
        uint8_t sampleRateIndex = 0; // 0..20
        uint16_t numSamples = 1;     // 1..256
        uint16_t numChannels = 1;    // 1..256
        VbanSubCodec subCodec = VbanSubCodec::PCM16;
        char streamName[kVbanStreamNameBytes + 1] = {0};
        uint32_t frameCounter = 0;
    };

    enum class VbanParseResult : uint8_t
    {
        Ok,
        TooShort,          // input shorter than kVbanHeaderBytes
        BadSignature,      // first 4 bytes are not 'V','B','A','N'
        NotAudio,          // valid VBAN but a non-audio sub-protocol
        SampleRateInvalid, // sample-rate index outside the 21-entry table
    };

    // Pack a VbanAudioHeader into the first 28 bytes of `out`. Returns
    // false if any field is out of range (numSamples / numChannels must
    // be 1..256; sampleRateIndex must be 0..20).
    bool encodeAudioHeader(const VbanAudioHeader &in, uint8_t *out);

    // Parse the first 28 bytes of `in` into a VbanAudioHeader. Only
    // succeeds for valid Audio-sub-protocol packets with a known sample
    // rate; the caller should look at the result code to distinguish
    // "drop silently" (NotAudio) from "log and drop" (BadSignature etc.).
    VbanParseResult parseAudioHeader(const uint8_t *in,
                                     size_t len,
                                     VbanAudioHeader &out);
} // namespace pcmflowudp

#endif // PCMFLOWUDP_VBANPROTOCOL_H
