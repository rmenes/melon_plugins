#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

MultiModProcessor::MultiModProcessor() : ProcessorBase (createParameterLayout(), 0.5)
{
    presets.setFactoryPresets (mmod::factoryPresets());
}

melon::params::Layout MultiModProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray names;
    for (const auto& m : mmod::machines())
        names.add (m.name);

    juce::StringArray sync;
    for (const auto& s : mmod::syncValues())
        sync.add (s.first);

    juce::StringArray waves;
    for (int w = 0; w < mmod::kNumWaves; ++w)
        waves.add (mmod::waveName (w));

    addChoice (l, "machine", "Machine", names, mmod::chorus);
    addFloat (l, "speed", "Speed", (float) mmod::kMinSpeedHz, (float) mmod::kMaxSpeedHz, 1.0f, "Hz", 2, 1.0f);
    addChoice (l, "sync", "Sync", sync, 0);
    addChoice (l, "wave", "Wave", waves, mmod::sine);
    addPercent (l, "depth", "Depth", 50.0f);
    addPercent (l, "mix", "Mix", 50.0f);
    addPercent (l, "width", "Width", 50.0f);
    addPercent (l, "param1", "Param 1", 50.0f);
    addPercent (l, "param2", "Param 2", 50.0f);
    addOutputAndBypass (l);
    return l;
}

mmod::Params MultiModProcessor::currentParams() const
{
    mmod::Params p;
    const double synced = mmod::syncedHz (choice ("sync"), bpm.load());
    p.machine  = juce::jlimit (0, mmod::kNumMachines - 1, choice ("machine"));
    p.speedHz  = synced > 0.0 ? synced : value ("speed");
    p.wave     = choice ("wave");
    p.depth    = value ("depth") * 0.01;
    p.mix      = value ("mix") * 0.01;
    p.width    = value ("width") * 0.01;
    p.param1   = value ("param1") * 0.01;
    p.param2   = value ("param2") * 0.01;
    p.outputDb = value ("output");
    p.bypassed = flag ("bypass");
    return p;
}

void MultiModProcessor::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double gap = now - lastTapMs;
    lastTapMs        = now;

    if (gap < 50.0 || gap > 1000.0 / mmod::kMinSpeedHz)
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
    set ("speed", (float) juce::jlimit (mmod::kMinSpeedHz, mmod::kMaxSpeedHz, 1000.0 / avg));
}

void MultiModProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
}

void MultiModProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    const int    syncIndex = choice ("sync");
    const double lengthQ   = mmod::syncLengthQuarters (syncIndex);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = *b;
            // Sync on and the host playing: lock the LFO (Pattern Trem: the steps) to the beat
            if (lengthQ > 0.0 && pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    engine.setHostPosition (*ppq / lengthQ);
        }

    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
    position = engine.getPosition();
}

juce::AudioProcessorEditor* MultiModProcessor::createEditor() { return new MultiModEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MultiModProcessor(); }
