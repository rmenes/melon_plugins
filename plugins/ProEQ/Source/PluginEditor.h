#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

//==============================================================================
// Reads and writes band parameters from the interface (with host undo/automation gestures).
class BandOps
{
public:
    explicit BandOps (ProEqProcessor& p) : proc (p) {}

    juce::RangedAudioParameter* param (int band, const char* field) const
    {
        return proc.apvts.getParameter (ProEqProcessor::bandId (band, field));
    }
    float get (int band, const char* field) const
    {
        auto* p = param (band, field);
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
    }
    void set (int band, const char* field, float value) const
    {
        if (auto* p = param (band, field))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
    void setWithGesture (int band, const char* field, float value) const
    {
        if (auto* p = param (band, field))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }
    void begin (int band, const char* field) const { if (auto* p = param (band, field)) p->beginChangeGesture(); }
    void end (int band, const char* field) const { if (auto* p = param (band, field)) p->endChangeGesture(); }

    bool isUsed (int band) const { return band >= 0 && band < proeq::kNumBands && get (band, "used") > 0.5f; }

    // Creates a band in the first free slot; returns its index or -1 if all 24 are in use.
    int create (proeq::Type type, float freq, float gainDb) const;
    void remove (int band) const;

    ProEqProcessor& proc;
};

//==============================================================================
// The interactive EQ display: grid, spectrum analyzer, band curves and draggable band nodes.
class EqGraph : public juce::Component
{
public:
    EqGraph (ProEqProcessor&, BandOps&);

    std::function<void (int)> onSelect;
    void setSelected (int band) { selected = band; repaint(); }

    void refresh(); // 30 times per second: new analyzer frame + repaint

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    float  xForFreq (double f) const;
    double freqForX (float x) const;
    float  yForDb (double db) const;
    double dbForY (float y) const;
    float  yForSpectrum (double db) const;
    double rangeDb() const;
    juce::Point<float> nodePosition (int band) const;
    int    nodeAt (juce::Point<float> p) const;
    void   select (int band);
    void   showBandMenu (int band);
    void   showEmptyMenu (juce::Point<float> p);
    void   updateAnalyzer();
    void   computeSpectrum (const proeq::AnalyzerFifo&, int& lastWrite, std::vector<float>& columns);
    void   drawCurve (juce::Graphics&, const std::vector<proeq::Cascade>& cascades, const std::vector<int>& bands,
                      juce::Colour, float thickness, bool fill);

    ProEqProcessor& proc;
    BandOps&        ops;
    proeq::Params   params;
    double          fs = 48000.0;
    std::array<float, proeq::kNumBands> dyn {};
    juce::Rectangle<float> plot;

    int selected = -1, hover = -1, dragBand = -1;
    juce::Point<float> dragStart;
    double startFreq = 1000.0, startGain = 0.0, startQ = 1.0;

    // analyzer
    static constexpr int kFftSize = 8192;
    proeq::Fft fft;
    std::vector<float> samples, window;
    std::vector<proeq::cd> fftBuf;
    std::vector<float> preCols, postCols, colFreqLo, colFreqHi;
    int lastPreWrite = -1, lastPostWrite = -1;
    float windowGain = 1.0f;
};

//==============================================================================
// Controls of the selected band, plus the global Gain Scale and dynamic Attack / Release.
class BandPanel : public juce::Component
{
public:
    BandPanel (ProEqProcessor&, BandOps&);

    std::function<void (int)> onSelect;
    void setBand (int band);
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& name, const juce::String& tooltip);
    void setupCombo (juce::ComboBox&, const juce::StringArray& items, const juce::String& tooltip);
    void step (int delta);
    static void layoutKnob (Knob&, juce::Rectangle<int>);

    ProEqProcessor& proc;
    BandOps&        ops;
    int band = -2; // -2 = not set up yet

    juce::Label title, hint, globalTitle;
    juce::TextButton prev { "<" }, next { ">" }, del { "Delete" };
    juce::ToggleButton onButton { "On" }, soloButton { "Solo" };
    juce::ComboBox typeBox, slopeBox, stereoBox;
    Knob freq, gain, q, dynRange, dynThr, scale, attack, release;

    std::unique_ptr<ButtonAttachment> onAttachment;
    std::unique_ptr<ComboAttachment>  typeAttachment, slopeAttachment, stereoAttachment;

    juce::Rectangle<int> globalArea;
};

//==============================================================================
class ProEqMain : public juce::Component
{
public:
    explicit ProEqMain (ProEqProcessor&);
    void refresh();
    void resized() override;

private:
    void select (int band);

    ProEqProcessor& proc;
    BandOps   ops;
    EqGraph   graph;
    BandPanel panel;
};

//==============================================================================
class ProEqEditor : public melon::EditorBase<ProEqProcessor>
{
public:
    explicit ProEqEditor (ProEqProcessor&);

private:
    void timerCallback() override;

    ProEqMain       main;
    juce::ComboBox* resolutionBox = nullptr;
    juce::ComboBox  analyzerBox, rangeBox;
    juce::Label     latencyLabel;
};
