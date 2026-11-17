#pragma once

#include <MelonProcessorBase.h>
#include "ReverbDsp.h"

class ReverberareProcessor : public melon::ProcessorBase
{
public:
    ReverberareProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    reverb::Params currentParams() const;

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    reverb::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverberareProcessor)
};
