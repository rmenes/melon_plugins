#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FlangerDsp.h"

class FlangerProcessor : public juce::AudioProcessor
{
public:
    FlangerProcessor();
    ~FlangerProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 2.0; } // high Regen rings for a while

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

    flanger::Params currentParams() const;
    float  currentDelayMs() const { return uiDelayMs.load(); }
    double sampleRateForDisplay() const { return uiSampleRate.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    flanger::Engine engine;
    std::atomic<float>  uiDelayMs { 2.0f };
    std::atomic<double> uiSampleRate { 48000.0 };

    std::atomic<float>* manualParam = nullptr;
    std::atomic<float>* widthParam  = nullptr;
    std::atomic<float>* speedParam  = nullptr;
    std::atomic<float>* regenParam  = nullptr;
    std::atomic<float>* mixParam    = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangerProcessor)
};
