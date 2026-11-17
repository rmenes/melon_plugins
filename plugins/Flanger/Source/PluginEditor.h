#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class FlangerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlangerLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

//==============================================================================
// Live frequency response of the flanger: the comb-filter notches move as the delay sweeps.
// The shaded band shows how far the first notch travels over one sweep.
class CombDisplay : public juce::Component
{
public:
    void setState (const flanger::Params& params, double delayNowMs, double sampleRate);
    void paint (juce::Graphics&) override;

private:
    flanger::Params p;
    double delayMs = 2.0, fs = 48000.0;
};

//==============================================================================
class FlangerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FlangerEditor (FlangerProcessor&);
    ~FlangerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider                      slider;
        juce::Label                       label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setUpKnob (Knob&, const juce::String& paramId, const juce::String& name, double defaultValue,
                    const juce::String& tooltip);

    FlangerProcessor& processor;
    FlangerLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Image          logo;
    juce::Rectangle<int> logoBounds, knobPanel;
    juce::Label          titleLabel;
    juce::ToggleButton   bypassButton { "Bypass" };
    juce::Slider         outputSlider;
    juce::Label          outputLabel;
    CombDisplay          display;

    Knob manual, width, speed, regen, mix;

    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangerEditor)
};
