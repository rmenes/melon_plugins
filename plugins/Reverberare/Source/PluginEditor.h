#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Shows the selected machine (name and description) and a sketch of its decay envelope:
// pre-delay, early reflections and the tail falling over the Decay time.
class ReverbDisplay : public juce::Component
{
public:
    void setState (const reverb::Params& p);
    void paint (juce::Graphics&) override;

private:
    reverb::Params params;
};

class ReverberareEditor : public melon::EditorBase<ReverberareProcessor>
{
public:
    explicit ReverberareEditor (ReverberareProcessor&);

private:
    void timerCallback() override;
    void updateParamLabels (int machine);

    ReverbDisplay display;
    Knob*         param1 = nullptr;
    Knob*         param2 = nullptr;
    int           shownMachine = -1;
};
