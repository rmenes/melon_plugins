#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Left: the drive curve of the selected model (input level -> output level).
// Right: the frequency response of its Tone control.
class DirtDisplay : public juce::Component
{
public:
    void setState (const dirt::Params& p, double sampleRate);
    void paint (juce::Graphics&) override;

private:
    dirt::Params params;
    double       fs = 48000.0;
};

class DirtEditor : public melon::EditorBase<DirtProcessor>
{
public:
    explicit DirtEditor (DirtProcessor&);

private:
    void timerCallback() override;
    DirtDisplay display;
};
