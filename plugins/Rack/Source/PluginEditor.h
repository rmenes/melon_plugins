#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

//==============================================================================
// Knob with its name above, in the series' style (sized by the rack units).
struct RackKnob
{
    juce::Slider slider;
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// Shows a loaded file and lets the user pick one: click the name (file chooser),
// < and > (previous / next file in the same folder), X (unload), or drop a file on it.
class FileSlot : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    FileSlot (const juce::String& caption, const juce::String& wildcard, const juce::String& emptyText);

    std::function<void (const juce::File&)> onLoad;
    std::function<void()>                   onClear;
    std::function<juce::File()>             startFolder;

    void setStatus (const RackProcessor::SlotStatus& status, const juce::String& extraInfo);

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void choose();
    void step (int delta);
    bool matches (const juce::File& f) const;

    juce::String     wildcard, emptyText;
    juce::Label      caption, info;
    juce::TextButton nameButton, prev { "<" }, next { ">" }, clear { "X" };
    juce::File       current;
    bool             dragOver = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
// The rack: five units stacked like 19" rack gear.
class RackView : public juce::Component
{
public:
    explicit RackView (RackProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh(); // called by the editor's timer

private:
    struct Unit
    {
        juce::String         title;
        juce::Rectangle<int> bounds, content;
        juce::ToggleButton*  power = nullptr;
    };

    RackKnob&           addKnob (const juce::String& paramId, const juce::String& name, const juce::String& tip);
    juce::ToggleButton& addPower (const juce::String& paramId, const juce::String& tip);
    juce::ToggleButton& addToggle (const juce::String& paramId, const juce::String& text, const juce::String& tip);
    juce::ComboBox&     addCombo (const juce::String& paramId, const juce::String& tip);
    static void         placeKnob (RackKnob& k, juce::Rectangle<int> r);

    RackProcessor& processor;

    std::vector<std::unique_ptr<RackKnob>>           knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::ComboBox>>     combos;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>   buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;

    Unit units[5]; // input, pedal, eq, amp, cabinet

    // input
    juce::ComboBox* inputChannel = nullptr;
    RackKnob *inputGain = nullptr, *gateThreshold = nullptr;
    juce::ToggleButton* gateOn = nullptr;
    // models
    FileSlot  modelSlot[RackProcessor::kNumModels] {
        { "MODEL", RackProcessor::modelWildcard, "Click to load a .nam, .json or .aidax model (or drop it here)" },
        { "MODEL", RackProcessor::modelWildcard, "Click to load a .nam, .json or .aidax model (or drop it here)" }
    };
    RackKnob*           modelIn[RackProcessor::kNumModels] {};
    RackKnob*           modelOut[RackProcessor::kNumModels] {};
    juce::ToggleButton* modelNorm[RackProcessor::kNumModels] {};
    // eq
    juce::ComboBox* eqPosition = nullptr;
    RackKnob*       eqBand[rack::Eq6::kBands] {};
    // cabinet
    FileSlot irSlot[RackProcessor::kNumIrs] {
        { "IR 1 (LEFT)", RackProcessor::irWildcard, "Click to load an impulse response (.wav) or drop it here" },
        { "IR 2 (RIGHT)", RackProcessor::irWildcard, "Click to load an impulse response (.wav) or drop it here" }
    };
    juce::ComboBox* cabMode = nullptr;
    RackKnob *cabBlend = nullptr, *cabLevel = nullptr;

    juce::Rectangle<int> footer, gateLed;
    bool                 gateLit = false;
    juce::String         latencyText;
};

//==============================================================================
class RackEditor : public melon::EditorBase<RackProcessor>
{
public:
    explicit RackEditor (RackProcessor&);

private:
    void timerCallback() override { rackView.refresh(); }
    RackView rackView;
};
