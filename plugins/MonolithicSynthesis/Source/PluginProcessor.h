#pragma once

#include <MelonDsp.h>
#include <MelonProcessorBase.h>
#include "MonolithicEngine.h"

class MonolithicSynthesisProcessor : public melon::ProcessorBase
{
public:
    MonolithicSynthesisProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    melon::mono::EngineParams currentParams() const;

    // MIDI input (used when the "MIDI In" switch is on)
    bool acceptsMidi() const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // Read by the editor's display
    struct Meters
    {
        std::atomic<float> pitch { 0.0f };   // tracked / played note (MIDI number, fractional)
        std::atomic<float> level { 0.0f };   // input envelope (linear)
        std::atomic<bool>  active { false };
        std::atomic<int>   step { -1 };      // SEQ / ARPEGGIO step, -1 = none
    };
    Meters& meters() { return meterValues; }
    double  hostBpm() const { return bpm.load(); }

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    melon::mono::MonolithicEngine engine;
    melon::Smoother               active, outputGain;
    std::vector<float>            input, outL, outR;
    Meters                        meterValues;
    std::atomic<double>           bpm { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonolithicSynthesisProcessor)
};
