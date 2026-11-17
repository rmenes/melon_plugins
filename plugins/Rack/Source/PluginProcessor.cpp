#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

const juce::String RackProcessor::modelWildcard = "*.nam;*.json;*.aidax";
const juce::String RackProcessor::irWildcard    = "*.wav;*.aif;*.aiff;*.flac";

namespace
{
const char* const kModelIds[RackProcessor::kNumModels][4] = {
    { "a_on", "a_in", "a_out", "a_norm" },
    { "b_on", "b_in", "b_out", "b_norm" },
};
const char* const kEqIds[rack::Eq6::kBands] = { "eq_1", "eq_2", "eq_3", "eq_4", "eq_5", "eq_6" };
const char* const kModelProps[RackProcessor::kNumModels] = { "modelPedal", "modelAmp" };
const char* const kIrProps[RackProcessor::kNumIrs]       = { "ir1", "ir2" };

juce::String rateText (double hz)
{
    const double k = hz / 1000.0;
    return (std::abs (k - std::round (k)) < 0.01 ? juce::String (juce::roundToInt (k)) : juce::String (k, 1)) + " kHz";
}
} // namespace

//==============================================================================
RackProcessor::RackProcessor() : ProcessorBase (createParameterLayout(), 0.5)
{
    lastModelFolder = lastIrFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
}

RackProcessor::~RackProcessor()
{
    cancelPendingUpdate();
    loader.removeAllJobs (true, 10000);
}

melon::params::Layout RackProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    addChoice (l, "in_ch", "Input Channel", { "Left", "Right", "Left + Right" }, 0);
    addDb (l, "in_gain", "Input", -24.0f, 24.0f, 0.0f);
    addBool (l, "gate_on", "Gate", false);
    addFloat (l, "gate_thr", "Gate Threshold", -90.0f, -10.0f, -60.0f, "dB", 1);

    const char* names[kNumModels] = { "Pedal", "Amp" };
    for (int s = 0; s < kNumModels; ++s)
    {
        const juce::String n (names[s]);
        addBool (l, kModelIds[s][0], n, true);
        addDb (l, kModelIds[s][1], n + " Input", -24.0f, 24.0f, 0.0f);
        addDb (l, kModelIds[s][2], n + " Output", -24.0f, 24.0f, 0.0f);
        addBool (l, kModelIds[s][3], n + " Normalize", true);

        if (s == 0) // the EQ parameters sit between the two slots in the host's list
        {
            addBool (l, "eq_on", "EQ", false);
            addChoice (l, "eq_pos", "EQ Position", { "Before Pedal", "Between Pedal and Amp", "After Amp" }, 1);
            for (int b = 0; b < rack::Eq6::kBands; ++b)
            {
                const double f = rack::Eq6::kFreqs[(size_t) b];
                const auto label = f >= 1000.0 ? juce::String (f / 1000.0, 1).trimCharactersAtEnd (".0") + " kHz"
                                                : juce::String (juce::roundToInt (f)) + " Hz";
                addDb (l, kEqIds[b], "EQ " + label, -12.0f, 12.0f, 0.0f);
            }
        }
    }

    addBool (l, "cab_on", "Cabinet", true);
    addChoice (l, "cab_mode", "Cabinet Mode", { "Stereo L/R", "Blend" }, 0);
    addPercent (l, "cab_blend", "Cabinet Blend", 50.0f);
    addDb (l, "cab_level", "Cabinet Level", -24.0f, 12.0f, 0.0f);

    addOutputAndBypass (l);
    return l;
}

juce::AudioProcessorEditor* RackProcessor::createEditor() { return new RackEditor (*this); }

