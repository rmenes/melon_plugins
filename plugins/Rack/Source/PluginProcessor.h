#pragma once

#include <MelonProcessorBase.h>
#include <juce_dsp/juce_dsp.h>
#include "NeuralModelSlot.h"
#include "RackDsp.h"

// Melon Rack - neural amp model + impulse response loader.
// Signal chain (mono in, stereo out), after NeuralRack by brummer10:
//
//   Input -> Gate -> [EQ] -> Pedal model -> [EQ] -> Amp model -> [EQ] -> Cabinet (2 IRs) -> Output
//
// The EQ sits in one of the three [EQ] places (parameter "EQ Position").
class RackProcessor : public melon::ProcessorBase, private juce::AsyncUpdater
{
public:
    RackProcessor();
    ~RackProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    static constexpr int kNumModels = 2; // 0 = Pedal, 1 = Amp
    static constexpr int kNumIrs    = 2; // 0 = IR 1 (left), 1 = IR 2 (right)

    struct SlotStatus
    {
        juce::File   file;           // empty = nothing loaded
        juce::String info;           // e.g. "48 kHz, resampled" or the error
        bool         loading = false;
        bool         error   = false;
    };

    // Called from the editor (message thread). Loading happens on a background thread.
    void loadModel (int slot, const juce::File& file);
    void clearModel (int slot);
    void loadIr (int slot, const juce::File& file);
    void clearIr (int slot);

    SlotStatus modelStatus (int slot) const;
    SlotStatus irStatus (int slot) const;

    int  latencyForDisplay() const { return getLatencySamples(); }
    bool isGateOpen() const { return gateOpen.load(); }
    bool isResampling (int slot) const { return modelResampling[slot].load(); }
    double hostSampleRate() const { return currentRate.load(); }

    // Folders the file choosers open in (remembered while the plugin is open)
    juce::File lastModelFolder, lastIrFolder;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static const juce::String modelWildcard, irWildcard;

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    struct ModelSlot
    {
        juce::SpinLock                     lock; // swap between loader thread and audio thread
        std::unique_ptr<rack::LoadedModel> model;
        rack::CompensationDelay            offDelay; // keeps timing when the slot is switched off
        melon::Smoother                    inGain, outGain;
        SlotStatus                         status;   // guarded by statusLock
        int                                generation = 0;
    };

    void processChunk (float* const* data, int numChannels, int offset, int n);
    void runModel (int slot, bool on, float inDb, float outDb, bool normalize, float* x, int n);
    void updateLatency();
    void handleAsyncUpdate() override { updateLatency(); }
    static double readModelSampleRate (const juce::File& file);

    ModelSlot slots[kNumModels];

    juce::dsp::Convolution convolution[kNumIrs];
    std::atomic<bool>      irActive[kNumIrs] { { false }, { false } };
    SlotStatus             irState[kNumIrs]; // guarded by statusLock

    rack::Gate              gate;
    rack::Eq6               eq;
    rack::CompensationDelay dryDelay[2];
    melon::Smoother         inputGain, cabLevel, cabBlend, outputGain, wetMix;

    std::vector<float> mono, cabA, cabB, dryL, dryR;
    int maxBlockSize = 512;

    std::atomic<double> currentRate { 48000.0 };
    std::atomic<int>    currentBlock { 512 };
    std::atomic<bool>   gateOpen { false };
    std::atomic<bool>   modelResampling[kNumModels] { { false }, { false } };

    mutable juce::CriticalSection statusLock;
    juce::ThreadPool              loader { 1 }; // declared last: destroyed first, waits for running loads

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackProcessor)
};
