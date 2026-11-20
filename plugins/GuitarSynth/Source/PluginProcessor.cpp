#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

GuitarSynthProcessor::GuitarSynthProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    presets.setFactoryPresets (gsynth::factoryPresets());

    triggerParam   = apvts.getRawParameterValue ("trigger");
    subParam       = apvts.getRawParameterValue ("suboctave");
    guitarParam    = apvts.getRawParameterValue ("guitar");
    octaveParam    = apvts.getRawParameterValue ("octave");
    squareParam    = apvts.getRawParameterValue ("square");
    attackParam    = apvts.getRawParameterValue ("attack");
    resonanceParam = apvts.getRawParameterValue ("resonance");
    startParam     = apvts.getRawParameterValue ("start");
    stopParam      = apvts.getRawParameterValue ("stop");
    rateParam      = apvts.getRawParameterValue ("rate");
    mixParam       = apvts.getRawParameterValue ("mix");
    outputParam    = apvts.getRawParameterValue ("output");
    bypassParam    = apvts.getRawParameterValue ("bypass");

    setLatencySamples (1); // the band-limited square edges need one sample of look-back
}

static juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 9995.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}

static float hzFromText (const juce::String& s)
{
    const auto v = s.getFloatValue();
    return s.containsIgnoreCase ("k") ? v * 1000.0f : v;
}

static juce::String secondsText (double s)
{
    return s < 1.0 ? juce::String (juce::roundToInt (s * 1000.0)) + " ms" : juce::String (s, 2) + " s";
}

static double secondsFromText (const juce::String& s)
{
    const auto v = s.getDoubleValue();
    return s.containsIgnoreCase ("ms") ? v * 0.001 : v;
}

juce::AudioProcessorValueTreeState::ParameterLayout GuitarSynthProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = juce::AudioParameterFloatAttributes()
                       .withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })
                       .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    const juce::NormalisableRange<float> percentRange (0.0f, 100.0f, 0.1f);
    auto addPercent = [&] (const char* id, const char* name, float def) {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, percentRange, def, percent));
    };

    // Same order as the sliders on the pedal
    addPercent ("trigger", "Trigger", 50.0f);
    addPercent ("suboctave", "Sub Octave", 40.0f);
    addPercent ("guitar", "Guitar", 30.0f);
    addPercent ("octave", "Octave", 0.0f);
    addPercent ("square", "Square Wave", 60.0f);

    // Attack Delay: fade-in time, skewed so the short, "bowed" times get most of the travel
    juce::NormalisableRange<float> attackRange (0.0f, (float) (gsynth::kMaxAttackS * 1000.0), 1.0f);
    attackRange.setSkewForCentre (300.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack Delay", attackRange, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? juce::String ("Off") : secondsText (v * 0.001); })
            .withValueFromStringFunction ([] (const juce::String& s) {
                return s.containsIgnoreCase ("off") ? 0.0f : (float) (secondsFromText (s.containsIgnoreCase ("s") ? s : s + " ms") * 1000.0);
            })));

    addPercent ("resonance", "Resonance", 60.0f);

    // Start / Stop Freq: log scale
    juce::NormalisableRange<float> freqRange ((float) gsynth::kMinFreqHz, (float) gsynth::kMaxFreqHz, 1.0f);
    freqRange.setSkewForCentre ((float) std::sqrt (gsynth::kMinFreqHz * gsynth::kMaxFreqHz));
    auto hz = juce::AudioParameterFloatAttributes()
                  .withLabel ("Hz")
                  .withStringFromValueFunction ([] (float v, int) { return hzText (v); })
                  .withValueFromStringFunction ([] (const juce::String& s) { return hzFromText (s); });
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "start", 1 }, "Start Freq", freqRange, 3000.0f, hz));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "stop", 1 }, "Stop Freq", freqRange, 300.0f, hz));

    // Rate: higher = faster. Shown as the sweep time, which is what you hear.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "rate", 1 }, "Rate", percentRange, 50.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return secondsText (gsynth::sweepSeconds (v * 0.01)); })
            .withValueFromStringFunction ([] (const juce::String& s) {
                return (float) (100.0 * gsynth::rateForSweepSeconds (secondsFromText (s.containsIgnoreCase ("s") ? s : s + " ms")));
            })));

    // Mix: dry/wet. 100 % = the pedal (dry guitar only through the Guitar voice)
    addPercent ("mix", "Mix", 100.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output gain", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

gsynth::Params GuitarSynthProcessor::currentParams() const
{
    gsynth::Params p;
    p.trigger   = triggerParam->load() * 0.01;
    p.subOctave = subParam->load() * 0.01;
    p.guitar    = guitarParam->load() * 0.01;
    p.octave    = octaveParam->load() * 0.01;
    p.square    = squareParam->load() * 0.01;
    p.attackS   = attackParam->load() * 0.001;
    p.resonance = resonanceParam->load() * 0.01;
    p.startHz   = startParam->load();
    p.stopHz    = stopParam->load();
    p.rate      = rateParam->load() * 0.01;
    p.mix       = mixParam->load() * 0.01;
    p.outputDb  = outputParam->load();
    p.bypassed  = bypassParam->load() > 0.5f;
    return p;
}

void GuitarSynthProcessor::prepareToPlay (double sampleRate, int)
{
    uiSampleRate = sampleRate;
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()));
    engine.setParams (currentParams());
    engine.snapToTargets();
}

bool GuitarSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    // mono → mono, stereo → stereo, and mono → stereo (a guitar on a stereo track)
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
    if (out == juce::AudioChannelSet::mono())
        return in == juce::AudioChannelSet::mono();
    return false;
}

void GuitarSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    else
        for (int ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, numSamples);

    // The engine handles bypass itself with a short crossfade, so switching never clicks
    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), numOut, numSamples);

    uiCutoff   = (float) engine.currentCutoffHz();
    uiLevel    = juce::jmax ((float) engine.currentLevel(), uiLevel.load() * 0.9f); // peak hold between UI frames
    uiPitch    = (float) engine.currentPitchHz();
    uiTriggers = engine.triggers();
}

juce::AudioProcessorParameter* GuitarSynthProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void GuitarSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        presets.writeState (*xml);
        copyXmlToBinary (*xml, destData);
    }
}

void GuitarSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto presetInfo = presets.extractState (*xml);
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presets.restoreState (presetInfo.get());
        }
}

juce::AudioProcessorEditor* GuitarSynthProcessor::createEditor()
{
    return new GuitarSynthEditor (*this);
}

// Entry point required by JUCE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarSynthProcessor();
}
