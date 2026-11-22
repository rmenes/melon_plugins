#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Left: the selected machine, its description and the speed readout.
// Right: the modulation over two LFO cycles (left channel in blue, right in orange) with a
// moving playhead; Pattern Trem shows its whole pattern, Autoswell its fade-in curve.
class MultiModDisplay : public juce::Component
{
public:
    void setState (const mmod::Params& p, double lfoPosition, double bpm, bool synced, const juce::String& syncName);
    void paint (juce::Graphics&) override;

private:
    mmod::Params params;
    double       position = 0.0, bpm = 0.0;
    bool         synced = false;
    juce::String syncName;
};

class MultiModEditor : public melon::EditorBase<MultiModProcessor>
{
public:
    explicit MultiModEditor (MultiModProcessor&);

private:
    void timerCallback() override;
    void updateMachine (int machine);

    MultiModDisplay display;
    Knob*           speedKnob = nullptr;
    Knob*           depthKnob = nullptr;
    Knob*           param1    = nullptr;
    Knob*           param2    = nullptr;
    juce::ComboBox* syncBox   = nullptr;
    juce::ComboBox* waveBox   = nullptr;
    juce::TextButton* tapButton = nullptr;
    int             shownMachine = -1;
};
