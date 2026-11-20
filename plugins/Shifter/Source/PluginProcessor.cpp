#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

ShifterProcessor::ShifterProcessor() : ProcessorBase (createParameterLayout(), 10.0)
{
    presets.setFactoryPresets (shifter::factoryPresets());
}

// Whole semitones, shown as "+7 st"
static void addSemitones (melon::params::Layout& l, const juce::String& id, const juce::String& name, float def)
{
    const auto max = (float) shifter::kMaxSemitones;
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (-max, max, 1.0f), def,
        melon::params::Attr().withLabel ("st")
            .withStringFromValueFunction ([] (float v, int) {
                const int s = juce::roundToInt (v);
                return (s > 0 ? "+" : "") + juce::String (s) + " st";
            })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));
}

// Cents, shown as "+7 ct"
static void addCents (melon::params::Layout& l, const juce::String& id, const juce::String& name)
{
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (-50.0f, 50.0f, 1.0f), 0.0f,
        melon::params::Attr().withLabel ("ct")
            .withStringFromValueFunction ([] (float v, int) {
                const int c = juce::roundToInt (v);
                return (c > 0 ? "+" : "") + juce::String (c) + " ct";
            })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));
}

melon::params::Layout ShifterProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    addSemitones (l, "pitchA", "Pitch A", 12.0f);
    addCents (l, "detuneA", "Detune A");
    addSemitones (l, "pitchB", "Pitch B", -12.0f);
    addCents (l, "detuneB", "Detune B");
    addBool (l, "voiceB", "Voice B", false);
    addChoice (l, "mode", "Mode", { "Tight", "Normal", "Smooth" }, 1);
    addPercent (l, "width", "Width", 60.0f);
    addFloat (l, "delay", "Delay", 0.0f, (float) shifter::kMaxDelayMs, 0.0f, "ms", 0, 150.0f, 1.0f);
    addPercent (l, "feedback", "Feedback", 0.0f);
    addFloat (l, "tone", "Tone", 1000.0f, 20000.0f, 20000.0f, "Hz", 0, 5000.0f);
    addPercent (l, "mix", "Mix", 50.0f);
    addOutputAndBypass (l);
    return l;
}

shifter::Params ShifterProcessor::currentParams() const
{
    shifter::Params p;
    p.pitchA   = juce::roundToInt (value ("pitchA"));
    p.detuneA  = value ("detuneA");
    p.pitchB   = juce::roundToInt (value ("pitchB"));
    p.detuneB  = value ("detuneB");
    p.voiceB   = flag ("voiceB");
    p.mode     = (shifter::Mode) choice ("mode");
    p.width    = value ("width") * 0.01;
    p.delayMs  = value ("delay");
    p.feedback = value ("feedback") * 0.01;
    p.toneHz   = value ("tone");
    p.mix      = value ("mix") * 0.01;
    p.outputDb = value ("output");
    p.bypassed = flag ("bypass");
    return p;
}

void ShifterProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
}

void ShifterProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* ShifterProcessor::createEditor() { return new ShifterEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShifterProcessor(); }
