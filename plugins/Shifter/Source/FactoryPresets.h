// FactoryPresets.h - Melon Shifter factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace shifter
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Pitch in semitones, Detune in cents, Voice B 0/1, Mode: 0 Tight, 1 Normal, 2 Smooth,
    // Width / Feedback / Mix in %, Delay in ms, Tone in Hz
    return {
        { "Octave Up",
          { { "pitchA", 12.0f }, { "voiceB", 0 }, { "mode", 1 }, { "mix", 45.0f }, { "tone", 12000.0f } } },
        { "Octave Down",
          { { "pitchA", -12.0f }, { "voiceB", 0 }, { "mode", 1 }, { "mix", 55.0f }, { "tone", 8000.0f } } },
        { "Octaves Both Ways",
          { { "pitchA", 12.0f }, { "pitchB", -12.0f }, { "voiceB", 1 }, { "width", 40.0f }, { "mode", 1 }, { "mix", 50.0f } } },
        { "Fifth Above",
          { { "pitchA", 7.0f }, { "voiceB", 0 }, { "mode", 0 }, { "mix", 40.0f } } },
        { "Major Triad",
          { { "pitchA", 4.0f }, { "pitchB", 7.0f }, { "voiceB", 1 }, { "width", 50.0f }, { "mode", 2 }, { "mix", 45.0f } } },
        { "Wide Doubler",
          { { "pitchA", 0.0f }, { "detuneA", 9.0f }, { "pitchB", 0.0f }, { "detuneB", -9.0f }, { "voiceB", 1 },
            { "width", 100.0f }, { "delay", 18.0f }, { "mode", 0 }, { "mix", 45.0f } } },
        { "Down a Whole Step",
          { { "pitchA", -2.0f }, { "voiceB", 0 }, { "mode", 0 }, { "mix", 100.0f } } },
        { "Baritone",
          { { "pitchA", -5.0f }, { "pitchB", -12.0f }, { "voiceB", 1 }, { "width", 20.0f }, { "mode", 1 }, { "mix", 100.0f },
            { "tone", 9000.0f } } },
        { "Shimmer Climb",
          { { "pitchA", 12.0f }, { "voiceB", 0 }, { "mode", 2 }, { "delay", 250.0f }, { "feedback", 55.0f },
            { "tone", 9000.0f }, { "mix", 35.0f } } },
        { "Spiral Fifths",
          { { "pitchA", 7.0f }, { "pitchB", -5.0f }, { "voiceB", 1 }, { "width", 90.0f }, { "mode", 1 },
            { "delay", 320.0f }, { "feedback", 50.0f }, { "tone", 7000.0f }, { "mix", 35.0f } } },
    };
}
} // namespace shifter
