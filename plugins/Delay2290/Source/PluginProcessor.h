#pragma once

#include <MelonProcessorBase.h>
#include "DelayDsp.h"

class Delay2290Processor : public melon::ProcessorBase
{
public:
    Delay2290Processor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    // Parameters as the engine uses them (Time already resolved from Sync + host tempo)
    delay2290::Params currentParams() const;

    double hostBpm() const { return bpm.load(); }
    double duckDb() const { return duck.load(); }

    // Tap tempo (called from the editor): sets Time and turns Sync off
    void tap();

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    delay2290::Engine   engine;
    std::atomic<double> bpm { 0.0 }, duck { 0.0 };
    double              lastTapMs = -1.0;
    std::vector<double> tapIntervals;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Delay2290Processor)
};
