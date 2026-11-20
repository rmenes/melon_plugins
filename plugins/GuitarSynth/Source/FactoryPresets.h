// FactoryPresets.h - Melon Guitar Synth factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace gsynth
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    return {
        { "Classic Synth Sweep",
          { { "suboctave", 50.0f }, { "guitar", 0.0f }, { "octave", 0.0f }, { "square", 70.0f }, { "attack", 0.0f }, { "resonance", 70.0f }, { "start", 3000.0f }, { "stop", 300.0f }, { "rate", 50.0f } } },
        { "Synth Bass",
          { { "suboctave", 90.0f }, { "guitar", 0.0f }, { "octave", 0.0f }, { "square", 40.0f }, { "resonance", 50.0f }, { "start", 1200.0f }, { "stop", 150.0f }, { "rate", 60.0f } } },
        { "Bowed Strings",
          { { "suboctave", 30.0f }, { "guitar", 40.0f }, { "octave", 40.0f }, { "square", 30.0f }, { "attack", 450.0f }, { "resonance", 20.0f }, { "start", 4000.0f }, { "stop", 2500.0f }, { "rate", 20.0f } } },
        { "Funky Filter Up",
          { { "trigger", 60.0f }, { "suboctave", 30.0f }, { "guitar", 30.0f }, { "octave", 20.0f }, { "square", 60.0f }, { "resonance", 85.0f }, { "start", 300.0f }, { "stop", 2500.0f }, { "rate", 70.0f } } },
        { "Octave Organ",
          { { "suboctave", 50.0f }, { "guitar", 50.0f }, { "octave", 60.0f }, { "square", 0.0f }, { "resonance", 10.0f }, { "start", 8000.0f }, { "stop", 8000.0f }, { "rate", 50.0f } } },
        { "Squelch Lead",
          { { "suboctave", 0.0f }, { "guitar", 0.0f }, { "octave", 50.0f }, { "square", 90.0f }, { "resonance", 90.0f }, { "start", 5000.0f }, { "stop", 400.0f }, { "rate", 80.0f } } },
        { "Slow Swell Pad",
          { { "suboctave", 40.0f }, { "guitar", 20.0f }, { "octave", 40.0f }, { "square", 50.0f }, { "attack", 1200.0f }, { "resonance", 40.0f }, { "start", 2000.0f }, { "stop", 600.0f }, { "rate", 10.0f } } },
        { "Fuzz Synth",
          { { "suboctave", 20.0f }, { "guitar", 0.0f }, { "octave", 30.0f }, { "square", 100.0f }, { "resonance", 30.0f }, { "start", 6000.0f }, { "stop", 3000.0f }, { "rate", 50.0f } } },
        { "Wah Pluck",
          { { "trigger", 65.0f }, { "suboctave", 0.0f }, { "guitar", 60.0f }, { "octave", 0.0f }, { "square", 30.0f }, { "resonance", 80.0f }, { "start", 2500.0f }, { "stop", 400.0f }, { "rate", 85.0f } } },
        { "Half Dry Blend",
          { { "suboctave", 60.0f }, { "guitar", 50.0f }, { "octave", 0.0f }, { "square", 50.0f }, { "resonance", 60.0f }, { "start", 2500.0f }, { "stop", 350.0f }, { "rate", 55.0f }, { "mix", 50.0f } } },
    };
}
} // namespace gsynth
