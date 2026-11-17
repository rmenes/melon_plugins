#include "PluginProcessor.h"
#include "PluginEditor.h"

GatesOfDoomProcessor::GatesOfDoomProcessor() : ProcessorBase (createParameterLayout(), 0.0) {}

melon::params::Layout GatesOfDoomProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    addChoice (l, "mode", "Mode", { "Classic", "Clean", "Guitar", "Ducking" }, 0);
    addFloat (l, "threshold", "Threshold", -80.0f, 0.0f, -40.0f, "dB", 1);
    addFloat (l, "range", "Range", 0.0f, 100.0f, 60.0f, "dB", 1);
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ratio", 1 }, "Ratio",
        [] { juce::NormalisableRange<float> r (1.0f, 100.0f, 0.01f); r.setSkewForCentre (4.0f); return r; }(), 100.0f,
        Attr().withStringFromValueFunction ([] (float v, int) { return v >= 99.95f ? juce::String ("Gate") : juce::String (v, 1) + ":1"; })
              .withValueFromStringFunction ([] (const juce::String& s) { return s.startsWithIgnoreCase ("g") ? 100.0f : s.getFloatValue(); })));
    addFloat (l, "knee", "Knee", 0.0f, 30.0f, 3.0f, "dB", 1);
    addFloat (l, "hysteresis", "Hysteresis", 0.0f, 20.0f, 3.0f, "dB", 1);
    addFloat (l, "attack", "Attack", 0.01f, 100.0f, 0.5f, "ms", 2, 3.0f);
    addFloat (l, "hold", "Hold", 0.0f, 1000.0f, 50.0f, "ms", 0, 80.0f);
    addFloat (l, "release", "Release", 5.0f, 3000.0f, 150.0f, "ms", 0, 200.0f);
    addFloat (l, "lookahead", "Lookahead", 0.0f, (float) gate::kMaxLookaheadMs, 0.0f, "ms", 1);
    addFloat (l, "schp", "SC High-pass", 20.0f, 2000.0f, 20.0f, "Hz", 0, 200.0f);
    addFloat (l, "sclp", "SC Low-pass", 1000.0f, 20000.0f, 20000.0f, "Hz", 0, 5000.0f);
    addPercent (l, "mix", "Mix", 100.0f);
    addBool (l, "audition", "Audition side-chain", false);
    addOutputAndBypass (l);
    return l;
}

gate::Params GatesOfDoomProcessor::currentParams() const
{
    gate::Params p;
    p.mode         = (gate::Mode) choice ("mode");
    p.thresholdDb  = value ("threshold");
    p.rangeDb      = value ("range");
    p.ratio        = value ("ratio");
    p.kneeDb       = value ("knee");
    p.hysteresisDb = value ("hysteresis");
    p.attackMs     = value ("attack");
    p.holdMs       = value ("hold");
    p.releaseMs    = value ("release");
    p.lookaheadMs  = value ("lookahead");
    p.scHighPassHz = value ("schp");
    p.scLowPassHz  = value ("sclp");
    p.mix          = value ("mix") * 0.01;
    p.audition     = flag ("audition");
    p.outputDb     = value ("output");
    p.bypassed     = flag ("bypass");
    return p;
}

void GatesOfDoomProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
    setLatencySamples (engine.latencySamples());
}

void GatesOfDoomProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    engine.setParams (currentParams());
    if (engine.latencySamples() != getLatencySamples())
        setLatencySamples (engine.latencySamples()); // the host re-aligns the track when Lookahead changes
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* GatesOfDoomProcessor::createEditor() { return new GatesOfDoomEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GatesOfDoomProcessor(); }
