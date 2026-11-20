#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

ReverberareProcessor::ReverberareProcessor() : ProcessorBase (createParameterLayout(), 30.0)
{
    presets.setFactoryPresets (reverb::factoryPresets());
}

melon::params::Layout ReverberareProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray names;
    for (const auto& m : reverb::machines())
        names.add (m.name);

    addChoice (l, "machine", "Machine", names, reverb::hall);
    addFloat (l, "decay", "Decay", 0.1f, 60.0f, 3.0f, "s", 2, 3.0f);
    addFloat (l, "predelay", "Pre-delay", 0.0f, (float) reverb::kMaxPreDelayMs, 20.0f, "ms", 0, 60.0f);
    addPercent (l, "tone", "Tone", 50.0f);
    addPercent (l, "mod", "Mod", 30.0f);
    addPercent (l, "param1", "Param 1", 50.0f);
    addPercent (l, "param2", "Param 2", 50.0f);
    addPercent (l, "mix", "Mix", 35.0f);
    addBool (l, "infinite", "Infinite", false);
    addOutputAndBypass (l);
    return l;
}

reverb::Params ReverberareProcessor::currentParams() const
{
    reverb::Params p;
    p.machine    = choice ("machine");
    p.decayS     = value ("decay");
    p.preDelayMs = value ("predelay");
    p.tone       = value ("tone") * 0.01;
    p.mod        = value ("mod") * 0.01;
    p.param1     = value ("param1") * 0.01;
    p.param2     = value ("param2") * 0.01;
    p.mix        = value ("mix") * 0.01;
    p.infinite   = flag ("infinite");
    p.outputDb   = value ("output");
    p.bypassed   = flag ("bypass");
    return p;
}

void ReverberareProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, numChannels);
    engine.snapToTargets();
}

void ReverberareProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* ReverberareProcessor::createEditor() { return new ReverberareEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverberareProcessor(); }
