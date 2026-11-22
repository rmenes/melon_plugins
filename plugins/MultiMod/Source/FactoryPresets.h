// FactoryPresets.h - Melon MultiMod factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>
#include "MultiModDsp.h"

namespace mmod
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Machine: 0 Chorus, 1 Flanger, 2 Rotary, 3 Vibe, 4 Phaser, 5 Filter, 6 Formant, 7 Vintage Trem,
    // 8 Pattern Trem, 9 Autoswell, 10 Destroyer, 11 Quadrature. Wave: 0 Sine, 1 Triangle, 2 Square,
    // 3 Ramp Up, 4 Ramp Down, 5 Random. Sync: 0 Off, 7 1/4, 10 1/8, 13 1/16 (without a host tempo, Speed is used).
    // Param 1 / 2 depend on the machine; stepped ones use stepPercent().
    return {
        { "Lush Stereo Chorus",
          { { "machine", chorus }, { "speed", 0.7f }, { "wave", sine }, { "depth", 55.0f }, { "mix", 50.0f }, { "width", 75.0f },
            { "param1", stepPercent (multiChorus, kNumChorusModes) }, { "param2", 75.0f } } },
        { "Jet Flanger",
          { { "machine", flanger }, { "speed", 0.18f }, { "wave", triangle }, { "depth", 85.0f }, { "mix", 50.0f }, { "width", 25.0f },
            { "param1", 88.0f }, { "param2", 45.0f } } },
        { "Fast Rotary",
          { { "machine", rotary }, { "speed", 6.5f }, { "depth", 70.0f }, { "mix", 100.0f }, { "width", 70.0f },
            { "param1", 40.0f }, { "param2", 55.0f } } },
        { "Machine Gun Vibe",
          { { "machine", vibe }, { "speed", 4.5f }, { "wave", sine }, { "depth", 75.0f }, { "mix", 100.0f }, { "width", 0.0f },
            { "param1", 55.0f }, { "param2", stepPercent (vibeChorus, kNumVibeModes) } } },
        { "Orange Four Stage",
          { { "machine", phaser }, { "speed", 0.45f }, { "wave", sine }, { "depth", 70.0f }, { "mix", 50.0f }, { "width", 0.0f },
            { "param1", stepPercent (1, 6) }, { "param2", 20.0f } } },
        { "Funky Auto Wah",
          { { "machine", filter }, { "speed", 3.0f }, { "depth", 75.0f }, { "mix", 100.0f }, { "width", 0.0f },
            { "param1", 65.0f }, { "param2", stepPercent (envUp, kNumFilterTypes) } } },
        { "Talking Vowels",
          { { "machine", formant }, { "speed", 0.6f }, { "wave", triangle }, { "depth", 100.0f }, { "mix", 100.0f }, { "width", 20.0f },
            { "param1", stepPercent (2, 8) }, { "param2", 55.0f } } },
        { "Brownface Harmonic",
          { { "machine", vintageTrem }, { "speed", 5.0f }, { "wave", sine }, { "depth", 75.0f }, { "mix", 100.0f }, { "width", 0.0f },
            { "param1", stepPercent (harmonicTrem, kNumTremTypes) }, { "param2", 15.0f } } },
        { "Sixteenth Stutter",
          { { "machine", patternTrem }, { "speed", 8.0f }, { "sync", 13.0f }, { "depth", 100.0f }, { "mix", 100.0f }, { "width", 0.0f },
            { "param1", stepPercent (7, 8) }, { "param2", 12.0f } } },
        { "Violin Swells",
          { { "machine", autoswell }, { "depth", 100.0f }, { "mix", 100.0f }, { "param1", 55.0f }, { "param2", 55.0f } } },
    };
}
} // namespace mmod
