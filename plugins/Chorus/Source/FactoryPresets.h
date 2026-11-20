// FactoryPresets.h - Melon Chorus factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace chorus
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    return {
        { "Silky Clean",
          { { "rate", 0.6f }, { "depth", 40.0f }, { "width", 80.0f }, { "mix", 45.0f }, { "tone", 10.0f } } },
        { "Eighties Shimmer",
          { { "rate", 1.1f }, { "depth", 65.0f }, { "width", 100.0f }, { "mix", 55.0f }, { "tone", 35.0f } } },
        { "Subtle Doubler",
          { { "rate", 0.25f }, { "depth", 20.0f }, { "width", 90.0f }, { "mix", 35.0f }, { "tone", 0.0f } } },
        { "Deep Swirl",
          { { "rate", 0.35f }, { "depth", 90.0f }, { "width", 100.0f }, { "mix", 50.0f }, { "tone", -10.0f } } },
        { "Watery Lead",
          { { "rate", 1.6f }, { "depth", 70.0f }, { "width", 60.0f }, { "mix", 50.0f }, { "tone", 20.0f } } },
        { "Vibrato",
          { { "rate", 5.0f }, { "depth", 60.0f }, { "width", 0.0f }, { "mix", 100.0f }, { "tone", 0.0f } } },
        { "Rotary Fast",
          { { "rate", 6.5f }, { "depth", 45.0f }, { "width", 100.0f }, { "mix", 60.0f }, { "tone", 15.0f } } },
        { "Dark Bass Chorus",
          { { "rate", 0.7f }, { "depth", 45.0f }, { "width", 30.0f }, { "mix", 40.0f }, { "tone", -60.0f } } },
        { "Wide Pad",
          { { "rate", 0.2f }, { "depth", 75.0f }, { "width", 100.0f }, { "mix", 60.0f }, { "tone", -20.0f } } },
        { "Seasick",
          { { "rate", 2.8f }, { "depth", 100.0f }, { "width", 80.0f }, { "mix", 70.0f }, { "tone", 0.0f } } },
    };
}
} // namespace chorus
