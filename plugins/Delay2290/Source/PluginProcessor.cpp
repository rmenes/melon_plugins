#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

Delay2290Processor::Delay2290Processor() : ProcessorBase (createParameterLayout(), 10.0)
{
    presets.setFactoryPresets (delay2290::factoryPresets());
}

melon::params::Layout Delay2290Processor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray sync;
    for (const auto& s : delay2290::syncValues())
        sync.add (s.first);

    addFloat (l, "time", "Time", 1.0f, (float) delay2290::kMaxTimeMs, 400.0f, "ms", 0, 400.0f);
    addChoice (l, "sync", "Sync", sync, 0);
    addChoice (l, "mode", "Mode", { "Mono", "Stereo", "Ping-pong" }, 1);
    addPercent (l, "feedback", "Feedback", 35.0f);
    addPercent (l, "mix", "Mix", 30.0f);
    addFloat (l, "hicut", "Hi Cut", 1000.0f, 20000.0f, 12000.0f, "Hz", 0, 5000.0f);
    addFloat (l, "locut", "Lo Cut", 10.0f, 1000.0f, 60.0f, "Hz", 0, 100.0f);
    addPercent (l, "spread", "Spread", 10.0f);
    addPercent (l, "moddepth", "Mod Depth", 0.0f);
    addFloat (l, "modspeed", "Mod Speed", 0.05f, 10.0f, 0.8f, "Hz", 2, 1.0f);
    addFloat (l, "duckthresh", "Duck Threshold", -60.0f, 0.0f, -30.0f, "dB", 1);
    addFloat (l, "duckamount", "Duck Amount", 0.0f, 40.0f, 0.0f, "dB", 1);
    addFloat (l, "duckrelease", "Duck Release", 20.0f, 2000.0f, 300.0f, "ms", 0, 300.0f);
    addOutputAndBypass (l);
    return l;
}

delay2290::Params Delay2290Processor::currentParams() const
{
    delay2290::Params p;
    const double synced = delay2290::syncedTimeMs (choice ("sync"), bpm.load());
    p.timeMs        = synced > 0.0 ? synced : value ("time");
    p.mode          = (delay2290::Mode) choice ("mode");
    p.feedback      = value ("feedback") * 0.01;
    p.mix           = value ("mix") * 0.01;
    p.hiCutHz       = value ("hicut");
    p.loCutHz       = value ("locut");
    p.spread        = value ("spread") * 0.01;
    p.modDepth      = value ("moddepth") * 0.01;
    p.modSpeedHz    = value ("modspeed");
    p.duckThreshDb  = value ("duckthresh");
    p.duckAmountDb  = value ("duckamount");
    p.duckReleaseMs = value ("duckrelease");
    p.outputDb      = value ("output");
    p.bypassed      = flag ("bypass");
    return p;
}

void Delay2290Processor::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double gap = now - lastTapMs;
    lastTapMs        = now;

    if (gap < 20.0 || gap > delay2290::kMaxTimeMs)
    {
        tapIntervals.clear(); // first tap (or a long pause): start counting again
        return;
    }
    tapIntervals.push_back (gap);
    if (tapIntervals.size() > 4)
        tapIntervals.erase (tapIntervals.begin());

    double avg = 0.0;
    for (double t : tapIntervals) avg += t;
    avg /= (double) tapIntervals.size();

    auto set = [this] (const char* id, float realValue) {
        if (auto* param = apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (realValue));
            param->endChangeGesture();
        }
    };
    set ("sync", 0.0f);
    set ("time", (float) avg);
}

void Delay2290Processor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
}

void Delay2290Processor::processDsp (float* const* data, int numChannels, int numSamples)
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
    duck = engine.currentDuckDb();
}

juce::AudioProcessorEditor* Delay2290Processor::createEditor() { return new Delay2290Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Delay2290Processor(); }
