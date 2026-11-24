#pragma once

#include <MelonProcessorBase.h>
#include "TremDsp.h"

class TremProcessor : public melon::ProcessorBase
{
public:
    TremProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    // Parameters as the engine uses them (Rate already resolved from Sync + host tempo)
    trem::Params  currentParams() const;
    trem::Meters& meters() { return engine.meterValues(); }

    double hostBpm() const { return bpm.load(); }
    bool   isSynced() const { return choice ("sync") > 0 && bpm.load() > 0.0; }

    // Tap tempo (called from the editor): sets Rate and turns Sync off
    void tap();

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    trem::Engine        engine;
    std::atomic<double> bpm { 0.0 };
    double              lastTapMs = -1.0;
    std::vector<double> tapIntervals;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TremProcessor)
};
