// FactoryPresets.h - Melon GraphicEQ factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>
#include "EqDsp.h"

namespace geq
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    std::vector<melon::FactoryPreset> list {
        { "Smiley Face",
          { { "mode", 0 }, { "b10_0", 6.0f }, { "b10_1", 4.0f }, { "b10_2", 1.0f }, { "b10_3", -1.0f }, { "b10_4", -3.0f }, { "b10_5", -3.0f }, { "b10_6", -1.0f }, { "b10_7", 2.0f }, { "b10_8", 4.0f }, { "b10_9", 5.0f } } },
        { "Mid Scoop",
          { { "mode", 0 }, { "b10_0", 3.0f }, { "b10_1", 2.0f }, { "b10_2", 0.0f }, { "b10_3", -2.0f }, { "b10_4", -5.0f }, { "b10_5", -6.0f }, { "b10_6", -3.0f }, { "b10_7", 1.0f }, { "b10_8", 3.0f }, { "b10_9", 3.0f } } },
        { "Bass Boost",
          { { "mode", 0 }, { "b10_0", 6.0f }, { "b10_1", 6.0f }, { "b10_2", 3.0f }, { "b10_3", 1.0f }, { "b10_4", 0.0f }, { "b10_5", 0.0f }, { "b10_6", 0.0f }, { "b10_7", 0.0f }, { "b10_8", 0.0f }, { "b10_9", 0.0f } } },
        { "Presence Lift",
          { { "mode", 0 }, { "b10_0", 0.0f }, { "b10_1", 0.0f }, { "b10_2", 0.0f }, { "b10_3", 0.0f }, { "b10_4", 0.0f }, { "b10_5", 1.0f }, { "b10_6", 3.0f }, { "b10_7", 4.0f }, { "b10_8", 2.0f }, { "b10_9", 1.0f } } },
        { "Warm Vintage",
          { { "mode", 0 }, { "b10_0", 2.0f }, { "b10_1", 2.0f }, { "b10_2", 2.0f }, { "b10_3", 1.0f }, { "b10_4", 0.0f }, { "b10_5", -1.0f }, { "b10_6", -2.0f }, { "b10_7", -3.0f }, { "b10_8", -5.0f }, { "b10_9", -6.0f } } },
        { "Telephone",
          { { "mode", 0 }, { "b10_0", -12.0f }, { "b10_1", -12.0f }, { "b10_2", -10.0f }, { "b10_3", -6.0f }, { "b10_4", 2.0f }, { "b10_5", 5.0f }, { "b10_6", 4.0f }, { "b10_7", -6.0f }, { "b10_8", -12.0f }, { "b10_9", -12.0f } } },
        { "Mud Cleaner",
          { { "mode", 0 }, { "b10_0", 0.0f }, { "b10_1", -1.0f }, { "b10_2", -3.0f }, { "b10_3", -4.0f }, { "b10_4", -2.0f }, { "b10_5", 0.0f }, { "b10_6", 1.0f }, { "b10_7", 1.0f }, { "b10_8", 0.0f }, { "b10_9", 0.0f } } },
        { "Lead Guitar Push",
          { { "mode", 0 }, { "b10_0", -3.0f }, { "b10_1", -2.0f }, { "b10_2", 0.0f }, { "b10_3", 1.0f }, { "b10_4", 3.0f }, { "b10_5", 4.0f }, { "b10_6", 3.0f }, { "b10_7", 1.0f }, { "b10_8", -1.0f }, { "b10_9", -3.0f } } },
    };

    // 31-band presets
    const auto& f31 = geq::bandFrequencies (31);
    auto make31 = [&f31] (const juce::String& name, auto gainForBand) {
        melon::FactoryPreset p { name, { { "mode", 1.0f } } };
        for (int i = 0; i < 31; ++i)
            p.values.push_back ({ "b31_" + juce::String (i), (float) gainForBand (i, f31[(size_t) i]) });
        return p;
    };

    static const float acoustic[31] = { -12.0f, -12.0f, -10.0f, -8.0f, -5.0f, -3.0f, -1.0f, 0.0f, -1.0f, -3.0f, -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f, 3.0f, 3.0f, 2.0f, 1.0f, 0.0f };
    list.push_back (make31 ("Acoustic Guitar (31)", [] (int i, double) { return acoustic[i]; }));

    // Gentle bright tilt: about +2.5 dB per octave above 1 kHz, -2.5 dB per octave below, limited to 6 dB
    list.push_back (make31 ("Bright Tilt (31)", [] (int, double f) {
        return juce::jlimit (-6.0, 6.0, std::round (2.5 * std::log2 (f / 1000.0) * 10.0) / 10.0);
    }));

    return list;
}
} // namespace geq
