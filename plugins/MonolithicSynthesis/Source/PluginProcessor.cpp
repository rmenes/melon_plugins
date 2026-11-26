#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace melon::mono;

MonolithicSynthesisProcessor::MonolithicSynthesisProcessor() : ProcessorBase (createParameterLayout(), 3.0)
{
    presets.setFactoryPresets (monolithic::factoryPresets());
}

melon::params::Layout MonolithicSynthesisProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    juce::StringArray modes;
    for (const auto& m : getModeInfos())
        modes.add (m.name);

    juce::StringArray sounds;
    for (int i = 1; i <= kVariationsPerMode; ++i)
        sounds.add (juce::String (i));

    addChoice (l, "mode", "Mode", modes, Lead);
    addChoice (l, "sound", "Sound", sounds, 0);
    addPercent (l, "param1", "Param 1", 50.0f);
    addPercent (l, "param2", "Param 2", 50.0f);
    addPercent (l, "param3", "Param 3", 50.0f);
    addDb (l, "effect", "Effect Level", -40.0f, 6.0f, 0.0f);

    addPercent (l, "mix", "Mix", 100.0f);

    addPercent (l, "sensitivity", "Sensitivity", 60.0f);
    addChoice (l, "octave", "Octave", { "-2", "-1", "0", "+1", "+2" }, 2);
    addBool (l, "snap", "Pitch Snap", false);
    addBool (l, "midi", "MIDI In", false);
    addOutputAndBypass (l);
    return l;
}

EngineParams MonolithicSynthesisProcessor::currentParams() const
{
    EngineParams p;
    p.mode          = juce::jlimit (0, NumModes - 1, choice ("mode"));
    p.variation     = juce::jlimit (0, kVariationsPerMode - 1, choice ("sound"));
    p.param1        = value ("param1") * 0.01f;
    p.param2        = value ("param2") * 0.01f;
    p.param3        = value ("param3") * 0.01f;
    p.effectLevelDb = value ("effect");
    p.mix           = value ("mix") * 0.01f;
    p.sensitivity   = value ("sensitivity") * 0.01f;
    p.octaveShift   = choice ("octave") - 2;
    p.pitchSnap     = flag ("snap");
    p.midiInput     = flag ("midi");
    return p;
}

void MonolithicSynthesisProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Notes are applied at the start of the block (the engine is monophonic, last-note priority)
    engine.setParams (currentParams());
    if (flag ("midi"))
    {
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
                engine.midiNoteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff())
                engine.midiNoteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                for (int n = 0; n < 128; ++n)
                    engine.midiNoteOff (n);
        }
    }
    ProcessorBase::processBlock (buffer, midi);
}

void MonolithicSynthesisProcessor::prepareDsp (double sampleRate, int maxBlock, int)
{
    engine.prepare (sampleRate);
    engine.setParams (currentParams());

    input.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
    outL.assign (input.size(), 0.0f);
    outR.assign (input.size(), 0.0f);

    active.prepare (sampleRate, 20.0);
    outputGain.prepare (sampleRate, 20.0);
    active.setTarget (flag ("bypass") ? 0.0 : 1.0);
    outputGain.setTarget (melon::dbToGain (value ("output")));
    active.snap();
    outputGain.snap();
}

void MonolithicSynthesisProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            double tempo = 120.0;
            if (auto b = pos->getBpm())
            {
                tempo = *b;
                bpm   = *b;
            }
            double ppq = 0.0;
            if (auto q = pos->getPpqPosition())
                ppq = *q;
            engine.setTransport (tempo, ppq, pos->getIsPlaying() && pos->getPpqPosition().hasValue());
        }

    engine.setParams (currentParams());
    active.setTarget (flag ("bypass") ? 0.0 : 1.0);
    outputGain.setTarget (melon::dbToGain (value ("output")));

    for (int start = 0; start < numSamples; start += (int) input.size())
    {
        const int n = juce::jmin ((int) input.size(), numSamples - start);
        float* l = data[0] + start;
        float* r = data[juce::jmin (1, numChannels - 1)] + start;

        // Guitar input: mono sum, or the one channel that has signal (guitar on L only, etc.)
        if (numChannels < 2)
        {
            std::copy (l, l + n, input.begin());
        }
        else
        {
            double el = 0.0, er = 0.0;
            for (int i = 0; i < n; ++i) { el += (double) l[i] * l[i]; er += (double) r[i] * r[i]; }
            for (int i = 0; i < n; ++i)
                input[(size_t) i] = er < 1.0e-4 * el ? l[i] : el < 1.0e-4 * er ? r[i] : 0.5f * (l[i] + r[i]);
        }

        engine.process (input.data(), outL.data(), outR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            const float on = (float) active.next();
            const float g  = (float) outputGain.next();
            const float dryL = l[i], dryR = r[i];
            const float wetL = outL[(size_t) i] * g, wetR = outR[(size_t) i] * g;
            l[i] = dryL + on * (wetL - dryL);   // bypass crossfades to the dry signal
            if (numChannels > 1)
                r[i] = dryR + on * (wetR - dryR);
        }
    }

    meterValues.pitch  = engine.getTrackedPitch();
    meterValues.level  = engine.getInputLevel();
    meterValues.active = engine.isNoteActive();
    meterValues.step   = engine.getPatternStep();
}

juce::AudioProcessorEditor* MonolithicSynthesisProcessor::createEditor() { return new MonolithicSynthesisEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MonolithicSynthesisProcessor(); }
