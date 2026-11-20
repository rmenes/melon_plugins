#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <MelonPresets.h>
#include "ChorusDsp.h"

class ChorusProcessor : public juce::AudioProcessor
{
public:
    ChorusProcessor();
    ~ChorusProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.02; }

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

    chorus::Params currentParams() const;
    float lfoPhase() const { return uiPhase.load(); }

    juce::AudioProcessorValueTreeState apvts;
    melon::PresetManager presets { *this, JucePlugin_Name }; // after apvts

private:
    chorus::Engine engine;
    std::atomic<float> uiPhase { 0.0f };

    std::atomic<float>* rateParam   = nullptr;
    std::atomic<float>* depthParam  = nullptr;
    std::atomic<float>* widthParam  = nullptr;
    std::atomic<float>* mixParam    = nullptr;
    std::atomic<float>* toneParam   = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusProcessor)
};
