#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Pedal-style status: a gate LED (green = open, red = closed, like the Zuul+),
// an input meter with the threshold marker and a gain-reduction readout.
class SimpleGateDisplay : public juce::Component
{
public:
    void pull (sgate::Meters& m, const sgate::Params& p);
    void paint (juce::Graphics&) override;

private:
    sgate::Params params;
    float inputDb = -120.0f, peakDb = -120.0f, grDb = 0.0f;
    int   peakAge = 0;
    bool  open = false;
};

class SimpleGateEditor : public melon::EditorBase<SimpleGateProcessor>
{
public:
    explicit SimpleGateEditor (SimpleGateProcessor&);

private:
    void timerCallback() override;
    SimpleGateDisplay display;
};
