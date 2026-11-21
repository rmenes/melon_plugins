#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Left: the selected machine, its description and the delay time readout.
// Right: where the repeats land over time (left repeats up, right repeats down;
// reversed repeats drawn as rising ramps).
class MultiDelayDisplay : public juce::Component
{
public:
    void setState (const mdelay::Params& p, double bpm, bool synced, const juce::String& syncName);
    void paint (juce::Graphics&) override;

private:
    mdelay::Params params;
    double         bpm = 0.0;
    bool           synced = false;
    juce::String   syncName;
};

class MultiDelayEditor : public melon::EditorBase<MultiDelayProcessor>
{
public:
    explicit MultiDelayEditor (MultiDelayProcessor&);

private:
    void timerCallback() override;
    void updateParamLabels (int machine);

    MultiDelayDisplay display;
    Knob*             timeKnob = nullptr;
    Knob*             param1   = nullptr;
    Knob*             param2   = nullptr;
    int               shownMachine = -1;
};
