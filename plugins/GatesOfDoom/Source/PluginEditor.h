#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

// Scrolling history (about 5 seconds): input level (grey), output level (blue),
// gain reduction (orange, from the top) and the threshold line.
class GateDisplay : public juce::Component
{
public:
    void pull (const gate::MeterQueue& q, const gate::Params& p);
    void paint (juce::Graphics&) override;

private:
    static constexpr int kHistory = gate::kMeterRate * 5;
    std::vector<gate::MeterPoint> history = std::vector<gate::MeterPoint> ((size_t) kHistory, { -120.0f, -120.0f, 0.0f });
    int          head = 0, readIndex = 0;
    gate::Params params;
};

class GatesOfDoomEditor : public melon::EditorBase<GatesOfDoomProcessor>
{
public:
    explicit GatesOfDoomEditor (GatesOfDoomProcessor&);

private:
    void timerCallback() override;
    GateDisplay display;
};
