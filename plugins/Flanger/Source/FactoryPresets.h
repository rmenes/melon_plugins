// FactoryPresets.h - Melon Flanger factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace flanger
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    return {
        { "Jet Plane",
          { { "manual", 2.5f }, { "width", 80.0f }, { "speed", 0.15f }, { "regen", 75.0f }, { "mix", 50.0f } } },
        { "Classic Sweep",
          { { "manual", 2.0f }, { "width", 60.0f }, { "speed", 0.4f }, { "regen", 50.0f }, { "mix", 50.0f } } },
        { "Metallic Ring",
          { { "manual", 0.6f }, { "width", 30.0f }, { "speed", 0.2f }, { "regen", 90.0f }, { "mix", 50.0f } } },
        { "Near Zero",
          { { "manual", 0.4f }, { "width", 100.0f }, { "speed", 0.12f }, { "regen", 40.0f }, { "mix", 50.0f } } },
        { "Slow Liquid",
          { { "manual", 4.0f }, { "width", 70.0f }, { "speed", 0.08f }, { "regen", 35.0f }, { "mix", 45.0f } } },
        { "Robot Voice",
          { { "manual", 1.2f }, { "width", 0.0f }, { "speed", 0.05f }, { "regen", 85.0f }, { "mix", 50.0f } } },
        { "Fast Warble",
          { { "manual", 3.0f }, { "width", 40.0f }, { "speed", 4.5f }, { "regen", 30.0f }, { "mix", 50.0f } } },
        { "Subtle Motion",
          { { "manual", 3.5f }, { "width", 50.0f }, { "speed", 0.3f }, { "regen", 20.0f }, { "mix", 30.0f } } },
        { "Chorused Flange",
          { { "manual", 7.0f }, { "width", 60.0f }, { "speed", 0.6f }, { "regen", 25.0f }, { "mix", 50.0f } } },
        { "Screaming Jet",
          { { "manual", 1.5f }, { "width", 90.0f }, { "speed", 0.25f }, { "regen", 95.0f }, { "mix", 55.0f } } },
    };
}
} // namespace flanger
