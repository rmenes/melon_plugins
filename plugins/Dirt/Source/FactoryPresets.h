// FactoryPresets.h - Melon Dirt factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace dirt
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Model index: 0 Emerald '79, 1 Emerald Nine, 2 Yellow Sun, 3 Plexi Grump, 4 Gutter Rodent,
    // 5 Hot Rod Brown, 6 Gilded Horse, 7 Dawn Flower, 8 Delta Blues, 9 Obsession, 10 Studio Green
    return {
        { "Emerald Blues",
          { { "model", 0 }, { "drive", 35.0f }, { "tone", 55.0f }, { "level", 60.0f } } },
        { "Nine Boost",
          { { "model", 1 }, { "drive", 15.0f }, { "tone", 60.0f }, { "level", 80.0f } } },
        { "Yellow Crunch",
          { { "model", 2 }, { "drive", 60.0f }, { "tone", 50.0f }, { "level", 55.0f } } },
        { "Plexi in a Box",
          { { "model", 3 }, { "drive", 70.0f }, { "tone", 55.0f }, { "level", 50.0f } } },
        { "Rodent Fuzz",
          { { "model", 4 }, { "drive", 85.0f }, { "tone", 40.0f }, { "level", 50.0f } } },
        { "Hot Rod High Gain",
          { { "model", 5 }, { "drive", 80.0f }, { "tone", 60.0f }, { "level", 50.0f } } },
        { "Golden Clean Boost",
          { { "model", 6 }, { "drive", 20.0f }, { "tone", 55.0f }, { "level", 70.0f } } },
        { "Dawn Edge of Breakup",
          { { "model", 7 }, { "drive", 40.0f }, { "tone", 50.0f }, { "level", 60.0f } } },
        { "Delta Grit",
          { { "model", 8 }, { "drive", 55.0f }, { "tone", 45.0f }, { "level", 55.0f } } },
        { "Obsessive Rock",
          { { "model", 9 }, { "drive", 65.0f }, { "tone", 55.0f }, { "level", 50.0f } } },
    };
}
} // namespace dirt
