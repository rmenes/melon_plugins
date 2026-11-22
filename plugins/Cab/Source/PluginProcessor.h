#pragma once

#include <MelonProcessorBase.h>
#include <MelonFileLoader.h>
#include <MelonLibrary.h>
#include <juce_dsp/juce_dsp.h>
#include "CabDsp.h"

// Melon Cab - a simple impulse response (cabinet) loader.
//
//   Input -> IR (convolution) -> Low Cut / High Cut -> Mix with the dry signal -> Output
//
// Mono input (or a mono IR on a mono track): one convolution, the result on both outputs.
// Stereo input: each side through the IR (a stereo IR gives each side its own channel).
class CabProcessor : public melon::ProcessorBase
{
public:
    CabProcessor();
    ~CabProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    static const juce::String irWildcard;

    // Message thread (also from setStateInformation). The IR is resampled and swapped in
    // by juce::dsp::Convolution on its own background thread.
    void              loadIr (const juce::File& file);
    void              clearIr();
    melon::LoadedFile irStatus() const;
    // Library folder used by the "ir_index" parameter (the Melon Pedal sets it)
    juce::File        libraryFolder() const { return library.root(); }

    juce::File lastIrFolder; // where the file chooser opens when nothing is loaded

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    // Any thread: loadIr / clearIr without touching lastIrFolder
    void startIrLoad (const juce::File& file);
    void unloadIr (const juce::String& message);

    juce::CriticalSection  loadLock; // one IR load at a time (message thread and library thread)
    juce::dsp::Convolution convolution;
    std::atomic<bool>      irActive { false }, irStereo { false };

    mutable juce::CriticalSection statusLock;
    melon::LoadedFile             status; // guarded by statusLock

    cab::CutFilters filters;
    melon::Smoother wetGain, dryGain, outputGain, bypassMix;

    juce::AudioBuffer<float> dry;
    int maxBlockSize = 512;

    // ir_index -> file in the library folder. Declared last: destroyed (stopped) first.
    melon::LibrarySelector library { apvts, "ir_index", "MELON_IR_DIR", "IR", { "wav", "aif", "aiff", "flac" } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabProcessor)
};
