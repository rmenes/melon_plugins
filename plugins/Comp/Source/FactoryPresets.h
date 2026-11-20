// FactoryPresets.h - Melon Comp factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace comp
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Pickup: 0 Single Coil, 1 Humbucker
    return {
        { "Country Squish",
          { { "sustain", 75.0f }, { "level", 60.0f }, { "blend", 90.0f }, { "tone", 60.0f }, { "pickup", 0 } } },
        { "Transparent Sustain",
          { { "sustain", 45.0f }, { "level", 55.0f }, { "blend", 50.0f }, { "tone", 50.0f }, { "pickup", 0 } } },
        { "Funk Snap",
          { { "sustain", 60.0f }, { "level", 60.0f }, { "blend", 70.0f }, { "tone", 65.0f }, { "pickup", 0 } } },
        { "Humbucker Smoother",
          { { "sustain", 50.0f }, { "level", 55.0f }, { "blend", 70.0f }, { "tone", 55.0f }, { "pickup", 1 } } },
        { "Clean Boost Comp",
          { { "sustain", 30.0f }, { "level", 75.0f }, { "blend", 60.0f }, { "tone", 55.0f }, { "pickup", 0 } } },
        { "Lead Sustainer",
          { { "sustain", 90.0f }, { "level", 55.0f }, { "blend", 85.0f }, { "tone", 50.0f }, { "pickup", 1 } } },
        { "Parallel Glue",
          { { "sustain", 55.0f }, { "level", 55.0f }, { "blend", 35.0f }, { "tone", 50.0f }, { "pickup", 0 } } },
        { "Dark Jazz",
          { { "sustain", 40.0f }, { "level", 55.0f }, { "blend", 60.0f }, { "tone", 30.0f }, { "pickup", 1 } } },
        { "Chicken Pickin'",
          { { "sustain", 80.0f }, { "level", 60.0f }, { "blend", 100.0f }, { "tone", 70.0f }, { "pickup", 0 } } },
        { "Acoustic Even",
          { { "sustain", 35.0f }, { "level", 55.0f }, { "blend", 45.0f }, { "tone", 55.0f }, { "pickup", 0 } } },
    };
}
} // namespace comp
