// FactoryPresets.h - Melon Reverberare factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace reverb
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Machine: 0 Room, 1 Hall, 2 Plate, 3 Spring, 4 Swell, 5 Bloom, 6 Cloud, 7 Chorale,
    // 8 Shimmer, 9 Magneto, 10 Nonlinear, 11 Reflections. Param 1 / 2 depend on the machine.
    return {
        { "Small Studio Room",
          { { "machine", 0 }, { "decay", 0.8f }, { "predelay", 5.0f }, { "tone", 55.0f }, { "mod", 15.0f }, { "param1", 35.0f }, { "param2", 55.0f }, { "mix", 25.0f } } },
        { "Concert Hall",
          { { "machine", 1 }, { "decay", 3.5f }, { "predelay", 25.0f }, { "tone", 45.0f }, { "mod", 25.0f }, { "param1", 70.0f }, { "param2", 55.0f }, { "mix", 30.0f } } },
        { "Vocal Plate",
          { { "machine", 2 }, { "decay", 2.2f }, { "predelay", 30.0f }, { "tone", 60.0f }, { "mod", 20.0f }, { "param1", 55.0f }, { "param2", 45.0f }, { "mix", 25.0f } } },
        { "Surf Spring",
          { { "machine", 3 }, { "decay", 2.5f }, { "predelay", 0.0f }, { "tone", 60.0f }, { "mod", 10.0f }, { "param1", 70.0f }, { "param2", 75.0f }, { "mix", 35.0f } } },
        { "Blooming Ambience",
          { { "machine", 5 }, { "decay", 6.0f }, { "predelay", 40.0f }, { "tone", 40.0f }, { "mod", 45.0f }, { "param1", 60.0f }, { "param2", 50.0f }, { "mix", 40.0f } } },
        { "Endless Cloud",
          { { "machine", 6 }, { "decay", 12.0f }, { "predelay", 50.0f }, { "tone", 40.0f }, { "mod", 50.0f }, { "param1", 85.0f }, { "param2", 50.0f }, { "mix", 50.0f } } },
        { "Angelic Choir",
          { { "machine", 7 }, { "decay", 5.0f }, { "predelay", 30.0f }, { "tone", 50.0f }, { "mod", 35.0f }, { "param1", 50.0f }, { "param2", 60.0f }, { "mix", 40.0f } } },
        { "Octave Shimmer",
          { { "machine", 8 }, { "decay", 8.0f }, { "predelay", 40.0f }, { "tone", 55.0f }, { "mod", 40.0f }, { "param1", 50.0f }, { "param2", 55.0f }, { "mix", 40.0f } } },
        { "Tape Echo Verb",
          { { "machine", 9 }, { "decay", 3.0f }, { "predelay", 0.0f }, { "tone", 40.0f }, { "mod", 30.0f }, { "param1", 45.0f }, { "param2", 50.0f }, { "mix", 35.0f } } },
        { "Gated 80s Drums",
          { { "machine", 10 }, { "decay", 0.45f }, { "predelay", 0.0f }, { "tone", 55.0f }, { "mod", 0.0f }, { "param1", 50.0f }, { "param2", 70.0f }, { "mix", 30.0f } } },
    };
}
} // namespace reverb
