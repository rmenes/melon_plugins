#pragma once

#include <MelonProcessorBase.h>
#include "TunerDsp.h"

class TunerProcessor : public melon::ProcessorBase
{
public:
    TunerProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    tuner::Params         currentParams() const;
    const tuner::Reading& reading() const { return engine.current(); }

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    tuner::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerProcessor)
};
