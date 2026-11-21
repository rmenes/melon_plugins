#pragma once

#include <MelonProcessorBase.h>
#include "SimpleGateDsp.h"

class SimpleGateProcessor : public melon::ProcessorBase
{
public:
    SimpleGateProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    sgate::Params  currentParams() const;
    sgate::Meters& meters() { return engine.meterValues(); }

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    sgate::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleGateProcessor)
};
