#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Left: the note being played (tracked from the guitar or from MIDI), its tuning in cents
// and the input level / trigger threshold.
// Right: the 12 modes (the current one lit), the sound name, its three Param values and,
// for SEQ and ARPEGGIO, the pattern steps with the current one lit.
class MonolithicSynthesisDisplay : public juce::Component
{
public:
    struct State
    {
        melon::mono::EngineParams params;
        float  pitch = 0.0f, level = 0.0f;
        bool   active = false, bypassed = false;
        int    step = -1;
        double bpm = 0.0;
    };

    void setState (const State& s);
    void paint (juce::Graphics&) override;

private:
    State state;
};

class MonolithicSynthesisEditor : public melon::EditorBase<MonolithicSynthesisProcessor>
{
public:
    explicit MonolithicSynthesisEditor (MonolithicSynthesisProcessor&);

private:
    void timerCallback() override;
    void updateMode (int mode);

    MonolithicSynthesisDisplay display;
    juce::ComboBox*            soundBox = nullptr;
    Knob*                      paramKnobs[3] {};
    Knob*                      sensitivityKnob = nullptr;
    int                        shownMode = -1;
};
