// FactoryPresets.h - Melon Cab factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace cab
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Factory presets set the controls only: the loaded IR is kept.
    // Low Cut 20 Hz = off, High Cut 20000 Hz = off. Mix in %.
    return {
        { "Full Range",       { { "lowcut", 20.0f }, { "highcut", 20000.0f } } },
        { "Tight Low End",    { { "lowcut", 90.0f } } },
        { "Modern Metal",     { { "lowcut", 80.0f }, { "highcut", 8500.0f } } },
        { "Smooth Top",       { { "highcut", 6500.0f } } },
        { "Vintage Dark",     { { "lowcut", 70.0f }, { "highcut", 5000.0f } } },
        { "Bright & Open",    { { "lowcut", 60.0f }, { "highcut", 16000.0f } } },
        { "Mix Ready",        { { "lowcut", 100.0f }, { "highcut", 9000.0f } } },
        { "Lo-Fi Radio",      { { "lowcut", 300.0f }, { "highcut", 3000.0f } } },
        { "Half Blend",       { { "mix", 50.0f } } },
        { "Bass Cabinet",     { { "lowcut", 30.0f }, { "highcut", 5000.0f } } },
    };
}
} // namespace cab
