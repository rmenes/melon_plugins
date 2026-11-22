#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

// .nam is the format this plugin is made for; the engine also reads AIDA-X (.aidax) and RTNeural (.json) models
const juce::String NamProcessor::modelWildcard = "*.nam;*.aidax;*.json";

namespace
{
constexpr const char* kModelProp = "model"; // the model file path, saved with projects and user presets

juce::String rateText (double hz)
{
    const double k = hz / 1000.0;
    return (std::abs (k - std::round (k)) < 0.01 ? juce::String (juce::roundToInt (k)) : juce::String (k, 1)) + " kHz";
}
} // namespace

//==============================================================================
NamProcessor::NamProcessor() : ProcessorBase (createParameterLayout(), 0.1)
{
    lastModelFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    // Factory presets only set the controls and keep the loaded model. User presets also remember the model.
    presets.setFactoryPresets (mnam::factoryPresets());
    presets.writeExtras = [this] (juce::XmlElement& xml) {
        xml.setAttribute (kModelProp, modelStatus().file.getFullPathName());
    };
    presets.readExtras = [this] (const juce::XmlElement& xml) {
        if (! xml.hasAttribute (kModelProp))
            return; // preset saved without a model: keep the current one
        const auto path = xml.getStringAttribute (kModelProp);
        if (path.isEmpty())
            clearModel();
        else if (juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
            loadModel (juce::File (path));
    };

    // The pedal (no plugin window) picks a model from the library folder by number
    library.onSelect = [this] (const juce::File& f) { startModelLoad (f); };
    library.onNone   = [this] (const juce::String& message) { unloadModel (message, false); };
    library.start();
}

NamProcessor::~NamProcessor()
{
    library.stop();
    cancelPendingUpdate();
    loader.removeAllJobs (true, 10000);
}

melon::params::Layout NamProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    addDb (l, "input", "Input", -20.0f, 20.0f, 0.0f);
    addBool (l, "gate_on", "Noise Gate", true);
    addFloat (l, "gate_thr", "Gate Threshold", -100.0f, 0.0f, -80.0f, "dB", 1);
    addBool (l, "eq_on", "EQ", true);
    addFloat (l, "bass", "Bass", 0.0f, 10.0f, 5.0f, "", 1);
    addFloat (l, "middle", "Middle", 0.0f, 10.0f, 5.0f, "", 1);
    addFloat (l, "treble", "Treble", 0.0f, 10.0f, 5.0f, "", 1);
    addBool (l, "normalize", "Normalize", true);
    melon::LibrarySelector::addParameter (l, "model_index", "Library Model"); // -1 = none; see MelonLibrary.h

    addOutputAndBypass (l);
    return l;
}

juce::AudioProcessorEditor* NamProcessor::createEditor() { return new NamEditor (*this); }

//==============================================================================
void NamProcessor::prepareDsp (double sampleRate, int maxBlock, int)
{
    maxBlockSize = juce::jmax (16, maxBlock);
    currentRate  = sampleRate;
    currentBlock = maxBlockSize;

    for (auto* v : { &mono, &dryL, &dryR })
        v->assign ((size_t) maxBlockSize, 0.0f);

    gate.prepare (sampleRate);
    toneStack.prepare (sampleRate);
    for (auto& d : dryDelay)
    {
        d.prepare (8192);
        d.reset();
    }

    for (auto* s : { &inputGain, &modelGain, &outputGain, &wetMix })
        s->prepare (sampleRate, 15.0);
    inputGain.setTarget (melon::dbToGain (value ("input")));
    outputGain.setTarget (melon::dbToGain (value ("output")));
    wetMix.setTarget (flag ("bypass") ? 0.0 : 1.0);
    {
        const juce::SpinLock::ScopedLockType sl (modelLock);
        modelGain.setTarget (model != nullptr && flag ("normalize") ? melon::dbToGain (model->normalizeDb()) : 1.0);
        if (model != nullptr)
            model->prepare (sampleRate, maxBlockSize);
    }
    for (auto* s : { &inputGain, &modelGain, &outputGain, &wetMix })
        s->snap();

    updateLatency();
}

void NamProcessor::updateLatency()
{
    int lat = 0;
    {
        const juce::SpinLock::ScopedLockType sl (modelLock);
        lat             = model != nullptr ? model->latencySamples() : 0;
        modelResampling = model != nullptr && model->isResampling();
    }
    for (auto& d : dryDelay)
        d.setDelay (lat);
    if (lat != getLatencySamples())
        setLatencySamples (lat);
}

//==============================================================================
void NamProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const int n = juce::jmin (maxBlockSize, numSamples - done);
        processChunk (data, numChannels, done, n);
        done += n;
    }
}

