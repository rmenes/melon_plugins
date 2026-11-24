// FactoryPresets.h - Melon Trem factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>
#include "TremDsp.h"

namespace trem
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Rate in Hz, Wave and Depth in %. Sync: index in syncValues() (0 = Off, 10 = 1/8, 12 = 1/8 T, 13 = 1/16).
    // Stereo: 0 Mono, 1 Pan.
    return {
        { "Surf Shimmer",      { { "rate", 6.0f },  { "wave", 0.0f },   { "depth", 60.0f } } },
        { "Vintage Amp Trem",  { { "rate", 4.5f },  { "wave", 20.0f },  { "depth", 45.0f } } },
        { "Slow Pulse",        { { "rate", 1.5f },  { "wave", 10.0f },  { "depth", 50.0f } } },
        { "Gentle Throb",      { { "rate", 2.5f },  { "wave", 35.0f },  { "depth", 30.0f } } },
        { "Choppy Square",     { { "rate", 6.5f },  { "wave", 90.0f },  { "depth", 90.0f } } },
        { "Helicopter",        { { "rate", 12.0f }, { "wave", 100.0f }, { "depth", 100.0f } } },
        { "Eighth Chop",       { { "sync", (float) sync1_8 },  { "wave", 100.0f }, { "depth", 100.0f } } },
        { "Triplet Swing",     { { "sync", (float) sync1_8T }, { "wave", 60.0f },  { "depth", 75.0f } } },
        { "Stereo Swirl",      { { "rate", 3.0f },  { "wave", 20.0f },  { "depth", 70.0f }, { "stereo", (float) pan } } },
        { "Ambient Pan",       { { "rate", 0.6f },  { "wave", 0.0f },   { "depth", 80.0f }, { "stereo", (float) pan } } },
    };
}
} // namespace trem
