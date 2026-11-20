#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Pitch ruler from -24 to +24 semitones: the dry note at 0, voice A (blue) and voice B (orange)
// with their interval names, and the climbing echoes when Feedback is up.
class PitchDisplay : public juce::Component
{
public:
    void update (const shifter::Params& p);
    void paint (juce::Graphics&) override;

private:
    shifter::Params params;
    bool            valid = false;
};

class ShifterEditor : public melon::EditorBase<ShifterProcessor>
{
public:
    explicit ShifterEditor (ShifterProcessor&);

private:
    void timerCallback() override;
    PitchDisplay display;
};
