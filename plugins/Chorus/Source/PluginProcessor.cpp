#include "PluginProcessor.h"
#include "PluginEditor.h"

ChorusProcessor::ChorusProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    rateParam   = apvts.getRawParameterValue ("rate");
    depthParam  = apvts.getRawParameterValue ("depth");
    widthParam  = apvts.getRawParameterValue ("width");
    mixParam    = apvts.getRawParameterValue ("mix");
    toneParam   = apvts.getRawParameterValue ("tone");
    outputParam = apvts.getRawParameterValue ("output");
    bypassParam = apvts.getRawParameterValue ("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout ChorusProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Rate: skewed so that the slow, musical speeds get most of the knob travel
    juce::NormalisableRange<float> rateRange ((float) chorus::kMinRateHz, (float) chorus::kMaxRateHz, 0.01f);
    rateRange.setSkewForCentre (1.2f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "rate", 1 }, "Rate", rateRange, 0.8f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));

    auto percent = juce::AudioParameterFloatAttributes()
                       .withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })
                       .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });

    const juce::NormalisableRange<float> percentRange (0.0f, 100.0f, 0.1f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "depth", 1 }, "Depth", percentRange, 50.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "width", 1 }, "Width", percentRange, 70.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 1 }, "Mix", percentRange, 50.0f, percent));

    // Tone: -100 = darkest, 0 = flat, +100 = brightest
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tone", 1 }, "Tone", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) -> juce::String {
                const int n = juce::roundToInt (v);
                if (n == 0)
                    return "Flat";
                return (n < 0 ? "Dark " : "Bright ") + juce::String (std::abs (n));
            })
            .withValueFromStringFunction ([] (const juce::String& s) {
                const auto t = s.trim().toLowerCase();
                if (t.startsWith ("flat"))
                    return 0.0f;
                const float v = t.retainCharacters ("0123456789.").getFloatValue();
                return t.startsWith ("dark") || t.startsWith ("-") ? -v : v;
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output gain", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

chorus::Params ChorusProcessor::currentParams() const
{
    chorus::Params p;
    p.rateHz   = rateParam->load();
    p.depth    = depthParam->load() * 0.01;
    p.width    = widthParam->load() * 0.01;
    p.mix      = mixParam->load() * 0.01;
    p.tone     = toneParam->load() * 0.01;
    p.outputDb = outputParam->load();
    p.bypassed = bypassParam->load() > 0.5f;
    return p;
}

void ChorusProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumOutputChannels()));
    engine.setParams (currentParams());
    engine.snapToTargets();
}

bool ChorusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    // mono → mono, stereo → stereo, and mono → stereo (a guitar straight into a stereo chorus)
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
    if (out == juce::AudioChannelSet::mono())
        return in == juce::AudioChannelSet::mono();
    return false;
}

void ChorusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Mono in, stereo out: send the guitar to both sides; Width then spreads the chorus
    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    else
        for (int ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, numSamples);

    // The engine handles bypass itself with a short crossfade, so switching never clicks
    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), numOut, numSamples);
    uiPhase = (float) engine.lfoPhase();
}

juce::AudioProcessorParameter* ChorusProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void ChorusProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ChorusProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ChorusProcessor::createEditor()
{
    return new ChorusEditor (*this);
}

// Entry point required by JUCE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChorusProcessor();
}
