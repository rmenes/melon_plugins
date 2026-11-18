#pragma once

#include <MelonProcessorBase.h>
#include "ProEqDsp.h"

class ProEqProcessor : public melon::ProcessorBase
{
public:
    ProEqProcessor();

    juce::AudioProcessorEditor* createEditor() override;
    static melon::params::Layout createParameterLayout();

    // Parameter ID of a band field, e.g. bandId (0, "freq") = "b1_freq"
    static juce::String bandId (int band, const char* field);

    // Band choice lists (shared with the editor)
    static const juce::StringArray& typeNames();
    static const juce::StringArray& slopeNames();
    static const juce::StringArray& placementNames();

    proeq::Params currentParams() const;
    const proeq::Engine& engine() const { return eng; }

    // Band being soloed (-1 = none). Not a parameter: it is a listening aid, not saved.
    std::atomic<int> soloBand { -1 };

    // Interface-only settings, saved with the session but not automatable
    static constexpr const char* kAnalyzerProp = "analyzer";     // 0 off, 1 pre, 2 post, 3 pre + post
    static constexpr const char* kRangeProp    = "displayRange"; // 0: 30, 1: 12, 2: 6, 3: 3 dB
    static constexpr const char* kSelectedProp = "selectedBand";

protected:
    void prepareDsp (double sampleRate, int maxBlock, int numChannels) override;
    void processDsp (float* const* data, int numChannels, int numSamples) override;

private:
    struct BandPointers
    {
        std::atomic<float>*used, *on, *type, *freq, *gain, *q, *slope, *stereo, *dynRange, *dynThreshold;
    };
    std::array<BandPointers, proeq::kNumBands> bandPtrs {};
    std::atomic<float>*phase, *resolution, *gainScale, *autoGain, *attack, *release, *output, *bypass;

    proeq::Engine eng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProEqProcessor)
};
