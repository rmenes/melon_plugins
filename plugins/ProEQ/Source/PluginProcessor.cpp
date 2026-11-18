#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace melon::params;

namespace
{
juce::String freqText (float f)
{
    return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : juce::String (f, f >= 100.0f ? 0 : 1) + " Hz";
}

float freqFromText (const juce::String& s)
{
    const float v = s.getFloatValue();
    return s.containsIgnoreCase ("k") ? v * 1000.0f : v;
}

// Logarithmic range lo..hi
juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float a, float b, float x) { return a * std::pow (b / a, x); },
             [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
             [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
}

juce::String dbText (float v) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
} // namespace

ProEqProcessor::ProEqProcessor() : ProcessorBase (createParameterLayout(), 0.0)
{
    for (int b = 0; b < proeq::kNumBands; ++b)
    {
        auto get = [&] (const char* f) { return apvts.getRawParameterValue (bandId (b, f)); };
        bandPtrs[(size_t) b] = { get ("used"), get ("on"), get ("type"), get ("freq"), get ("gain"),
                                 get ("q"), get ("slope"), get ("stereo"), get ("dynrange"), get ("dynthr") };
    }
    phase      = apvts.getRawParameterValue ("phase");
    resolution = apvts.getRawParameterValue ("resolution");
    gainScale  = apvts.getRawParameterValue ("gainscale");
    autoGain   = apvts.getRawParameterValue ("autogain");
    attack     = apvts.getRawParameterValue ("attack");
    release    = apvts.getRawParameterValue ("release");
    output     = apvts.getRawParameterValue ("output");
    bypass     = apvts.getRawParameterValue ("bypass");

    if (! apvts.state.hasProperty (kAnalyzerProp)) apvts.state.setProperty (kAnalyzerProp, 3, nullptr);
    if (! apvts.state.hasProperty (kRangeProp))    apvts.state.setProperty (kRangeProp, 1, nullptr);
    if (! apvts.state.hasProperty (kSelectedProp)) apvts.state.setProperty (kSelectedProp, -1, nullptr);
}

juce::String ProEqProcessor::bandId (int band, const char* field) { return "b" + juce::String (band + 1) + "_" + field; }

const juce::StringArray& ProEqProcessor::typeNames()
{
    static const juce::StringArray s { "Bell", "Low Shelf", "Low Cut", "High Shelf", "High Cut", "Notch", "Band Pass", "Tilt Shelf" };
    return s;
}
const juce::StringArray& ProEqProcessor::slopeNames()
{
    static const juce::StringArray s = [] {
        juce::StringArray a;
        for (int v : proeq::kSlopes) a.add (juce::String (v) + " dB/oct");
        return a;
    }();
    return s;
}
const juce::StringArray& ProEqProcessor::placementNames()
{
    static const juce::StringArray s { "Stereo", "Left", "Right", "Mid", "Side" };
    return s;
}

Layout ProEqProcessor::createParameterLayout()
{
    Layout l;

    addChoice (l, "phase", "Phase Mode", { "Zero Latency", "Linear Phase" }, 0);
    addChoice (l, "resolution", "Linear Phase Resolution", { "Low", "Medium", "High" }, 1);
    addFloat (l, "gainscale", "Gain Scale", -100.0f, 200.0f, 100.0f, "%", 0, 0.0f, 1.0f);
    addBool (l, "autogain", "Auto Gain", false);
    addFloat (l, "attack", "Dynamic Attack", 0.1f, 200.0f, 5.0f, "ms", 1, 10.0f);
    addFloat (l, "release", "Dynamic Release", 5.0f, 3000.0f, 120.0f, "ms", 0, 200.0f);
    addOutputAndBypass (l);

    const auto dbAttr = Attr().withLabel ("dB")
                            .withStringFromValueFunction ([] (float v, int) { return dbText (v); })
                            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });

    for (int b = 0; b < proeq::kNumBands; ++b)
    {
        const juce::String n = "Band " + juce::String (b + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("band" + juce::String (b + 1), "Band " + juce::String (b + 1), " | ");
        auto id = [b] (const char* f) { return juce::ParameterID { bandId (b, f), 1 }; };

        group->addChild (std::make_unique<juce::AudioParameterBool> (id ("used"), n + "Used", false));
        group->addChild (std::make_unique<juce::AudioParameterBool> (id ("on"), n + "Enabled", true));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (id ("type"), n + "Type", typeNames(), 0));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id ("freq"), n + "Frequency", logRange ((float) proeq::kMinFreq, (float) proeq::kMaxFreq), 1000.0f,
            Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return freqText (v); })
                                   .withValueFromStringFunction (freqFromText)));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id ("gain"), n + "Gain", juce::NormalisableRange<float> (-30.0f, 30.0f, 0.01f), 0.0f, dbAttr));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id ("q"), n + "Q", logRange ((float) proeq::kMinQ, (float) proeq::kMaxQ), 1.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1); })
                  .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); })));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (id ("slope"), n + "Slope", slopeNames(), 1));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (id ("stereo"), n + "Stereo Placement", placementNames(), 0));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id ("dynrange"), n + "Dynamic Range", juce::NormalisableRange<float> (-30.0f, 30.0f, 0.01f), 0.0f, dbAttr));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id ("dynthr"), n + "Threshold", juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -30.0f, dbAttr));
        l.add (std::move (group));
    }
    return l;
}

proeq::Params ProEqProcessor::currentParams() const
{
    proeq::Params p;
    for (int b = 0; b < proeq::kNumBands; ++b)
    {
        const auto& s = bandPtrs[(size_t) b];
        auto& d = p.bands[(size_t) b];
        d.used           = s.used->load() > 0.5f;
        d.on             = s.on->load() > 0.5f;
        d.type           = (proeq::Type) juce::jlimit (0, proeq::kNumTypes - 1, juce::roundToInt (s.type->load()));
        d.freq           = s.freq->load();
        d.gainDb         = s.gain->load();
        d.q              = s.q->load();
        d.slope          = juce::jlimit (0, proeq::kNumSlopes - 1, juce::roundToInt (s.slope->load()));
        d.placement      = (proeq::Placement) juce::jlimit (0, 4, juce::roundToInt (s.stereo->load()));
        d.dynRangeDb     = s.dynRange->load();
        d.dynThresholdDb = s.dynThreshold->load();
    }
    p.phase      = phase->load() > 0.5f ? proeq::Phase::linear : proeq::Phase::zeroLatency;
    p.resolution = juce::roundToInt (resolution->load());
    p.gainScale  = gainScale->load() * 0.01;
    p.autoGain   = autoGain->load() > 0.5f;
    p.attackMs   = attack->load();
    p.releaseMs  = release->load();
    p.outputDb   = output->load();
    p.bypassed   = bypass->load() > 0.5f;
    p.solo       = soloBand.load();
    return p;
}

void ProEqProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    const auto p = currentParams();
    eng.prepare (sampleRate, numChannels);
    eng.setParams (p);
    eng.snapToTargets();
    setLatencySamples (p.phase == proeq::Phase::linear ? proeq::kernelSize (p.resolution, sampleRate) : 0);
}

void ProEqProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    eng.setParams (currentParams());
    eng.process (data, numChannels, numSamples);
    if (eng.latencySamples() != getLatencySamples())
        setLatencySamples (eng.latencySamples()); // the host re-aligns the track when the phase mode changes
}

juce::AudioProcessorEditor* ProEqProcessor::createEditor() { return new ProEqEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ProEqProcessor(); }
