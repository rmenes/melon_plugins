#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

FlangerProcessor::FlangerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    presets.setFactoryPresets (flanger::factoryPresets());

    manualParam = apvts.getRawParameterValue ("manual");
    widthParam  = apvts.getRawParameterValue ("width");
    speedParam  = apvts.getRawParameterValue ("speed");
    regenParam  = apvts.getRawParameterValue ("regen");
    mixParam    = apvts.getRawParameterValue ("mix");
    outputParam = apvts.getRawParameterValue ("output");
    bypassParam = apvts.getRawParameterValue ("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout FlangerProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Manual: centre of the sweep, in ms. Skewed so the knob moves evenly in octaves.
    juce::NormalisableRange<float> manualRange ((float) flanger::kMinManualMs, (float) flanger::kMaxManualMs, 0.01f);
    manualRange.setSkewForCentre ((float) std::sqrt (flanger::kMinManualMs * flanger::kMaxManualMs));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "manual", 1 }, "Manual", manualRange, 2.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " ms"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));

    auto percent = juce::AudioParameterFloatAttributes()
                       .withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })
                       .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    const juce::NormalisableRange<float> percentRange (0.0f, 100.0f, 0.1f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "width", 1 }, "Width", percentRange, 60.0f, percent));

    // Speed: skewed so the slow, sweeping speeds get most of the knob travel
    juce::NormalisableRange<float> speedRange ((float) flanger::kMinSpeedHz, (float) flanger::kMaxSpeedHz, 0.01f);
    speedRange.setSkewForCentre (0.8f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "speed", 1 }, "Speed", speedRange, 0.4f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "regen", 1 }, "Regen", percentRange, 50.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Mix", percentRange, 50.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output gain", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

flanger::Params FlangerProcessor::currentParams() const
{
    flanger::Params p;
    p.manualMs = manualParam->load();
    p.width    = widthParam->load() * 0.01;
    p.speedHz  = speedParam->load();
    p.regen    = regenParam->load() * 0.01;
    p.mix      = mixParam->load() * 0.01;
    p.outputDb = outputParam->load();
    p.bypassed = bypassParam->load() > 0.5f;
    return p;
}

void FlangerProcessor::prepareToPlay (double sampleRate, int)
{
    uiSampleRate = sampleRate;
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()));
    engine.setParams (currentParams());
    engine.snapToTargets();
}

bool FlangerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    // mono → mono, stereo → stereo, and mono → stereo (a guitar on a stereo track)
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
    if (out == juce::AudioChannelSet::mono())
        return in == juce::AudioChannelSet::mono();
    return false;
}

void FlangerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    else
        for (int ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, numSamples);

    // The engine handles bypass itself with a short crossfade, so switching never clicks
    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), numOut, numSamples);
    uiDelayMs = (float) engine.currentDelayMs();
}

juce::AudioProcessorParameter* FlangerProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void FlangerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        presets.writeState (*xml);
        copyXmlToBinary (*xml, destData);
    }
}

void FlangerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto presetInfo = presets.extractState (*xml);
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presets.restoreState (presetInfo.get());
        }
}

juce::AudioProcessorEditor* FlangerProcessor::createEditor()
{
    return new FlangerEditor (*this);
}

// Entry point required by JUCE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlangerProcessor();
}
