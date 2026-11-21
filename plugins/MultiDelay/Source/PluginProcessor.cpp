#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

MultiDelayProcessor::MultiDelayProcessor() : ProcessorBase (createParameterLayout(), 12.0)
{
    presets.setFactoryPresets (mdelay::factoryPresets());
}

melon::params::Layout MultiDelayProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray names;
    for (const auto& m : mdelay::machines())
        names.add (m.name);

    juce::StringArray sync;
    for (const auto& s : mdelay::syncValues())
        sync.add (s.first);

    addChoice (l, "machine", "Machine", names, mdelay::digital);
    addFloat (l, "time", "Time", 1.0f, (float) mdelay::kMaxTimeMs, 400.0f, "ms", 0, 400.0f);
    addChoice (l, "sync", "Sync", sync, 0);
    addPercent (l, "repeats", "Repeats", 35.0f);
    addPercent (l, "mix", "Mix", 35.0f);
    addPercent (l, "filter", "Filter", 80.0f);
    addPercent (l, "grit", "Grit", 0.0f);
    addPercent (l, "speed", "Speed", 30.0f);
    addPercent (l, "depth", "Depth", 10.0f);
    addPercent (l, "param1", "Param 1", 50.0f);
    addPercent (l, "param2", "Param 2", 50.0f);
    addBool (l, "infinite", "Infinite", false);
    addBool (l, "trails", "Trails", false);
    addOutputAndBypass (l);
    return l;
}

mdelay::Params MultiDelayProcessor::currentParams() const
{
    mdelay::Params p;
    const double synced = mdelay::syncedTimeMs (choice ("sync"), bpm.load());
    p.machine  = choice ("machine");
    p.timeMs   = synced > 0.0 ? synced : value ("time");
    p.repeats  = value ("repeats") * 0.01;
    p.mix      = value ("mix") * 0.01;
    p.filter   = value ("filter") * 0.01;
    p.grit     = value ("grit") * 0.01;
    p.speed    = value ("speed") * 0.01;
    p.depth    = value ("depth") * 0.01;
    p.param1   = value ("param1") * 0.01;
    p.param2   = value ("param2") * 0.01;
    p.infinite = flag ("infinite");
    p.trails   = flag ("trails");
    p.outputDb = value ("output");
    p.bypassed = flag ("bypass");
    return p;
}

void MultiDelayProcessor::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double gap = now - lastTapMs;
    lastTapMs        = now;

    if (gap < 20.0 || gap > mdelay::kMaxTimeMs)
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

void MultiDelayProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
}

void MultiDelayProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* MultiDelayProcessor::createEditor() { return new MultiDelayEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MultiDelayProcessor(); }
