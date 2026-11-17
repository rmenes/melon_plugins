#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Left: red LED-style readout of the delay time (a nod to the rack unit's display).
// Right: the echo pattern over time (left repeats up, right repeats down) and the ducking meter.
class DelayDisplay : public juce::Component
{
public:
    void setState (const delay2290::Params& p, double bpm, double duckDb, bool synced, const juce::String& syncName);
    void paint (juce::Graphics&) override;

private:
    delay2290::Params params;
    double            bpm = 0.0, duck = 0.0;
    bool              synced = false;
    juce::String      syncName;
};

class Delay2290Editor : public melon::EditorBase<Delay2290Processor>
{
public:
    explicit Delay2290Editor (Delay2290Processor&);

private:
    void timerCallback() override;
    DelayDisplay display;
    Knob*        timeKnob = nullptr;
};
