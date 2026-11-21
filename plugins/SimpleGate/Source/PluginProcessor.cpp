#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

SimpleGateProcessor::SimpleGateProcessor() : ProcessorBase (createParameterLayout(), 0.0)
{
    presets.setFactoryPresets (sgate::factoryPresets());
}

melon::params::Layout SimpleGateProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    addFloat (l, "threshold", "Threshold", -90.0f, -10.0f, -55.0f, "dB", 1);
    addFloat (l, "hold", "Hold", 0.0f, 500.0f, 30.0f, "ms", 0, 60.0f);
    addFloat (l, "release", "Release", 5.0f, 1000.0f, 80.0f, "ms", 0, 120.0f);
    addChoice (l, "instrument", "Instrument", { "Guitar", "Bass" }, 0);
    addOutputAndBypass (l);
    return l;
}

sgate::Params SimpleGateProcessor::currentParams() const
{
    sgate::Params p;
    p.thresholdDb = value ("threshold");
    p.holdMs      = value ("hold");
    p.releaseMs   = value ("release");
    p.instrument  = (sgate::Instrument) choice ("instrument");
    p.outputDb    = value ("output");
    p.bypassed    = flag ("bypass");
    return p;
}

void SimpleGateProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
}

void SimpleGateProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* SimpleGateProcessor::createEditor() { return new SimpleGateEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SimpleGateProcessor(); }
