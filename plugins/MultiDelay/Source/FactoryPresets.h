// FactoryPresets.h - Melon Multi Delay factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>
#include "MultiDelayDsp.h"

namespace mdelay
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Machine: 0 Digital, 1 Tape, 2 Analog, 3 Dual, 4 Pattern, 5 Reverse, 6 Ice, 7 Duck, 8 Swell,
    // 9 Trem, 10 Filter, 11 Lo-Fi. Param 1 / 2 depend on the machine; stepped ones use stepPercent().
    // Sync: 0 Off, 5 1/4, 9 1/8 D (without a host tempo, Time is used).
    return {
        { "Studio Slapback",
          { { "machine", tape }, { "time", 110.0f }, { "repeats", 12.0f }, { "mix", 35.0f }, { "filter", 70.0f }, { "grit", 20.0f },
            { "speed", 25.0f }, { "depth", 8.0f }, { "param1", 35.0f }, { "param2", 25.0f } } },
        { "Warm Analog Lead",
          { { "machine", analog }, { "time", 420.0f }, { "repeats", 40.0f }, { "mix", 30.0f }, { "filter", 100.0f },
            { "speed", 25.0f }, { "depth", 20.0f }, { "param1", 35.0f }, { "param2", 10.0f } } },
        { "Dotted Eighth Shimmer",
          { { "machine", digital }, { "time", 375.0f }, { "sync", 9.0f }, { "repeats", 38.0f }, { "mix", 38.0f }, { "filter", 85.0f },
            { "speed", 30.0f }, { "depth", 12.0f }, { "param1", 25.0f }, { "param2", 25.0f } } },
        { "Wide Dual Stereo",
          { { "machine", dual }, { "time", 320.0f }, { "repeats", 40.0f }, { "mix", 35.0f }, { "filter", 80.0f }, { "depth", 10.0f },
            { "param1", stepPercent (4, 9) }, { "param2", stepPercent (parallel, kNumRoutings) } } },
        { "Gallop Rhythm",
          { { "machine", pattern }, { "time", 500.0f }, { "sync", 5.0f }, { "repeats", 30.0f }, { "mix", 35.0f }, { "filter", 75.0f },
            { "param1", stepPercent (1, 8) }, { "param2", 70.0f } } },
        { "Backwards Dream",
          { { "machine", reverse }, { "time", 650.0f }, { "repeats", 30.0f }, { "mix", 50.0f }, { "filter", 75.0f },
            { "depth", 15.0f }, { "param1", 70.0f }, { "param2", 0.0f } } },
        { "Crystal Octaves",
          { { "machine", ice }, { "time", 450.0f }, { "repeats", 45.0f }, { "mix", 40.0f }, { "filter", 80.0f },
            { "depth", 10.0f }, { "param1", stepPercent (4, 7) }, { "param2", 60.0f } } },
        { "Ducked Vocal Echo",
          { { "machine", duck }, { "time", 350.0f }, { "repeats", 45.0f }, { "mix", 40.0f }, { "filter", 75.0f },
            { "param1", 60.0f }, { "param2", 40.0f } } },
        { "Violin Swell Pad",
          { { "machine", swell }, { "time", 700.0f }, { "repeats", 60.0f }, { "mix", 45.0f }, { "filter", 70.0f },
            { "speed", 20.0f }, { "depth", 25.0f }, { "param1", 65.0f }, { "param2", 100.0f } } },
        { "Broken Radio",
          { { "machine", lofi }, { "time", 280.0f }, { "repeats", 45.0f }, { "mix", 40.0f }, { "filter", 90.0f }, { "grit", 40.0f },
            { "param1", 45.0f }, { "param2", 40.0f } } },
    };
}
} // namespace mdelay
