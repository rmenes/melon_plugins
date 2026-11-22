#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"
#include <juce_audio_formats/juce_audio_formats.h>

const juce::String CabProcessor::irWildcard = "*.wav;*.aif;*.aiff;*.flac";

namespace
{
constexpr const char* kIrProp = "ir"; // the IR file path, saved with projects and user presets

juce::String rateText (double hz)
{
    const double k = hz / 1000.0;
    return (std::abs (k - std::round (k)) < 0.01 ? juce::String (juce::roundToInt (k)) : juce::String (k, 1)) + " kHz";
}
} // namespace

//==============================================================================
CabProcessor::CabProcessor() : ProcessorBase (createParameterLayout(), 0.5)
{
    lastIrFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    // Factory presets only set the controls and keep the loaded IR. User presets also remember the IR.
    presets.setFactoryPresets (cab::factoryPresets());
    presets.writeExtras = [this] (juce::XmlElement& xml) {
        xml.setAttribute (kIrProp, irStatus().file.getFullPathName());
    };
    presets.readExtras = [this] (const juce::XmlElement& xml) {
        if (! xml.hasAttribute (kIrProp))
            return; // preset saved without an IR: keep the current one
        const auto path = xml.getStringAttribute (kIrProp);
        if (path.isEmpty())
            clearIr();
        else if (juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
            loadIr (juce::File (path));
    };
}

melon::params::Layout CabProcessor::createParameterLayout()
{
    using namespace melon::params;
    Layout l;

    addFloat (l, "lowcut", "Low Cut", (float) cab::kLowCutOff, 500.0f, (float) cab::kLowCutOff, "Hz", 0, 80.0f);
    addFloat (l, "highcut", "High Cut", 2000.0f, (float) cab::kHighCutOff, (float) cab::kHighCutOff, "Hz", 0, 7000.0f);
    addPercent (l, "mix", "Mix", 100.0f);

    addOutputAndBypass (l);
    return l;
}

juce::AudioProcessorEditor* CabProcessor::createEditor() { return new CabEditor (*this); }

//==============================================================================
void CabProcessor::prepareDsp (double sampleRate, int maxBlock, int numChannels)
{
    maxBlockSize = juce::jmax (16, maxBlock);
    dry.setSize (juce::jmax (2, numChannels), maxBlockSize);

    convolution.prepare ({ sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) juce::jlimit (1, 2, numChannels) });
    convolution.reset();
    filters.prepare (sampleRate);

    for (auto* s : { &wetGain, &dryGain, &outputGain, &bypassMix })
        s->prepare (sampleRate, 15.0);
    const double mix = value ("mix") * 0.01;
    wetGain.setTarget (mix);
    dryGain.setTarget (1.0 - mix);
    outputGain.setTarget (melon::dbToGain (value ("output")));
    bypassMix.setTarget (flag ("bypass") ? 0.0 : 1.0);
    for (auto* s : { &wetGain, &dryGain, &outputGain, &bypassMix })
        s->snap();
}

void CabProcessor::processDsp (float* const* data, int numChannels, int numSamples)
{
    numChannels = juce::jmin (numChannels, cab::CutFilters::kMaxChannels);

    for (int offset = 0; offset < numSamples;)
    {
        const int n = juce::jmin (maxBlockSize, numSamples - offset);

        for (int ch = 0; ch < numChannels; ++ch)
            dry.copyFrom (ch, 0, data[ch] + offset, n);

        // --- cabinet -------------------------------------------------------------------------
        if (irActive.load())
        {
            // Mono source and mono IR: one convolution is enough, the result goes to both sides
            const bool monoSource = getTotalNumInputChannels() == 1 && ! irStereo.load();
            const int  convCh     = monoSource ? 1 : numChannels;
            juce::dsp::AudioBlock<float> block (data, (size_t) convCh, (size_t) offset, (size_t) n);
            convolution.process (juce::dsp::ProcessContextReplacing<float> (block));
            for (int ch = convCh; ch < numChannels; ++ch)
                std::copy (data[0] + offset, data[0] + offset + n, data[ch] + offset);
        }

        // --- low cut / high cut ------------------------------------------------------------------
        filters.set (value ("lowcut"), value ("highcut"));
        for (int ch = 0; ch < numChannels; ++ch)
            filters.process (data[ch] + offset, n, ch);

        // --- mix, output, bypass -----------------------------------------------------------------
        const double mix = value ("mix") * 0.01;
        wetGain.setTarget (mix);
        dryGain.setTarget (1.0 - mix);
        outputGain.setTarget (melon::dbToGain (value ("output")));
        bypassMix.setTarget (flag ("bypass") ? 0.0 : 1.0);

        float* out[2] = { data[0] + offset, numChannels > 1 ? data[1] + offset : nullptr };
        const float* d[2] = { dry.getReadPointer (0), dry.getReadPointer (numChannels > 1 ? 1 : 0) };
        for (int i = 0; i < n; ++i)
        {
            const float wg = (float) wetGain.next(), dg = (float) dryGain.next();
            const float og = (float) outputGain.next(), b = (float) bypassMix.next();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float y = (wg * out[ch][i] + dg * d[ch][i]) * og;
                out[ch][i]    = b * y + (1.0f - b) * d[ch][i];
            }
        }

        offset += n;
    }
}

//==============================================================================
void CabProcessor::loadIr (const juce::File& file)
{
    lastIrFolder = file.getParentDirectory();
    melon::LoadedFile st { file, {}, false, false };

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader;
    if (file.existsAsFile())
        reader.reset (fm.createReaderFor (file));

    if (! file.existsAsFile())
    {
        st.info  = "File not found: " + file.getFullPathName();
        st.error = true;
        irActive = false;
    }
    else if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        st.info  = "Could not read this file as audio (use a .wav, .aiff or .flac impulse response)";
        st.error = true;
        irActive = false;
    }
    else
    {
        const bool stereo = reader->numChannels >= 2;
        // Trims silence at the start and end, normalizes, and resamples to the session rate
        convolution.loadImpulseResponse (file, stereo ? juce::dsp::Convolution::Stereo::yes : juce::dsp::Convolution::Stereo::no,
                                         juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
        irStereo = stereo;
        irActive = true;
        st.info  = juce::String (stereo ? "Stereo" : "Mono") + " IR, " + juce::String (reader->lengthInSamples * 1000.0 / reader->sampleRate, 0)
                  + " ms, " + rateText (reader->sampleRate);
        const double host = sampleRateForDisplay();
        if (std::abs (reader->sampleRate - host) > 1.0)
            st.info << " (resampled, the session runs at " << rateText (host) << ")";
    }

    const juce::ScopedLock sl (statusLock);
    status = st;
}

void CabProcessor::clearIr()
{
    irActive = false;
    const juce::ScopedLock sl (statusLock);
    status = {};
}

melon::LoadedFile CabProcessor::irStatus() const
{
    const juce::ScopedLock sl (statusLock);
    return status;
}

//==============================================================================
// The IR path is stored with the parameters, so projects reload it.
void CabProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateXml())
    {
        xml->setAttribute (kIrProp, irStatus().file.getFullPathName());
        copyXmlToBinary (*xml, destData);
    }
}

void CabProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ProcessorBase::setStateInformation (data, sizeInBytes);
    const auto path = apvts.copyState().getProperty (kIrProp).toString();
    if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
        loadIr (juce::File (path));
    else
        clearIr();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CabProcessor(); }
