#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Big note name with flat/sharp lights, a needle (or strobe) meter in cents and, for the
// fixed tunings, a row with the strings (the one being tuned is highlighted, tuned ones ticked).
class TunerDisplay : public juce::Component
{
public:
    void update (const tuner::Reading& r, const tuner::Params& p, double dt);
    void paint (juce::Graphics&) override;

    static constexpr double kInTuneCents = 2.0;

private:
    void drawNeedle (juce::Graphics&, juce::Rectangle<float>, bool active, juce::Colour);
    void drawStrobe (juce::Graphics&, juce::Rectangle<float>, bool active, juce::Colour);
    void drawStrings (juce::Graphics&, juce::Rectangle<float>);

    tuner::Params params;
    tuner::Target target;
    bool   hasNote = false, live = false;
    double cents = 0.0, frequency = 0.0;
    double sinceSeen = 1e9, inTuneTime = 0.0, strobePhase = 0.0;
    int    lastTuning = -1;
    std::vector<bool> tuned;
};

class TunerEditor : public melon::EditorBase<TunerProcessor>
{
public:
    explicit TunerEditor (TunerProcessor&);

private:
    void timerCallback() override;
    TunerDisplay display;
    double lastTime = 0.0;
};
