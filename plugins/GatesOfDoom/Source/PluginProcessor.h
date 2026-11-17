#pragma once

#include <MelonProcessorBase.h>
#include "GateDsp.h"

class GatesOfDoomProcessor : public melon::ProcessorBase
{
public:
    GatesOfDoomProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    gate::Params currentParams() const;
    const gate::MeterQueue& meters() const { return engine.meterQueue(); }

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    gate::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GatesOfDoomProcessor)
};
