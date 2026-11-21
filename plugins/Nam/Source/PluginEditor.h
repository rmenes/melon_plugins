#pragma once

#include <MelonEditorBase.h>
#include <MelonFileLoader.h>
#include "PluginProcessor.h"

//   ┌──────────────────────────────────────────────────────────────┐
//   │ [Melon logo] | NAM   [Bypass]                   Output [===] │
//   │ Preset [<] [ preset name ] [>]               [Save] [Save As] │
//   │ ┌ NAM FILE LOADER ─────────────────────────────────────────┐ │
//   │ │ [<] [ model name                                v ] [>]  │ │
//   │ └──────────────────────────────────────────────────────────┘ │
//   │  [x] Noise Gate   [x] EQ   [x] Normalize                     │
//   │   (Input)   (Gate)   (Bass)   (Middle)   (Treble)            │
//   └──────────────────────────────────────────────────────────────┘
class NamEditor : public melon::EditorBase<NamProcessor>
{
public:
    explicit NamEditor (NamProcessor&);

private:
    void timerCallback() override;

    melon::FileLoader loader { "NAM File Loader", "NAM", NamProcessor::modelWildcard,
                               "No model loaded - click to load a .nam file", JucePlugin_Name };

    juce::ToggleButton* gateOn = nullptr;
    juce::ToggleButton* eqOn   = nullptr;
    Knob*               gateKnob = nullptr;
    Knob*               toneKnobs[3] {};
};
