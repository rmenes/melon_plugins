// FactoryPresets.h - Melon Simple Gate factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace sgate
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Instrument: 0 Guitar, 1 Bass. Threshold in dB, Hold and Release in ms.
    return {
        { "High-Gain Rhythm",   { { "instrument", 0 }, { "threshold", -50.0f }, { "hold", 20.0f },  { "release", 60.0f } } },
        { "Djent Staccato",     { { "instrument", 0 }, { "threshold", -42.0f }, { "hold", 5.0f },   { "release", 25.0f } } },
        { "Lead Sustain",       { { "instrument", 0 }, { "threshold", -68.0f }, { "hold", 80.0f },  { "release", 400.0f } } },
        { "Clean Hum Cut",      { { "instrument", 0 }, { "threshold", -72.0f }, { "hold", 50.0f },  { "release", 250.0f } } },
        { "Fuzz Tamer",         { { "instrument", 0 }, { "threshold", -55.0f }, { "hold", 30.0f },  { "release", 120.0f } } },
        { "Doom Ring-Out",      { { "instrument", 0 }, { "threshold", -60.0f }, { "hold", 120.0f }, { "release", 600.0f } } },
        { "Bass Pick Tight",    { { "instrument", 1 }, { "threshold", -50.0f }, { "hold", 30.0f },  { "release", 80.0f } } },
        { "Bass Fingerstyle",   { { "instrument", 1 }, { "threshold", -62.0f }, { "hold", 60.0f },  { "release", 200.0f } } },
        { "Bass Drive Gate",    { { "instrument", 1 }, { "threshold", -48.0f }, { "hold", 25.0f },  { "release", 70.0f } } },
        { "Slap & Pop",         { { "instrument", 1 }, { "threshold", -52.0f }, { "hold", 15.0f },  { "release", 50.0f } } },
    };
}
} // namespace sgate
