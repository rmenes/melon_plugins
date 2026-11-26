// FactoryPresets.h - Melon Monolithic Synthesis factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>
#include "Patches.h"

namespace monolithic
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    using namespace melon::mono;
    // mode: index in getModeInfos() (0 LEAD ... 11 ARPEGGIO); sound: 0-5 (shown as 1-6).
    // param1-3, mix, sensitivity in %; effect in dB.
    return {
        { "Saw Lead",          { { "mode", (float) Lead },     { "sound", 0 }, { "param1", 55.0f }, { "param3", 55.0f } } },
        { "Sync Scream",       { { "mode", (float) Lead },     { "sound", 2 }, { "param1", 65.0f } } },
        { "Pad Under Guitar",  { { "mode", (float) Pad },      { "sound", 0 }, { "param1", 60.0f }, { "mix", 45.0f } } },
        { "Ensemble Strings",  { { "mode", (float) String },   { "sound", 0 } } },
        { "Crystal Bells",     { { "mode", (float) Bell },     { "sound", 0 }, { "param1", 60.0f }, { "mix", 75.0f } } },
        { "Rock Organ",        { { "mode", (float) Organ },    { "sound", 1 }, { "param2", 60.0f } } },
        { "Acid Bass",         { { "mode", (float) Bass },     { "sound", 1 }, { "param2", 60.0f } } },
        { "Big Filter Sweep",  { { "mode", (float) Sweep },    { "sound", 0 }, { "param3", 60.0f } } },
        { "Acid Sequence",     { { "mode", (float) Seq },      { "sound", 3 } } },
        { "Octave Arpeggio",   { { "mode", (float) Arpeggio }, { "sound", 0 }, { "mix", 70.0f } } },
    };
}
} // namespace monolithic
