#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "EqDsp.h"

class GraphicEqProcessor : public juce::AudioProcessor
{
public:
    GraphicEqProcessor();
    ~GraphicEqProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String bandParamId (int numBands, int index);

    int    currentNumBands() const;
    bool   isPrecise() const;
    double currentSampleRate() const { return lastSampleRate.load(); }
    void   readSliderValues (int numBands, double* out) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    geq::Engine engine;
    std::atomic<double> lastSampleRate { 48000.0 };

    std::atomic<float>* modeParam    = nullptr;
    std::atomic<float>* preciseParam = nullptr;
    std::atomic<float>* outputParam  = nullptr;
    std::atomic<float>* bypassParam  = nullptr;
    std::array<std::atomic<float>*, 10> bands10 {};
    std::array<std::atomic<float>*, 31> bands31 {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphicEqProcessor)
};
