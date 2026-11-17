#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GuitarSynthDsp.h"

class GuitarSynthProcessor : public juce::AudioProcessor
{
public:
    GuitarSynthProcessor();
    ~GuitarSynthProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 1.0; } // high Resonance rings for a moment

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

    gsynth::Params currentParams() const;

    // Live values for the display
    float  currentCutoffHz() const { return uiCutoff.load(); }
    float  currentLevel() const { return uiLevel.load(); }
    float  currentPitchHz() const { return uiPitch.load(); }
    int    triggerCount() const { return uiTriggers.load(); }
    double sampleRateForDisplay() const { return uiSampleRate.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    gsynth::Engine engine;
    std::atomic<float>  uiCutoff { 300.0f }, uiLevel { 0.0f }, uiPitch { 0.0f };
    std::atomic<int>    uiTriggers { 0 };
    std::atomic<double> uiSampleRate { 48000.0 };

    std::atomic<float>* triggerParam   = nullptr;
    std::atomic<float>* subParam       = nullptr;
    std::atomic<float>* guitarParam    = nullptr;
    std::atomic<float>* octaveParam    = nullptr;
    std::atomic<float>* squareParam    = nullptr;
    std::atomic<float>* attackParam    = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* startParam     = nullptr;
    std::atomic<float>* stopParam      = nullptr;
    std::atomic<float>* rateParam      = nullptr;
    std::atomic<float>* mixParam       = nullptr;
    std::atomic<float>* outputParam    = nullptr;
    std::atomic<float>* bypassParam    = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarSynthProcessor)
};
