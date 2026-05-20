#include "VbanProtocol.h"

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
} // namespace pcmflowudp
