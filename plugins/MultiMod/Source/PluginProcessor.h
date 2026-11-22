#pragma once

#include <MelonProcessorBase.h>
#include "MultiModDsp.h"

class MultiModProcessor : public melon::ProcessorBase
{
public:
    MultiModProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    // Parameters as the engine uses them (Speed already resolved from Sync + host tempo)
    mmod::Params currentParams() const;

    double hostBpm() const { return bpm.load(); }
    double lfoPosition() const { return position.load(); } // for the display

    // Tap tempo (called from the editor): sets Speed and turns Sync off
    void tap();

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    mmod::Engine        engine;
    std::atomic<double> bpm { 0.0 }, position { 0.0 };
    double              lastTapMs = -1.0;
    std::vector<double> tapIntervals;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiModProcessor)
};
