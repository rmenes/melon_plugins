#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

GraphicEqProcessor::GraphicEqProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    presets.setFactoryPresets (geq::factoryPresets());

    modeParam    = apvts.getRawParameterValue ("mode");
    preciseParam = apvts.getRawParameterValue ("precise");
    outputParam  = apvts.getRawParameterValue ("output");
    bypassParam  = apvts.getRawParameterValue ("bypass");

    for (int i = 0; i < 10; ++i)
        bands10[(size_t) i] = apvts.getRawParameterValue (bandParamId (10, i));
    for (int i = 0; i < 31; ++i)
        bands31[(size_t) i] = apvts.getRawParameterValue (bandParamId (31, i));
}

juce::String GraphicEqProcessor::bandParamId (int numBands, int index)
{
    return (numBands == 31 ? "b31_" : "b10_") + juce::String (index);
}

juce::AudioProcessorValueTreeState::ParameterLayout GraphicEqProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Bands", juce::StringArray { "10 bands", "31 bands" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "precise", 1 }, "Precise mode", true));

    auto dbAttributes = juce::AudioParameterFloatAttributes()
                            .withLabel ("dB")
                            .withStringFromValueFunction ([] (float v, int) {
                                return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
                            });

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output gain",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbAttributes));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    const auto range = juce::NormalisableRange<float> ((float) -geq::kSliderRangeDb, (float) geq::kSliderRangeDb, 0.1f);

    for (int n : { 10, 31 })
    {
        const auto& freqs = geq::bandFrequencies (n);
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
            n == 31 ? "bands31" : "bands10", n == 31 ? "31 bands" : "10 bands", " | ");

        for (int i = 0; i < n; ++i)
        {
            const auto name = juce::String (n) + "b " + juce::String (geq::frequencyLabel (freqs[(size_t) i])) + " Hz";
            group->addChild (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { bandParamId (n, i), 1 }, name, range, 0.0f, dbAttributes));
        }

        layout.add (std::move (group));
    }

    return layout;
}

int GraphicEqProcessor::currentNumBands() const { return modeParam->load() > 0.5f ? 31 : 10; }
bool GraphicEqProcessor::isPrecise() const { return preciseParam->load() > 0.5f; }

void GraphicEqProcessor::readSliderValues (int numBands, double* out) const
{
    for (int i = 0; i < numBands; ++i)
        out[i] = numBands == 31 ? (double) bands31[(size_t) i]->load() : (double) bands10[(size_t) i]->load();
}

void GraphicEqProcessor::prepareToPlay (double sampleRate, int)
{
    lastSampleRate = sampleRate;
    const int n = currentNumBands();
    engine.prepare (sampleRate, getTotalNumOutputChannels(), n);

    double values[geq::kMaxBands] {};
    readSliderValues (n, values);
    engine.setTargets (n, values, isPrecise());
    engine.setOutputGainDb (outputParam->load());
    engine.snapToTargets();
}

bool GraphicEqProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void GraphicEqProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (bypassParam->load() > 0.5f)
        return;

    const int n = currentNumBands();
    double values[geq::kMaxBands] {};
    readSliderValues (n, values);
    engine.setTargets (n, values, isPrecise());
    engine.setOutputGainDb (outputParam->load());

    engine.process (buffer.getArrayOfWritePointers(), juce::jmin (numIn, numOut), buffer.getNumSamples());
}

juce::AudioProcessorParameter* GraphicEqProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void GraphicEqProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        presets.writeState (*xml);
        copyXmlToBinary (*xml, destData);
    }
}

void GraphicEqProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto presetInfo = presets.extractState (*xml);
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presets.restoreState (presetInfo.get());
        }
}

juce::AudioProcessorEditor* GraphicEqProcessor::createEditor()
{
    return new GraphicEqEditor (*this);
}

// Entry point required by JUCE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GraphicEqProcessor();
}
