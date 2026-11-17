#include "PluginProcessor.h"
#include "PluginEditor.h"

DirtProcessor::DirtProcessor() : ProcessorBase (createParameterLayout(), 0.1) {}

melon::params::Layout DirtProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray names;
    for (const auto& m : dirt::models())
        names.add (m.name);

    addChoice (l, "model", "Model", names, 0);
    addPercent (l, "drive", "Drive", 50.0f);
    addPercent (l, "tone", "Tone", 50.0f);
    addPercent (l, "level", "Level", 50.0f);
    addPercent (l, "mix", "Mix", 100.0f);
    addOutputAndBypass (l);
    return l;
}

dirt::Params DirtProcessor::currentParams() const
{
    dirt::Params p;
    p.model    = choice ("model");
    p.drive    = value ("drive") * 0.01;
    p.tone     = value ("tone") * 0.01;
    p.level    = value ("level") * 0.01;
    p.mix      = value ("mix") * 0.01;
    p.outputDb = value ("output");
    p.bypassed = flag ("bypass");
    return p;
}

void DirtProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, numChannels);
    engine.snapToTargets();
}

void DirtProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* DirtProcessor::createEditor() { return new DirtEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DirtProcessor(); }