//==============================================================================
void RackProcessor::prepareDsp (double sampleRate, int maxBlock, int)
{
    maxBlockSize = juce::jmax (16, maxBlock);
    currentRate  = sampleRate;
    currentBlock = maxBlockSize;

    for (auto* v : { &mono, &cabA, &cabB, &dryL, &dryR })
        v->assign ((size_t) maxBlockSize, 0.0f);

    gate.prepare (sampleRate);
    eq.prepare (sampleRate);
    for (auto& d : dryDelay)
    {
        d.prepare (4096);
        d.reset();
    }

    for (auto* s : { &inputGain, &cabLevel, &cabBlend, &outputGain, &wetMix })
        s->prepare (sampleRate, 15.0);
    inputGain.setTarget (melon::dbToGain (value ("in_gain")));
    cabLevel.setTarget (melon::dbToGain (value ("cab_level")));
    cabBlend.setTarget (value ("cab_blend") * 0.01);
    outputGain.setTarget (melon::dbToGain (value ("output")));
    wetMix.setTarget (flag ("bypass") ? 0.0 : 1.0);
    for (auto* s : { &inputGain, &cabLevel, &cabBlend, &outputGain, &wetMix })
        s->snap();

    for (auto& slot : slots)
    {
        slot.inGain.prepare (sampleRate, 15.0);
        slot.outGain.prepare (sampleRate, 15.0);
        slot.offDelay.prepare (4096);
        const juce::SpinLock::ScopedLockType sl (slot.lock);
        if (slot.model != nullptr)
            slot.model->prepare (sampleRate, maxBlockSize);
    }

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    for (auto& c : convolution)
    {
        c.prepare (spec);
        c.reset();
    }

    updateLatency();
}

void RackProcessor::updateLatency()
{
    int total = 0;
    for (int s = 0; s < kNumModels; ++s)
    {
        auto& slot = slots[s];
        const juce::SpinLock::ScopedLockType sl (slot.lock);
        const int lat = slot.model != nullptr ? slot.model->latencySamples() : 0;
        slot.offDelay.setDelay (lat);
        modelResampling[s] = slot.model != nullptr && slot.model->isResampling();
        total += lat;
    }
    for (auto& d : dryDelay)
        d.setDelay (total);
    if (total != getLatencySamples())
        setLatencySamples (total);
}

//==============================================================================
void RackProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const int n = juce::jmin (maxBlockSize, numSamples - done);
        processChunk (data, numChannels, done, n);
        done += n;
    }
}

void RackProcessor::runModel (int s, bool on, float inDb, float outDb, bool normalize, float* x, int n)
{
    auto& slot = slots[s];
    const juce::SpinLock::ScopedTryLockType tl (slot.lock);
    if (! tl.isLocked() || slot.model == nullptr)
        return; // nothing loaded (or a new model is being swapped in right now): pass through

    if (! on)
    {
        slot.offDelay.process (x, n);
        return;
    }

    slot.inGain.setTarget (melon::dbToGain (inDb));
    slot.outGain.setTarget (melon::dbToGain (outDb + (normalize ? slot.model->normalizeDb() : 0.0f)));
    for (int i = 0; i < n; ++i)
        x[i] *= (float) slot.inGain.next();
    slot.model->process (x, n);
    for (int i = 0; i < n; ++i)
        x[i] *= (float) slot.outGain.next();
}

