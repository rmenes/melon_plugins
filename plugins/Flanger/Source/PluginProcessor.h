#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <MelonPresets.h>
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

    // The host's program list is the factory presets
    int getNumPrograms() override { return presets.getNumPrograms(); }
    int getCurrentProgram() override { return presets.getCurrentProgram(); }
    void setCurrentProgram (int index) override { presets.setCurrentProgram (index); }
    const juce::String getProgramName (int index) override { return presets.getProgramName (index); }
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
    melon::PresetManager presets { *this, JucePlugin_Name }; // after apvts

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
