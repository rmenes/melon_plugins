#pragma once

#include <MelonProcessorBase.h>
#include "MuffsDsp.h"

class MuffsProcessor : public melon::ProcessorBase
{
public:
    MuffsProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    muffs::Params currentParams() const;

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    muffs::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuffsProcessor)
};
