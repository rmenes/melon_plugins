#pragma once

#include <MelonProcessorBase.h>
#include "TunerDsp.h"

class TunerProcessor : public melon::ProcessorBase
{
public:
    TunerProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    static constexpr const char* kReadoutId    = "readout"; // read-only: detected Hz (0 = none), LV2 only
    static constexpr float       kReadoutMaxHz = 2000.0f;

    tuner::Params         currentParams() const;
    const tuner::Reading& reading() const { return engine.current(); }

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    void publishReading();

    tuner::Engine engine;
    juce::RangedAudioParameter* readout = nullptr;
    uint32_t lastFrames  = 0;
    float    lastReadout = -1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerProcessor)
};