void RackProcessor::processChunk (float* const* data, int numChannels, int offset, int n)
{
    float* L = data[0] + offset;
    float* R = numChannels > 1 ? data[1] + offset : nullptr;

    // dry copy for the bypass crossfade, delayed by the plugin's latency
    std::copy (L, L + n, dryL.data());
    std::copy (R != nullptr ? R : L, (R != nullptr ? R : L) + n, dryR.data());
    dryDelay[0].process (dryL.data(), n);
    dryDelay[1].process (dryR.data(), n);

    // --- input -----------------------------------------------------------------
    const int inCh = choice ("in_ch");
    inputGain.setTarget (melon::dbToGain (value ("in_gain")));
    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R != nullptr ? R[i] : L[i];
        const float x = inCh == 0 ? l : inCh == 1 ? r : 0.5f * (l + r);
        mono[(size_t) i] = x * (float) inputGain.next();
    }
    float* x = mono.data();

    if (flag ("gate_on"))
    {
        gate.setThresholdDb (value ("gate_thr"));
        gate.process (x, n);
        gateOpen = gate.isOpen();
    }
    else
    {
        gateOpen = true;
    }

    // --- pedal, EQ, amp ----------------------------------------------------------
    const bool eqOn  = flag ("eq_on");
    const int  eqPos = choice ("eq_pos");
    if (eqOn)
        for (int b = 0; b < rack::Eq6::kBands; ++b)
            eq.setBand (b, value (kEqIds[b]));

    for (int stage = 0; stage <= kNumModels; ++stage)
    {
        if (eqOn && eqPos == stage)
            eq.process (x, n);
        if (stage < kNumModels)
            runModel (stage, flag (kModelIds[stage][0]), value (kModelIds[stage][1]), value (kModelIds[stage][2]),
                      flag (kModelIds[stage][3]), x, n);
    }

    // --- cabinet: mono -> stereo ------------------------------------------------------
    const bool ir1 = irActive[0].load(), ir2 = irActive[1].load();
    cabLevel.setTarget (melon::dbToGain (value ("cab_level")));
    cabBlend.setTarget (value ("cab_blend") * 0.01);
    float* outL = cabA.data();
    float* outR = cabB.data();

    if (flag ("cab_on") && (ir1 || ir2))
    {
        float* in[1]  = { x };
        float* oa[1]  = { cabA.data() };
        float* ob[1]  = { cabB.data() };
        juce::dsp::AudioBlock<float> inBlock (in, 1, (size_t) n), aBlock (oa, 1, (size_t) n), bBlock (ob, 1, (size_t) n);

        if (ir1) convolution[0].process (juce::dsp::ProcessContextNonReplacing<float> (inBlock, aBlock));
        if (ir2) convolution[1].process (juce::dsp::ProcessContextNonReplacing<float> (inBlock, bBlock));
        if (! ir1) std::copy (cabB.begin(), cabB.begin() + n, cabA.begin()); // only one IR: use it on both sides
        if (! ir2) std::copy (cabA.begin(), cabA.begin() + n, cabB.begin());

        const bool blend = choice ("cab_mode") == 1;
        for (int i = 0; i < n; ++i)
        {
            const float lvl = (float) cabLevel.next();
            const float b   = (float) cabBlend.next();
            if (blend)
            {
                const float m = (1.0f - b) * cabA[(size_t) i] + b * cabB[(size_t) i];
                cabA[(size_t) i] = cabB[(size_t) i] = m * lvl;
            }
            else
            {
                cabA[(size_t) i] *= lvl;
                cabB[(size_t) i] *= lvl;
            }
        }
    }
    else
    {
        std::copy (x, x + n, cabA.begin());
        std::copy (x, x + n, cabB.begin());
        for (int i = 0; i < n; ++i)
        {
            cabLevel.next();
            cabBlend.next();
        }
    }

    // --- output and bypass ----------------------------------------------------------
    outputGain.setTarget (melon::dbToGain (value ("output")));
    wetMix.setTarget (flag ("bypass") ? 0.0 : 1.0);
    for (int i = 0; i < n; ++i)
    {
        const float g = (float) outputGain.next();
        const float w = (float) wetMix.next();
        const float l = outL[i] * g, r = outR[i] * g;
        if (R != nullptr)
        {
            L[i] = w * l + (1.0f - w) * dryL[(size_t) i];
            R[i] = w * r + (1.0f - w) * dryR[(size_t) i];
        }
        else
        {
            L[i] = w * 0.5f * (l + r) + (1.0f - w) * dryL[(size_t) i];
        }
    }
}

//==============================================================================
double RackProcessor::readModelSampleRate (const juce::File& file)
{
    const auto json = juce::JSON::parse (file);
    auto find = [] (const juce::var& v) -> double {
        for (auto key : { "sample_rate", "samplerate", "sampleRate" })
            if (v.hasProperty (key) && (double) v[key] > 1000.0)
                return (double) v[key];
        return 0.0;
    };
    if (const double r = find (json); r > 0.0)
        return r;
    if (const double r = find (json["metadata"]); r > 0.0)
        return r;
    return 48000.0; // NAM's standard rate, and what AIDA-X models without metadata expect
}

