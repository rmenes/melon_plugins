#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class EqLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EqLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
};

//==============================================================================
class ResponseCurve : public juce::Component
{
public:
    void setData (std::vector<juce::Point<float>> curveDb, std::vector<juce::Point<float>> bandPointsDb, bool bypassed);
    void paint (juce::Graphics&) override;

    static constexpr float minFreq = 20.0f, maxFreq = 20000.0f, rangeDb = 15.0f;

private:
    float xForFreq (float f) const;
    float yForDb (float db) const;

    std::vector<juce::Point<float>> curve, points; // x = frequencia (Hz), y = dB
    bool isBypassed = false;
};

//==============================================================================
class GraphicEqEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GraphicEqEditor (GraphicEqProcessor&);
    ~GraphicEqEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Band
    {
        std::unique_ptr<juce::Slider>     slider;
        std::unique_ptr<juce::Label>      label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void updateVisibleBands();
    void updateCurve();
    void resetBands();

    GraphicEqProcessor& processor;
    EqLookAndFeel lookAndFeel;

    juce::Image        logo;
    juce::Rectangle<int> logoBounds;
    juce::Label        titleLabel;
    juce::ComboBox     modeBox;
    juce::ToggleButton preciseButton { "Precise mode" };
    juce::ToggleButton bypassButton { "Bypass" };
    juce::TextButton   resetButton { "Reset" };
    juce::Slider       outputSlider;
    juce::Label        outputLabel;
    ResponseCurve      curve;

    std::vector<Band> bands10, bands31;

    std::unique_ptr<ComboBoxAttachment> modeAttachment;
    std::unique_ptr<ButtonAttachment>   preciseAttachment, bypassAttachment;
    std::unique_ptr<SliderAttachment>   outputAttachment;

    int shownBands = 0;
    geq::Design design;
    int    designBands = 0;
    double designFs    = 0.0;
    std::vector<double> lastSnapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphicEqEditor)
};
