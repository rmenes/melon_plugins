// FactoryPresets.h - Melon Rack factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace rack
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Factory presets set the controls only: the loaded models and IRs are kept.
    // eq_1..eq_6 = 100, 220, 500, 1200, 2700, 6000 Hz. EQ Position: 0 Before Pedal, 1 Between, 2 After Amp.
    return {
        { "Clean Studio",
          { { "gate_on", 0 }, { "eq_on", 0 }, { "cab_on", 1 }, { "cab_mode", 0 } } },
        { "High-Gain Tight",
          { { "gate_on", 1 }, { "gate_thr", -55.0f }, { "eq_on", 1 }, { "eq_pos", 0 }, { "eq_1", -3.0f }, { "eq_2", -1.0f }, { "eq_4", 2.0f } } },
        { "Scooped Metal",
          { { "gate_on", 1 }, { "gate_thr", -60.0f }, { "eq_on", 1 }, { "eq_pos", 2 }, { "eq_1", 2.0f }, { "eq_3", -4.0f }, { "eq_4", -2.0f }, { "eq_5", 2.0f }, { "eq_6", 1.0f } } },
        { "Warm Blues",
          { { "eq_on", 1 }, { "eq_pos", 2 }, { "eq_1", 1.0f }, { "eq_2", 1.0f }, { "eq_5", -2.0f }, { "eq_6", -3.0f } } },
        { "Bright Clean",
          { { "eq_on", 1 }, { "eq_pos", 2 }, { "eq_2", -2.0f }, { "eq_5", 2.0f }, { "eq_6", 3.0f } } },
        { "Mid Push Lead",
          { { "eq_on", 1 }, { "eq_pos", 1 }, { "eq_1", -2.0f }, { "eq_3", 2.0f }, { "eq_4", 4.0f }, { "eq_5", 1.0f } } },
        { "Amp Only (No Cab)",
          { { "cab_on", 0 } } },
        { "Blended Cabinets",
          { { "cab_on", 1 }, { "cab_mode", 1 }, { "cab_blend", 50.0f } } },
        { "Pedal Off",
          { { "a_on", 0 } } },
        { "Quiet Bedroom",
          { { "gate_on", 1 }, { "gate_thr", -65.0f }, { "eq_on", 1 }, { "eq_pos", 2 }, { "eq_1", -2.0f }, { "eq_6", -4.0f }, { "output", -6.0f } } },
    };
}
} // namespace rack
