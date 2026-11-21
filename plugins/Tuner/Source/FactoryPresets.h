// FactoryPresets.h - Melon Tuner factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace tuner
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Tuning: 0 Chromatic, 1 Guitar Standard, 2 Guitar Eb Standard, 3 Guitar D Standard, 4 Guitar Drop D,
    //         5 Guitar Drop C, 6 Guitar DADGAD, 7 Guitar Open G, 8 Guitar Open D, 9 Guitar 7-String,
    //         10 Bass Standard, 11 Bass 5-String, 12 Bass 6-String, 13 Bass Drop D, 14 Bass Eb Standard
    // Display: 0 Needle, 1 Strobe
    return {
        { "Guitar Standard", { { "tuning", 1 } } },
        { "Guitar Drop D", { { "tuning", 4 } } },
        { "Guitar Eb Standard", { { "tuning", 2 } } },
        { "Guitar Drop C", { { "tuning", 5 }, { "threshold", -65.0f } } },
        { "Guitar DADGAD", { { "tuning", 6 } } },
        { "Guitar Open G", { { "tuning", 7 } } },
        { "7-String Standard", { { "tuning", 9 }, { "threshold", -65.0f } } },
        { "Bass Standard", { { "tuning", 10 }, { "smoothing", 60.0f } } },
        { "Bass 5-String", { { "tuning", 11 }, { "smoothing", 65.0f }, { "threshold", -65.0f } } },
        { "Silent Stage Strobe",
          { { "tuning", 0 }, { "display", 1 }, { "mute", 1 }, { "smoothing", 30.0f } } },
    };
}
} // namespace tuner
