// ToneSuggestions.h - TONE3000 tones suggested for each Melon Rack factory preset.
//
// Nothing here is shipped with the plugin: these are links to tones on tone3000.com.
// The user opens them in the browser, or signs in and lets Melon Rack download them
// (only when they click), as the TONE3000 API terms require.
//
// toneId is the number at the end of the tone's address, e.g.
//   https://www.tone3000.com/tones/maxon-od808-115dbu-6431  ->  6431
// modelHint (optional) picks the capture / IR whose name contains that text; otherwise the
// first one is used. The user can always pick another one from the tone's menu.
// creator may be left empty: it is filled in from TONE3000 once the tone is fetched.
#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace rack
{
enum class ToneSlot { pedal, amp, ir1, ir2 };

inline const char* slotName (ToneSlot s)
{
    switch (s)
    {
        case ToneSlot::pedal: return "Pedal";
        case ToneSlot::amp:   return "Amp";
        case ToneSlot::ir1:   return "IR 1";
        case ToneSlot::ir2:   return "IR 2";
    }
    return "";
}

struct ToneSuggestion
{
    ToneSlot     slot;
    int          toneId;
    juce::String title, creator, url, modelHint;
};

struct PresetTones
{
    juce::String                preset; // factory preset name (see FactoryPresets.h)
    std::vector<ToneSuggestion> tones;
};

inline const std::vector<PresetTones>& presetTones()
{
    using S = ToneSlot;
    static const std::vector<PresetTones> list {
        { "Clean Studio",
          { { S::amp, 13767, "1971 Fender Twin Reverb", "maestrodimusica",
              "https://www.tone3000.com/tones/1971-fender-twin-reverb-13767", {} },
            { S::ir1, 56792, "1966 Fender Bassman Cabinet Jensen C12NA", "maestrodimusica",
              "https://www.tone3000.com/tones/1966-fender-bassman-cabinet-jensen-c12na-speaker-56792", {} } } },

        { "High-Gain Tight",
          { { S::pedal, 6431, "Maxon OD808", "outmodedelectronics",
              "https://www.tone3000.com/tones/maxon-od808-115dbu-6431", {} },
            { S::amp, 6013, "Peavey 6505 1992 Original", "outmodedelectronics",
              "https://www.tone3000.com/tones/peavey-6505-1992-original-115dbu-6013", "Lead" },
            { S::ir1, 45023, "V30 Mesa Boogie 4x12 SM57", "outmodedelectronics",
              "https://www.tone3000.com/tones/celestion-vintage-30-2002-mesa-boogie-4x12-sm57-45023", "0.75in" } } },

        { "Scooped Metal",
          { { S::amp, 28128, "Mesa Dual Rectifier Modern Mode", {},
              "https://www.tone3000.com/tones/mesa-boogie-dual-rectifier-multi-watt-modern-mode-28128", "Red" },
            { S::ir1, 45023, "V30 Mesa Boogie 4x12 SM57", "outmodedelectronics",
              "https://www.tone3000.com/tones/celestion-vintage-30-2002-mesa-boogie-4x12-sm57-45023", "0.50in" },
            { S::ir2, 44078, "The Heaviest Mesa Recto V30 IR", "mattvdh",
              "https://www.tone3000.com/tones/the-heaviest-mesa-recto-v30-ir-44078", {} } } },

        { "Warm Blues",
          { { S::amp, 15222, "1966 Fender Deluxe Reverb", "maestrodimusica",
              "https://www.tone3000.com/tones/66-deluxe-reverb-15222", {} },
            { S::ir1, 56792, "1966 Fender Bassman Cabinet Jensen C12NA", "maestrodimusica",
              "https://www.tone3000.com/tones/1966-fender-bassman-cabinet-jensen-c12na-speaker-56792", {} } } },

        { "Bright Clean",
          { { S::amp, 55221, "Vox AC30S1 Head-Only (EQ-Neutral)", "jtmulcahey",
              "https://www.tone3000.com/tones/ac30s1-head-only-10-pack-eq-neutral-55221", {} },
            { S::ir1, 2288, "1966 Vox AC30 Alnico Silver IR", "tonejunkie",
              "https://www.tone3000.com/tones/tj-66-vox-ac30-alnico-silver-free-ir-pack-2288", {} } } },

        { "Mid Push Lead",
          { { S::amp, 5749, "Marshall JCM800 2203 Boost/Plain", "northernfox",
              "https://www.tone3000.com/tones/marshall-jcm800-2203-boostplain-5749", {} },
            { S::ir1, 45022, "Greenback Marshall 1960TV 4x12 SM57", "outmodedelectronics",
              "https://www.tone3000.com/tones/celestion-greenback-marshall-1960tv-4x12-sm57-45022", {} } } },

        { "Amp Only (No Cab)",
          { { S::amp, 2696, "Vox AC30 Top Boost Greenback (full rig)", "darkwaverec",
              "https://www.tone3000.com/tones/vox-ac30-top-boost-greenback-2696", {} } } },

        { "Blended Cabinets",
          { { S::amp, 5875, "Marshall 1959HW Plexi Medium Gain", "slamminmofo",
              "https://www.tone3000.com/tones/marshall-1959hw-plexi-medium-gain-hyper-accuracy-5875", {} },
            { S::ir1, 45022, "Greenback Marshall 1960TV 4x12 SM57", "outmodedelectronics",
              "https://www.tone3000.com/tones/celestion-greenback-marshall-1960tv-4x12-sm57-45022", {} },
            { S::ir2, 45023, "V30 Mesa Boogie 4x12 SM57", "outmodedelectronics",
              "https://www.tone3000.com/tones/celestion-vintage-30-2002-mesa-boogie-4x12-sm57-45023", {} } } },

        { "Pedal Off",
          { { S::amp, 40971, "Marshall Plexi Super Lead 1959 EL34", {},
              "https://www.tone3000.com/tones/marshall-plexi-super-lead-1959-el34-40971", {} },
            { S::ir1, 45022, "Greenback Marshall 1960TV 4x12 SM57", "outmodedelectronics",
              "https://www.tone3000.com/tones/celestion-greenback-marshall-1960tv-4x12-sm57-45022", {} } } },

        { "Quiet Bedroom",
          { { S::amp, 64903, "Fender Blues DeVille 4x10", "outmodedelectronics",
              "https://www.tone3000.com/tones/fender-blues-deville-4x10-amp-head-captures-64903", "Normal" },
            { S::ir1, 64904, "Eminence Blue Frame Alnico (Blues DeVille 4x10)", "outmodedelectronics",
              "https://www.tone3000.com/tones/eminence-blue-frame-alnico-fender-blues-deville-4x10-64904", {} } } },
    };
    return list;
}

// Suggestions for a factory preset (nullptr = none, e.g. "Default" or a user preset)
inline const std::vector<ToneSuggestion>* tonesForPreset (const juce::String& presetName)
{
    for (const auto& p : presetTones())
        if (p.preset == presetName)
            return &p.tones;
    return nullptr;
}
} // namespace rack
