#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <MelonPresets.h>

//==============================================================================
class ChorusLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChorusLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

//==============================================================================
// Scrolling view of the delay sweep for the left and right channels.
// "Now" is at the right edge; the last two LFO cycles scroll to the left.
class ModulationDisplay : public juce::Component
{
public:
    void setState (const chorus::Params& params, double lfoPhase);
    void paint (juce::Graphics&) override;

private:
    chorus::Params p;
    double phase = 0.0;
};

//==============================================================================
class ChorusEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChorusEditor (ChorusProcessor&);
    ~ChorusEditor() override;

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
                    bool bipolar, const juce::String& tooltip);

    ChorusProcessor& processor;
    ChorusLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Image          logo;
    juce::Rectangle<int> logoBounds, knobPanel;
    juce::Label          titleLabel;
    juce::ToggleButton   bypassButton { "Bypass" };
    juce::Slider         outputSlider;
    juce::Label          outputLabel;
    melon::PresetBar presetBar { processor.presets };
    ModulationDisplay    display;

    Knob rate, depth, width, mix, tone;

    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChorusEditor)
};
