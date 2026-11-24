#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

TremProcessor::TremProcessor() : ProcessorBase (createParameterLayout(), 0.0)
{
    presets.setFactoryPresets (trem::factoryPresets());
}

melon::params::Layout TremProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray sync;
    for (const auto& s : trem::syncValues())
        sync.add (s.first);

    addFloat (l, "rate", "Rate", (float) trem::kMinRateHz, (float) trem::kMaxRateHz, 4.0f, "Hz", 2, 3.0f);
    addPercent (l, "wave", "Wave", 50.0f);
    addPercent (l, "depth", "Depth", 50.0f);
    addChoice (l, "sync", "Sync", sync, trem::syncOff);
    addChoice (l, "stereo", "Stereo", { "Mono", "Pan" }, trem::mono);
    addOutputAndBypass (l);
    return l;
}

trem::Params TremProcessor::currentParams() const
{
    trem::Params p;
    const double synced = trem::syncedHz (choice ("sync"), bpm.load());
    p.rateHz   = synced > 0.0 ? synced : (double) value ("rate");
    p.wave     = value ("wave") * 0.01;
    p.depth    = value ("depth") * 0.01;
    p.stereo   = juce::jlimit (0, trem::kNumStereoModes - 1, choice ("stereo"));
    p.outputDb = value ("output");
    p.bypassed = flag ("bypass");
    return p;
}

void TremProcessor::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double gap = now - lastTapMs;
    lastTapMs        = now;

    if (gap < 50.0 || gap > 1000.0 / trem::kMinRateHz)
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
    set ("rate", (float) juce::jlimit (trem::kMinRateHz, trem::kMaxRateHz, 1000.0 / avg));
}

void TremProcessor::prepareDsp (double sampleRate, int, int numChannels)
{
    engine.prepare (sampleRate, numChannels);
    engine.setParams (currentParams());
    engine.snapToTargets();
}

void TremProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    const double lengthQ = trem::syncLengthQuarters (choice ("sync"));

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = *b;
            // Sync on and the host playing: lock the LFO to the beat
            if (lengthQ > 0.0 && pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    engine.setHostPosition (*ppq / lengthQ);
        }

    engine.setParams (currentParams());
    engine.process (data, numChannels, numSamples);
}

juce::AudioProcessorEditor* TremProcessor::createEditor() { return new TremEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TremProcessor(); }
