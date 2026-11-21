#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

TunerProcessor::TunerProcessor() : ProcessorBase (createParameterLayout(), 0.0)
{
    presets.setFactoryPresets (tuner::factoryPresets());
}

melon::params::Layout TunerProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray tuningNames;
    for (const auto& t : tuner::tunings())
        tuningNames.add (t.name);

    addChoice (l, "tuning", "Tuning", tuningNames, 0);
    addFloat (l, "reference", "Reference A4", 410.0f, 460.0f, 440.0f, "Hz", 1, 0.0f, 0.1f);
    addDb (l, "threshold", "Threshold", -90.0f, -20.0f, -60.0f);
    addPercent (l, "smoothing", "Smoothing", 50.0f);
    addChoice (l, "display", "Display", { "Needle", "Strobe" }, 0);
    addBool (l, "mute", "Mute when engaged", false); // id stays "mute" (saved presets/projects)
    addOutputAndBypass (l);
    return l;
}

tuner::Params TunerProcessor::currentParams() const
{
    tuner::Params p;
    p.tuning      = choice ("tuning");
    p.reference   = value ("reference");
    p.thresholdDb = value ("threshold");
    p.smoothing   = value ("smoothing") * 0.01;
    p.display     = choice ("display");
    p.mute        = flag ("mute");
    p.outputDb    = value ("output");
    p.bypassed    = flag ("bypass");
    return p;
}

void TunerProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
}

void TunerProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* TunerProcessor::createEditor() { return new TunerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TunerProcessor(); }
