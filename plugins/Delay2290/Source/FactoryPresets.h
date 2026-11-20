// FactoryPresets.h - Melon 2290 Delay factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace delay2290
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Sync: 0 Off, 5 = 1/4, 8 = 1/8, 9 = 1/8 dotted (see syncValues()). Mode: 0 Mono, 1 Stereo, 2 Ping-pong
    return {
        { "Slapback",
          { { "time", 110.0f }, { "sync", 0 }, { "mode", 0 }, { "feedback", 5.0f }, { "mix", 30.0f }, { "hicut", 6000.0f }, { "locut", 100.0f } } },
        { "Quarter Note",
          { { "sync", 5 }, { "mode", 1 }, { "feedback", 30.0f }, { "mix", 25.0f } } },
        { "Dotted Eighth Rhythm",
          { { "sync", 9 }, { "mode", 1 }, { "feedback", 35.0f }, { "mix", 30.0f }, { "hicut", 8000.0f } } },
        { "Ping-Pong Wide",
          { { "time", 375.0f }, { "sync", 0 }, { "mode", 2 }, { "feedback", 45.0f }, { "mix", 30.0f } } },
        { "Ducked Lead",
          { { "time", 450.0f }, { "sync", 0 }, { "mode", 1 }, { "feedback", 40.0f }, { "mix", 35.0f }, { "duckthresh", -35.0f }, { "duckamount", 12.0f }, { "duckrelease", 400.0f } } },
        { "Tape Echo Wobble",
          { { "time", 320.0f }, { "sync", 0 }, { "mode", 0 }, { "feedback", 45.0f }, { "mix", 30.0f }, { "hicut", 3500.0f }, { "locut", 150.0f }, { "moddepth", 35.0f }, { "modspeed", 0.7f } } },
        { "Ambient Wash",
          { { "time", 700.0f }, { "sync", 0 }, { "mode", 2 }, { "feedback", 70.0f }, { "mix", 40.0f }, { "hicut", 4500.0f }, { "spread", 40.0f }, { "moddepth", 25.0f }, { "modspeed", 0.3f } } },
        { "Stereo Doubler",
          { { "time", 25.0f }, { "sync", 0 }, { "mode", 1 }, { "feedback", 0.0f }, { "mix", 40.0f }, { "spread", 60.0f }, { "moddepth", 50.0f }, { "modspeed", 1.2f } } },
        { "Runaway Feedback",
          { { "time", 300.0f }, { "sync", 0 }, { "mode", 1 }, { "feedback", 92.0f }, { "mix", 40.0f }, { "hicut", 5000.0f }, { "locut", 200.0f } } },
        { "Eighth Ping-Pong",
          { { "sync", 8 }, { "mode", 2 }, { "feedback", 40.0f }, { "mix", 25.0f } } },
    };
}
} // namespace delay2290
