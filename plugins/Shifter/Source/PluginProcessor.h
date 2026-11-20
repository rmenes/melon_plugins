#pragma once

#include <MelonProcessorBase.h>
#include "ShifterDsp.h"

class ShifterProcessor : public melon::ProcessorBase
{
public:
    ShifterProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    shifter::Params currentParams() const;

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    shifter::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShifterProcessor)
};
