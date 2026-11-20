// FactoryPresets.h - Melon Gates of Doom factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace gate
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Mode: 0 Classic, 1 Clean, 2 Guitar, 3 Ducking. Ratio 100 = full gate.
    return {
        { "Guitar Noise Killer",
          { { "mode", 2 }, { "threshold", -50.0f }, { "range", 80.0f }, { "attack", 0.5f }, { "hold", 20.0f }, { "release", 120.0f } } },
        { "High-Gain Tight",
          { { "mode", 2 }, { "threshold", -40.0f }, { "range", 100.0f }, { "attack", 0.1f }, { "hold", 10.0f }, { "release", 60.0f }, { "lookahead", 2.0f } } },
        { "Smooth Expander",
          { { "mode", 1 }, { "threshold", -45.0f }, { "range", 25.0f }, { "ratio", 2.0f }, { "knee", 12.0f }, { "release", 300.0f } } },
        { "Drum Gate Tight",
          { { "mode", 0 }, { "threshold", -30.0f }, { "range", 60.0f }, { "attack", 0.05f }, { "hold", 40.0f }, { "release", 80.0f }, { "lookahead", 2.0f }, { "schp", 80.0f } } },
        { "Kick Isolator",
          { { "mode", 0 }, { "threshold", -25.0f }, { "range", 80.0f }, { "attack", 0.1f }, { "hold", 30.0f }, { "release", 100.0f }, { "lookahead", 3.0f }, { "schp", 30.0f }, { "sclp", 1000.0f } } },
        { "Snare Focus",
          { { "mode", 0 }, { "threshold", -28.0f }, { "range", 70.0f }, { "attack", 0.1f }, { "hold", 50.0f }, { "release", 150.0f }, { "lookahead", 2.0f }, { "schp", 400.0f }, { "sclp", 6000.0f } } },
        { "Vocal Cleanup",
          { { "mode", 1 }, { "threshold", -50.0f }, { "range", 15.0f }, { "ratio", 3.0f }, { "knee", 10.0f }, { "attack", 5.0f }, { "hold", 80.0f }, { "release", 250.0f } } },
        { "Ducking Pump",
          { { "mode", 3 }, { "threshold", -30.0f }, { "range", 12.0f }, { "attack", 5.0f }, { "hold", 50.0f }, { "release", 300.0f } } },
        { "Gated Reverb Chop",
          { { "mode", 0 }, { "threshold", -35.0f }, { "range", 100.0f }, { "attack", 0.1f }, { "hold", 180.0f }, { "release", 20.0f } } },
        { "Gentle Room Gate",
          { { "mode", 1 }, { "threshold", -55.0f }, { "range", 12.0f }, { "ratio", 1.8f }, { "knee", 15.0f }, { "release", 500.0f } } },
    };
}
} // namespace gate
