// MelonProcessorBase.h - boilerplate shared by the Melon effect plugins (JUCE).
// Handles buses (mono, stereo, mono -> stereo), state saving, presets, the bypass parameter
// and parameter helpers, so each plugin only writes its parameters and its DSP.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MelonPresets.h"

namespace melon
{
//==============================================================================
namespace params
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Attr   = juce::AudioParameterFloatAttributes;

inline Attr withUnit (const juce::String& unit, int decimals)
{
    return Attr().withLabel (unit)
        .withStringFromValueFunction ([unit, decimals] (float v, int) {
            return juce::String (v, decimals) + (unit.isEmpty() ? "" : " " + unit);
        })
        .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
}

inline void addPercent (Layout& l, const juce::String& id, const juce::String& name, float def)
{
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
        Attr().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));
}

// Range with an optional skew so that `centre` sits in the middle of the knob.
inline void addFloat (Layout& l, const juce::String& id, const juce::String& name, float lo, float hi, float def,
                      const juce::String& unit, int decimals, float centre = 0.0f, float step = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    if (centre > lo && centre < hi)
        r.setSkewForCentre (centre);
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def, withUnit (unit, decimals)));
}

inline void addDb (Layout& l, const juce::String& id, const juce::String& name, float lo, float hi, float def)
{
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, 0.1f), def,
        Attr().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));
}

inline void addChoice (Layout& l, const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
{
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
}

inline void addBool (Layout& l, const juce::String& id, const juce::String& name, bool def)
{
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}

// The series' standard header controls
inline void addOutputAndBypass (Layout& l)
{
    addDb (l, "output", "Output gain", -24.0f, 12.0f, 0.0f);
    addBool (l, "bypass", "Bypass", false);
}
} // namespace params

//==============================================================================
class ProcessorBase : public juce::AudioProcessor
{
public:
    ProcessorBase (params::Layout layout, double tailSeconds)
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMETERS", std::move (layout)),
          presets (*this, JucePlugin_Name),
          tail (tailSeconds)
    {
    }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager                      presets; // derived classes call presets.setFactoryPresets (...)

    // Raw value of a parameter (real units, as shown in the host)
    float value (const char* id) const
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load();
        jassertfalse;
        return 0.0f;
    }
    int  choice (const char* id) const { return juce::roundToInt (value (id)); }
    bool flag (const char* id) const { return value (id) > 0.5f; }

    double sampleRateForDisplay() const { return uiSampleRate.load(); }

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        uiSampleRate = sampleRate;
        prepareDsp (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    }
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto in  = layouts.getMainInputChannelSet();
        const auto out = layouts.getMainOutputChannelSet();
        if (out == juce::AudioChannelSet::stereo())
            return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
        if (out == juce::AudioChannelSet::mono())
            return in == juce::AudioChannelSet::mono();
        return false;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
        const int n     = buffer.getNumSamples();

        if (numIn == 1 && numOut >= 2)
            buffer.copyFrom (1, 0, buffer, 0, 0, n);
        else
            for (int ch = numIn; ch < numOut; ++ch)
                buffer.clear (ch, 0, n);

        processDsp (buffer.getArrayOfWritePointers(), numOut, n);
    }
    using AudioProcessor::processBlock;

    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi() const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return tail; }

    // The host's program list is the factory presets
    int  getNumPrograms() override { return presets.getNumPrograms(); }
    int  getCurrentProgram() override { return presets.getCurrentProgram(); }
    void setCurrentProgram (int index) override { presets.setCurrentProgram (index); }
    const juce::String getProgramName (int index) override { return presets.getProgramName (index); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        if (auto xml = createStateXml())
            copyXmlToBinary (*xml, destData);
    }
    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
            {
                auto presetInfo = presets.extractState (*xml);
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
                presets.restoreState (presetInfo.get());
            }
    }

    // Parameters + current preset, as saved in the project
    std::unique_ptr<juce::XmlElement> createStateXml()
    {
        auto xml = apvts.copyState().createXml();
        if (xml != nullptr)
            presets.writeState (*xml);
        return xml;
    }

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

protected:
    virtual void prepareDsp (double sampleRate, int maxBlock, int numChannels) = 0;
    virtual void processDsp (float* const* data, int numChannels, int numSamples) = 0;

private:
    double tail;
    std::atomic<double> uiSampleRate { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorBase)
};

} // namespace melon
