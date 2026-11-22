#pragma once

#include <MelonProcessorBase.h>
#include <MelonFileLoader.h>
#include <MelonLibrary.h>
#include "NamDsp.h"
#include "NeuralModelSlot.h"

// Melon NAM - a simple Neural Amp Modeler (.nam) loader, after the official NeuralAmpModelerPlugin.
// Mono in, the same signal on both outputs:
//
//   Input -> Noise Gate -> NAM model -> Bass / Middle / Treble -> Output
//
// No cabinet: put Melon Cab (or any IR loader) after it.
class NamProcessor : public melon::ProcessorBase,
                     private juce::AsyncUpdater,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    NamProcessor();
    ~NamProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    static const juce::String modelWildcard;

    // Message thread. The model loads on a background thread.
    void             loadModel (const juce::File& file);
    void             clearModel();
    // Library folder used by the "model_index" parameter (the Melon Pedal sets it)
    juce::File       libraryFolder() const { return library.root(); }
    melon::LoadedFile modelStatus() const;

    juce::File lastModelFolder; // where the file chooser opens when nothing is loaded

    bool isGateOpen() const { return gateOpen.load(); }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    void processChunk (float* const* data, int numChannels, int offset, int n);
    void updateLatency();
    void handleAsyncUpdate() override
    {
        applyModelSize();
        updateLatency();
    }

    // "size" parameter (Full / Light) -> engine size 1.0 / 0.0. Changes can arrive on the audio thread
    // (host automation, mod-host patch_set), so they are applied later on the message thread.
    void  parameterChanged (const juce::String& parameterID, float newValue) override;
    float modelSize() const { return flag ("size") ? 0.0f : 1.0f; }
    void  applyModelSize();

    // Any thread: the parts of loadModel / clearModel that don't touch lastModelFolder
    void startModelLoad (const juce::File& file);
    void unloadModel (const juce::String& message, bool updateLatencyNow);

    juce::SpinLock                     modelLock; // swap between the loader thread and the audio thread
    std::unique_ptr<mnam::LoadedModel> model;
    std::atomic<bool>                  modelResampling { false };

    mutable juce::CriticalSection statusLock;
    melon::LoadedFile             status; // guarded by statusLock
    int                           generation = 0;

    mnam::Gate              gate;
    mnam::ToneStack         toneStack;
    mnam::CompensationDelay dryDelay[2];
    melon::Smoother         inputGain, modelGain, outputGain, wetMix;

    std::vector<float> mono, dryL, dryR;
    int maxBlockSize = 512;

    std::atomic<double> currentRate { 48000.0 };
    std::atomic<int>    currentBlock { 512 };
    std::atomic<bool>   gateOpen { false };

    juce::ThreadPool loader { 1 }; // destroyed before the members above, waits for a running load

    // model_index -> file in the library folder. Declared last: destroyed (stopped) first.
    melon::LibrarySelector library { apvts, "model_index", "MELON_NAM_DIR", "NAM", { "nam", "aidax", "json" } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamProcessor)
};
