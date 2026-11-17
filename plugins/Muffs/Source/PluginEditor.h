#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Frequency response of the selected version's tone stack (the mid scoop), with the
// bass-only and treble-only extremes drawn faintly for reference.
class MuffsDisplay : public juce::Component
{
public:
    void setState (const muffs::Params& p);
    void paint (juce::Graphics&) override;

private:
    muffs::Params params;
};

class MuffsEditor : public melon::EditorBase<MuffsProcessor>
{
public:
    explicit MuffsEditor (MuffsProcessor&);

private:
    void timerCallback() override;
    MuffsDisplay display;
};
