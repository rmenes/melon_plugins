#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

CompProcessor::CompProcessor() : ProcessorBase (createParameterLayout(), 0.5)
{
    presets.setFactoryPresets (comp::factoryPresets());
}

melon::params::Layout CompProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    addPercent (l, "sustain", "Sustain", 50.0f);
    addPercent (l, "level", "Level", 60.0f);
    addPercent (l, "blend", "Blend", 75.0f);
    addPercent (l, "tone", "Tone", 50.0f);
    addChoice (l, "pickup", "Pickup", { "Single Coil", "Humbucker" }, 0);
    addOutputAndBypass (l);
    return l;
}

comp::Params CompProcessor::currentParams() const
{
    comp::Params p;
    p.sustain  = value ("sustain") * 0.01;
    p.level    = value ("level") * 0.01;
    p.blend    = value ("blend") * 0.01;
    p.tone     = value ("tone") * 0.01;
    p.pickup   = (comp::Pickup) choice ("pickup");
    p.outputDb = value ("output");
    p.bypassed = flag ("bypass");
    return p;
}

void CompProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
}

void CompProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* CompProcessor::createEditor() { return new CompEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CompProcessor(); }
