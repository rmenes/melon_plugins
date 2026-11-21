#pragma once

#include <MelonProcessorBase.h>
#include "MultiDelayDsp.h"

class MultiDelayProcessor : public melon::ProcessorBase
{
public:
    MultiDelayProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    // Parameters as the engine uses them (Time already resolved from Sync + host tempo)
    mdelay::Params currentParams() const;

    double hostBpm() const { return bpm.load(); }

    // Tap tempo (called from the editor): sets Time and turns Sync off
    void tap();

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    mdelay::Engine      engine;
    std::atomic<double> bpm { 0.0 };
    double              lastTapMs = -1.0;
    std::vector<double> tapIntervals;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiDelayProcessor)
};
