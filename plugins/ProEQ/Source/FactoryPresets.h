// FactoryPresets.h - Melon Pro EQ factory presets.
// Built into the plugin and read-only. Values are in real units (as shown in the host);
// anything not listed uses its default value. "Default" is added automatically as preset 0.
#pragma once

#include <MelonPresets.h>

namespace proeq
{
inline std::vector<melon::FactoryPreset> factoryPresets()
{
    // Type: 0 Bell, 1 Low Shelf, 2 Low Cut, 3 High Shelf, 4 High Cut, 5 Notch, 6 Band Pass, 7 Tilt Shelf
    // Slope: 0 = 6, 1 = 12, 2 = 18, 3 = 24, 4 = 30, 5 = 36, 6 = 48, 7 = 72, 8 = 96 dB/oct
    // Stereo: 0 Stereo, 1 Left, 2 Right, 3 Mid, 4 Side
    enum { bell, lowShelf, lowCut, highShelf, highCut, notch, bandPass, tilt };
    enum { stereo, left, right, mid, side };

    struct Band
    {
        int   type;
        float freq, gain = 0.0f, q = 1.0f;
        int   slope = 1, placement = stereo;
        float dynRange = 0.0f, dynThreshold = -30.0f;
    };

    auto preset = [] (const juce::String& name, std::vector<Band> bands) {
        melon::FactoryPreset p { name, {} };
        for (size_t i = 0; i < bands.size(); ++i)
        {
            const auto& b = bands[i];
            const auto id = [i] (const char* field) { return "b" + juce::String ((int) i + 1) + "_" + field; };
            p.values.push_back ({ id ("used"), 1.0f });
            p.values.push_back ({ id ("type"), (float) b.type });
            p.values.push_back ({ id ("freq"), b.freq });
            p.values.push_back ({ id ("gain"), b.gain });
            p.values.push_back ({ id ("q"), b.q });
            p.values.push_back ({ id ("slope"), (float) b.slope });
            p.values.push_back ({ id ("stereo"), (float) b.placement });
            p.values.push_back ({ id ("dynrange"), b.dynRange });
            p.values.push_back ({ id ("dynthr"), b.dynThreshold });
        }
        return p;
    };

    return {
        preset ("Vocal Presence", { { lowCut, 80.0f, 0.0f, 0.7f, 2 },
                                    { bell, 250.0f, -2.5f, 1.2f },
                                    { bell, 3500.0f, 2.5f, 0.9f },
                                    { highShelf, 12000.0f, 2.0f, 0.7f } }),
        preset ("Kick Punch", { { lowCut, 30.0f, 0.0f, 0.7f, 3 },
                                { bell, 60.0f, 4.0f, 1.2f },
                                { bell, 350.0f, -5.0f, 1.5f },
                                { bell, 4000.0f, 3.0f, 1.0f } }),
        preset ("Snare Crack", { { lowCut, 80.0f, 0.0f, 0.7f, 2 },
                                 { bell, 200.0f, 2.0f, 1.0f },
                                 { bell, 900.0f, -3.0f, 2.0f },
                                 { bell, 5000.0f, 3.0f, 1.0f },
                                 { highShelf, 10000.0f, 2.0f, 0.7f } }),
        preset ("Acoustic Guitar", { { lowCut, 90.0f, 0.0f, 0.7f, 2 },
                                     { bell, 200.0f, -3.0f, 1.0f },
                                     { bell, 3000.0f, 1.5f, 1.0f },
                                     { highShelf, 10000.0f, 2.5f, 0.7f } }),
        preset ("Bass Definition", { { lowCut, 35.0f, 0.0f, 0.7f, 3 },
                                     { bell, 80.0f, 2.0f, 1.0f },
                                     { bell, 250.0f, -2.5f, 1.0f },
                                     { bell, 800.0f, 2.0f, 1.2f },
                                     { highCut, 8000.0f, 0.0f, 0.7f, 1 } }),
        preset ("Electric Guitar Cleanup", { { lowCut, 100.0f, 0.0f, 0.7f, 3 },
                                             { bell, 400.0f, -2.0f, 1.4f },
                                             { bell, 2500.0f, 1.5f, 1.0f },
                                             { highCut, 7000.0f, 0.0f, 0.7f, 2 } }),
        preset ("Master Air & Weight", { { lowCut, 20.0f, 0.0f, 0.7f, 4 },
                                         { lowShelf, 80.0f, 1.5f, 0.7f },
                                         { highShelf, 12000.0f, 2.0f, 0.7f } }),
        preset ("Telephone", { { lowCut, 400.0f, 0.0f, 0.7f, 6 },
                               { bell, 1500.0f, 6.0f, 1.5f },
                               { highCut, 3000.0f, 0.0f, 0.7f, 6 } }),
        preset ("De-Harsh (Dynamic)", { { lowCut, 30.0f, 0.0f, 0.7f, 3 },
                                        { bell, 3200.0f, 0.0f, 2.0f, 1, stereo, -6.0f, -30.0f },
                                        { bell, 7000.0f, 0.0f, 3.0f, 1, stereo, -4.0f, -35.0f } }),
        preset ("Mid/Side Master", { { lowCut, 120.0f, 0.0f, 0.7f, 1, side },
                                     { bell, 300.0f, -1.5f, 1.0f, 1, mid },
                                     { highShelf, 8000.0f, 2.5f, 0.7f, 1, side } }),
    };
}
} // namespace proeq
