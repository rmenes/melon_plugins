#pragma once

#include <MelonProcessorBase.h>
#include "DirtDsp.h"

class DirtProcessor : public melon::ProcessorBase
{
public:
    DirtProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    dirt::Params currentParams() const;

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    dirt::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DirtProcessor)
};
