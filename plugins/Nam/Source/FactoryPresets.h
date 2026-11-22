// FactoryPresets.h - Melon NAM factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace mnam
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Factory presets set the controls only: the loaded model is kept.
    // Bass / Middle / Treble: 0..10, 5 = flat. Gate threshold in dB, Input in dB.
    return {
        { "Flat (EQ Off)",       { { "eq_on", 0 } } },
        { "Tight High Gain",     { { "gate_thr", -60.0f }, { "bass", 4.0f }, { "middle", 6.0f }, { "treble", 6.0f } } },
        { "Scooped Metal",       { { "gate_thr", -58.0f }, { "bass", 7.0f }, { "middle", 2.5f }, { "treble", 7.0f } } },
        { "Warm Blues",          { { "gate_on", 0 }, { "bass", 6.0f }, { "middle", 6.0f }, { "treble", 3.5f } } },
        { "Bright Clean",        { { "gate_on", 0 }, { "bass", 4.5f }, { "treble", 7.0f } } },
        { "Mid Push Lead",       { { "gate_thr", -65.0f }, { "bass", 4.0f }, { "middle", 8.0f }, { "treble", 5.5f } } },
        { "Dark Jazz",           { { "gate_on", 0 }, { "bass", 6.0f }, { "middle", 4.5f }, { "treble", 2.0f } } },
        { "Hot Input",           { { "input", 6.0f }, { "gate_thr", -60.0f } } },
        { "Hot Pickups (-6 dB)", { { "input", -6.0f } } },
        { "Raw Model",           { { "gate_on", 0 }, { "eq_on", 0 }, { "normalize", 0 } } },
    };
}
} // namespace mnam
