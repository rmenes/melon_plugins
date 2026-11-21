#pragma once

#include <MelonEditorBase.h>
#include <MelonFileLoader.h>
#include "PluginProcessor.h"

//   ┌──────────────────────────────────────────────────────────────┐
//   │ [Melon logo] | Cab   [Bypass]                   Output [===] │
//   │ Preset [<] [ preset name ] [>]               [Save] [Save As] │
//   │ ┌ IR FILE LOADER ──────────────────────────────────────────┐ │
//   │ │ [<] [ impulse response name                     v ] [>]  │ │
//   │ └──────────────────────────────────────────────────────────┘ │
//   │        (Low Cut)        (High Cut)        (Mix)              │
//   └──────────────────────────────────────────────────────────────┘
class CabEditor : public melon::EditorBase<CabProcessor>
{
public:
    explicit CabEditor (CabProcessor&);

private:
    void timerCallback() override { loader.setStatus (processor.irStatus()); }

    melon::FileLoader loader { "IR File Loader", "IR", CabProcessor::irWildcard,
                               "No IR loaded - click to load a .wav impulse response", JucePlugin_Name };
};
