#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

MuffsProcessor::MuffsProcessor() : ProcessorBase (createParameterLayout(), 0.1)
{
    presets.setFactoryPresets (muffs::factoryPresets());
}

melon::params::Layout MuffsProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray names;
    for (const auto& m : muffs::models())
        names.add (m.name);

    addChoice (l, "model", "Version", names, 0);
    addPercent (l, "volume", "Volume", 50.0f);
    addPercent (l, "tone", "Tone", 50.0f);
    addPercent (l, "sustain", "Sustain", 50.0f);
    addPercent (l, "mix", "Mix", 100.0f);
    addOutputAndBypass (l);
    return l;
}

muffs::Params MuffsProcessor::currentParams() const
{
    muffs::Params p;
    p.model    = choice ("model");
    p.sustain  = value ("sustain") * 0.01;
    p.tone     = value ("tone") * 0.01;
    p.volume   = value ("volume") * 0.01;
    p.mix      = value ("mix") * 0.01;
    p.outputDb = value ("output");
    p.bypassed = flag ("bypass");
    return p;
}

void MuffsProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, numChannels);
    engine.snapToTargets();
}

void MuffsProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* MuffsProcessor::createEditor() { return new MuffsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MuffsProcessor(); }
