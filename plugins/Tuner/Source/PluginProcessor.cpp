#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

TunerProcessor::TunerProcessor() : ProcessorBase (createParameterLayout(), 0.0)
{
    readout = apvts.getParameter (kReadoutId);
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

    // Read-only output for hosts without the plugin's own display (the Melon pedal): the detected
    // frequency in Hz, 0 = no note. Only written in LV2 builds (sent to the host as patch:Set).
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kReadoutId, 1 }, "Tuner reading",
        juce::NormalisableRange<float> (0.0f, kReadoutMaxHz), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz").withAutomatable (false)
            .withCategory (juce::AudioProcessorParameter::outputMeter)));
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
    publishReading();
}

// LV2 only: the reading goes to the host as a parameter change (patch:Set on the notify port),
// once per new analysis (every ~20 ms) and only when it changed. VST3/AU hosts would record it
// as an edit, and they have the plugin's own display anyway.
void TunerProcessor::publishReading()
{
    if (wrapperType != wrapperType_LV2 || readout == nullptr)
        return;

    const auto& r      = engine.current();
    const auto  frames = r.frames.load (std::memory_order_relaxed);
    if (frames == lastFrames)
        return;
    lastFrames = frames;

    const float hz = juce::jlimit (0.0f, kReadoutMaxHz, r.frequency.load (std::memory_order_relaxed));
    if (std::abs (hz - lastReadout) < 0.005f)
        return;
    lastReadout = hz;
    readout->setValueNotifyingHost (readout->convertTo0to1 (hz));
}

juce::AudioProcessorEditor* TunerProcessor::createEditor() { return new TunerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TunerProcessor(); }
