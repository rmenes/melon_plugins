#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Left: a rate LED that pulses with the tremolo (like the TR-2's Rate indicator) and the
// rate readout. Right: two cycles of the volume curve (left channel in blue, right in
// orange in Pan mode) with a moving playhead.
class TremDisplay : public juce::Component
{
public:
    void setState (const trem::Params& p, double phase, float gainL, float gainR, double bpm, bool synced,
                   const juce::String& syncName);
    void paint (juce::Graphics&) override;

private:
    trem::Params params;
    double       phase = 0.0, bpm = 0.0;
    float        gainL = 1.0f, gainR = 1.0f;
    bool         synced = false;
    juce::String syncName;
};

class TremEditor : public melon::EditorBase<TremProcessor>
{
public:
    explicit TremEditor (TremProcessor&);

private:
    void timerCallback() override;

    TremDisplay display;
    Knob*       rateKnob = nullptr;
};