void NamProcessor::processChunk (float* const* data, int numChannels, int offset, int n)
{
    float* L = data[0] + offset;
    float* R = numChannels > 1 ? data[1] + offset : nullptr;

    // dry copy for the bypass crossfade, delayed by the model's latency
    std::copy (L, L + n, dryL.data());
    std::copy (R != nullptr ? R : L, (R != nullptr ? R : L) + n, dryR.data());
    dryDelay[0].process (dryL.data(), n);
    dryDelay[1].process (dryR.data(), n);

    // --- input (left channel: the guitar) ----------------------------------------------
    inputGain.setTarget (melon::dbToGain (value ("input")));
    float* x = mono.data();
    for (int i = 0; i < n; ++i)
        x[i] = L[i] * (float) inputGain.next();

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

    // --- model ----------------------------------------------------------------------------
    {
        const juce::SpinLock::ScopedTryLockType tl (modelLock);
        if (tl.isLocked() && model != nullptr)
        {
            modelGain.setTarget (flag ("normalize") ? melon::dbToGain (model->normalizeDb()) : 1.0);
            model->process (x, n);
            for (int i = 0; i < n; ++i)
                x[i] *= (float) modelGain.next();
        }
        // nothing loaded (or a new model is being swapped in right now): pass through
    }

    // --- tone stack -----------------------------------------------------------------------
    if (flag ("eq_on"))
    {
        toneStack.set (value ("bass"), value ("middle"), value ("treble"));
        toneStack.process (x, n);
    }

    // --- output and bypass ------------------------------------------------------------------
    outputGain.setTarget (melon::dbToGain (value ("output")));
    wetMix.setTarget (flag ("bypass") ? 0.0 : 1.0);
    for (int i = 0; i < n; ++i)
    {
        const float w = (float) wetMix.next();
        const float y = x[i] * (float) outputGain.next() * w;
        L[i] = y + (1.0f - w) * dryL[(size_t) i];
        if (R != nullptr)
            R[i] = y + (1.0f - w) * dryR[(size_t) i];
    }
}

//==============================================================================
void NamProcessor::loadModel (const juce::File& file)
{
    lastModelFolder = file.getParentDirectory();
    startModelLoad (file);
}

void NamProcessor::startModelLoad (const juce::File& file)
{
    int gen;
    {
        const juce::ScopedLock sl (statusLock);
        gen    = ++generation;
        status = { file, "Loading...", true, false };
    }

    loader.addJob ([this, file, gen] {
        juce::String info;
        bool failed = false;
        std::unique_ptr<mnam::LoadedModel> m;
        double preparedRate  = 0.0;
        int    preparedBlock = 0;

        if (! file.existsAsFile())
        {
            info   = "File not found: " + file.getFullPathName();
            failed = true;
        }
        else
        {
            std::string error;
            m = mnam::LoadedModel::load (file.getFullPathName().toStdString(), 48000.0, error);
            if (m == nullptr)
            {
                info   = "Could not load: " + juce::String (error);
                failed = true;
            }
            else
            {
                preparedRate  = currentRate.load();
                preparedBlock = currentBlock.load();
                m->prepare (preparedRate, preparedBlock);
                info = file.getFileExtension().substring (1).toUpperCase() + " model, trained at " + rateText (m->modelSampleRate());
                if (std::abs (m->modelSampleRate() - preparedRate) > 1.0)
                    info << " (resampled, the session runs at " << rateText (preparedRate) << ")";
                if (m->normalizeDb() != 0.0f)
                    info << "  |  loudness data: " << (m->normalizeDb() > 0.0f ? "+" : "") << juce::String (m->normalizeDb(), 1) << " dB";
            }
        }

        // the host may have changed the sample rate or block size while the model was loading
        if (m != nullptr && (currentRate.load() != preparedRate || currentBlock.load() != preparedBlock))
            m->prepare (currentRate.load(), currentBlock.load());

        {
            const juce::ScopedLock l (statusLock);
            if (gen != generation)
                return; // a newer request (another file, or Unload) replaced this one
            status = { file, info, false, failed };
            if (m != nullptr)
            {
                const juce::SpinLock::ScopedLockType lock (modelLock);
                std::swap (model, m);
            }
        }

        if (! failed)
        {
            m.reset(); // the previous model, freed outside the locks and off the audio thread
            triggerAsyncUpdate();
        }
    });
}

void NamProcessor::clearModel() { unloadModel ({}, true); }

// message: empty, or why nothing is loaded (shown as an error, e.g. "No file #12 in ...")
void NamProcessor::unloadModel (const juce::String& message, bool updateLatencyNow)
{
    std::unique_ptr<mnam::LoadedModel> old;
    {
        const juce::ScopedLock sl (statusLock);
        ++generation;
        status = {};
        if (message.isNotEmpty())
            status = { juce::File(), message, false, true };
        const juce::SpinLock::ScopedLockType lock (modelLock);
        std::swap (model, old);
    }
    old.reset();
    if (updateLatencyNow)
        updateLatency();
    else
        triggerAsyncUpdate(); // not on the message thread: update the latency from there
}

melon::LoadedFile NamProcessor::modelStatus() const
{
    const juce::ScopedLock sl (statusLock);
    return status;
}

//==============================================================================
// The model path is stored with the parameters, so projects reload it.
void NamProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateXml())
    {
        xml->setAttribute (kModelProp, modelStatus().file.getFullPathName());
        copyXmlToBinary (*xml, destData);
    }
}

void NamProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ProcessorBase::setStateInformation (data, sizeInBytes);
    library.syncToCurrent(); // the saved path below wins over the restored model_index
    const auto path = apvts.copyState().getProperty (kModelProp).toString();
    if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
        loadModel (juce::File (path));
    else
        clearModel();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NamProcessor(); }
