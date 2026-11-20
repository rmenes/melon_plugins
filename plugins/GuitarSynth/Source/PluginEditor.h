#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <MelonPresets.h>

//==============================================================================
class GuitarSynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GuitarSynthLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int  getSliderThumbRadius (juce::Slider&) override;
};

//==============================================================================
// Live view of the swept filter: its response at the current cutoff, the Start → Stop range,
// an input meter with the Trigger threshold, a trigger LED and the note being tracked.
class FilterDisplay : public juce::Component
{
public:
    struct State
    {
        gsynth::Params params;
        double cutoffHz = 300.0, level = 0.0, pitchHz = 0.0, sampleRate = 48000.0;
        float  ledGlow = 0.0f;
    };

    void setState (const State&);
    void paint (juce::Graphics&) override;

private:
    State s;
};

//==============================================================================
class GuitarSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GuitarSynthEditor (GuitarSynthProcessor&);
    ~GuitarSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Fader
    {
        juce::Slider                      slider;
        juce::Label                       label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setUpFader (Fader&, const juce::String& paramId, const juce::String& name, double defaultValue,
                     const juce::String& tooltip);

    GuitarSynthProcessor& processor;
    GuitarSynthLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Image          logo;
    juce::Rectangle<int> logoBounds, faderPanel;
    juce::Rectangle<int> voiceCaption, filterCaption;  // section titles above the fader groups
    std::vector<int>     dividers;                     // x positions of the lines between sections
    juce::Label          titleLabel;
    juce::ToggleButton   bypassButton { "Bypass" };
    juce::Slider         outputSlider;
    juce::Label          outputLabel;
    melon::PresetBar presetBar { processor.presets };
    FilterDisplay        display;

    // pedal order: Trigger | Sub Octave, Guitar, Octave, Square Wave | Attack Delay | Resonance, Start, Stop, Rate | Mix
    Fader trigger, subOctave, guitar, octave, square, attack, resonance, start, stop, rate, mix;

    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;

    int   lastTriggers = 0;
    float ledGlow      = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarSynthEditor)
};
