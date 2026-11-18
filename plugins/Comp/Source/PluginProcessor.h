#pragma once

#include <MelonProcessorBase.h>
#include "CompDsp.h"

class CompProcessor : public melon::ProcessorBase
{
public:
    CompProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    comp::Params currentParams() const;
    const comp::MeterQueue& meters() const { return engine.meterQueue(); }

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    comp::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompProcessor)
};
