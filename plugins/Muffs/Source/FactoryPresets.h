// FactoryPresets.h - Melon Muffs factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace muffs
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Version index: 0 Triangle, 1 Ram's Head, 2 Op-Amp, 3 Russian Civil War, 4 NYC, 5 Big Muff Pi 2
    return {
        { "Triangle Sustain",
          { { "model", 0 }, { "volume", 50.0f }, { "tone", 50.0f }, { "sustain", 70.0f } } },
        { "Ram's Head Lead",
          { { "model", 1 }, { "volume", 50.0f }, { "tone", 60.0f }, { "sustain", 75.0f } } },
        { "Op-Amp Grind",
          { { "model", 2 }, { "volume", 50.0f }, { "tone", 45.0f }, { "sustain", 65.0f } } },
        { "Russian Doom",
          { { "model", 3 }, { "volume", 50.0f }, { "tone", 35.0f }, { "sustain", 90.0f } } },
        { "NYC Wall",
          { { "model", 4 }, { "volume", 50.0f }, { "tone", 50.0f }, { "sustain", 80.0f } } },
        { "Pi 2 Modern",
          { { "model", 5 }, { "volume", 50.0f }, { "tone", 55.0f }, { "sustain", 70.0f } } },
        { "Scooped Stoner",
          { { "model", 3 }, { "volume", 50.0f }, { "tone", 25.0f }, { "sustain", 100.0f } } },
        { "Bright Singing Lead",
          { { "model", 1 }, { "volume", 50.0f }, { "tone", 75.0f }, { "sustain", 85.0f } } },
        { "Parallel Bass Fuzz",
          { { "model", 3 }, { "volume", 50.0f }, { "tone", 40.0f }, { "sustain", 80.0f }, { "mix", 55.0f } } },
        { "Low Gain Muff",
          { { "model", 0 }, { "volume", 55.0f }, { "tone", 55.0f }, { "sustain", 30.0f } } },
    };
}
} // namespace muffs
