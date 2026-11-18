#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Scrolling history (about 5 seconds): input level (grey), output level (blue) and
// gain reduction (orange, from the top), plus a gain-reduction bar on the right.
class CompDisplay : public juce::Component
{
public:
    void pull (const comp::MeterQueue& q, const comp::Params& p);
    void paint (juce::Graphics&) override;

private:
    static constexpr int kHistory = comp::kMeterRate * 5;
    std::vector<comp::MeterPoint> history = std::vector<comp::MeterPoint> ((size_t) kHistory, { -120.0f, -120.0f, 0.0f });
    int          head = 0, readIndex = 0;
    float        grBar = 0.0f; // smoothed for the bar meter
    comp::Params params;
};

class CompEditor : public melon::EditorBase<CompProcessor>
{
public:
    explicit CompEditor (CompProcessor&);

private:
    void timerCallback() override;
    CompDisplay display;
};