void RackProcessor::loadModel (int s, const juce::File& file)
{
    auto& slot = slots[s];
    int gen;
    {
        const juce::ScopedLock sl (statusLock);
        gen = ++slot.generation;
        slot.status = { file, "Loading...", true, false };
    }
    lastModelFolder = file.getParentDirectory();

    loader.addJob ([this, s, file, gen] {
        auto& sl = slots[s];
        juce::String info;
        bool failed = false;

        std::unique_ptr<rack::LoadedModel> model;
        double preparedRate  = 0.0;
        int    preparedBlock = 0;
        if (! file.existsAsFile())
        {
            info   = "File not found";
            failed = true;
        }
        else
        {
            const double rate = readModelSampleRate (file);
            std::string  error;
            model = rack::LoadedModel::load (file.getFullPathName().toStdString(), rate, error);
            if (model == nullptr)
            {
                info   = "Could not load: " + juce::String (error);
                failed = true;
            }
            else
            {
                preparedRate  = currentRate.load();
                preparedBlock = currentBlock.load();
                model->prepare (preparedRate, preparedBlock);
                info = file.getFileExtension().substring (1).toUpperCase() + " model, trained at " + rateText (rate);
            }
        }

        {
            const juce::ScopedLock l (statusLock);
            if (gen != sl.generation)
                return; // a newer request replaced this one
            sl.status = { file, info, false, failed };
        }

        if (model != nullptr)
        {
            {
                const juce::SpinLock::ScopedLockType lock (sl.lock);
                // the host may have changed the sample rate while the model was loading
                if (currentRate.load() != preparedRate || currentBlock.load() != preparedBlock)
                    model->prepare (currentRate.load(), currentBlock.load());
                std::swap (sl.model, model);
            }
            model.reset(); // the previous model, freed outside the lock and off the audio thread
            triggerAsyncUpdate();
        }
    });
}

void RackProcessor::clearModel (int s)
{
    auto& slot = slots[s];
    {
        const juce::ScopedLock sl (statusLock);
        ++slot.generation;
        slot.status = {};
    }
    std::unique_ptr<rack::LoadedModel> old;
    {
        const juce::SpinLock::ScopedLockType lock (slot.lock);
        std::swap (slot.model, old);
    }
    old.reset();
    updateLatency();
}

void RackProcessor::loadIr (int s, const juce::File& file)
{
    lastIrFolder = file.getParentDirectory();
    SlotStatus st { file, {}, false, false };

    if (! file.existsAsFile())
    {
        st.info  = "File not found";
        st.error = true;
        irActive[s] = false;
    }
    else
    {
        // Convolution loads, resamples to the host rate and swaps the IR in on its own background thread
        convolution[s].loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes,
                                            0, juce::dsp::Convolution::Normalise::yes);
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (file) })
            st.info = juce::String (r->lengthInSamples * 1000.0 / r->sampleRate, 0) + " ms, " + rateText (r->sampleRate);
        irActive[s] = true;
    }

    const juce::ScopedLock sl (statusLock);
    irState[s] = st;
}

void RackProcessor::clearIr (int s)
{
    irActive[s] = false;
    const juce::ScopedLock sl (statusLock);
    irState[s] = {};
}

RackProcessor::SlotStatus RackProcessor::modelStatus (int s) const
{
    const juce::ScopedLock sl (statusLock);
    return slots[s].status;
}

RackProcessor::SlotStatus RackProcessor::irStatus (int s) const
{
    const juce::ScopedLock sl (statusLock);
    return irState[s];
}

//==============================================================================
// The file paths are stored with the parameters, so projects and presets reload them.
void RackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int s = 0; s < kNumModels; ++s)
        state.setProperty (kModelProps[s], modelStatus (s).file.getFullPathName(), nullptr);
    for (int s = 0; s < kNumIrs; ++s)
        state.setProperty (kIrProps[s], irStatus (s).file.getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ProcessorBase::setStateInformation (data, sizeInBytes);
    const auto state = apvts.copyState();

    for (int s = 0; s < kNumModels; ++s)
    {
        const auto path = state.getProperty (kModelProps[s]).toString();
        if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
            loadModel (s, juce::File (path));
        else
            clearModel (s);
    }
    for (int s = 0; s < kNumIrs; ++s)
    {
        const auto path = state.getProperty (kIrProps[s]).toString();
        if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
            loadIr (s, juce::File (path));
        else
            clearIr (s);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RackProcessor(); }
