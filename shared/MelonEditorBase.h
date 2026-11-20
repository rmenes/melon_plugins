// MelonEditorBase.h - the series' look and the standard editor layout (JUCE).
//
//   ┌──────────────────────────────────────────────────────────────┐
//   │ [Melon logo] | Title   [Bypass]                 Output [===] │  header
//   │ Preset [<] [ preset name        ] [>]       [Save] [Save As] │  presets
//   │ ┌──────────────────────────────────────────────────────────┐ │
//   │ │                        display                           │ │  (optional)
//   │ └──────────────────────────────────────────────────────────┘ │
//   │  [<] [ model drop-down ] [>]   [toggle] [button] ...         │  selector row (optional)
//   │ ┌──────────────────────────────────────────────────────────┐ │
//   │ │   (knob)   (knob)   (knob)   (knob)   (knob)              │ │  knob panel
//   │ └──────────────────────────────────────────────────────────┘ │
//   └──────────────────────────────────────────────────────────────┘
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <MelonBranding.h>
#include "MelonPresets.h"
#include "MelonVersion.h"

namespace melon
{
// Series palette (see docs/NEW_PLUGIN.md)
namespace colours
{
const juce::Colour background { 0xff15181d };
const juce::Colour panel      { 0xff1d2128 };
const juce::Colour grid       { 0xff2c323b };
const juce::Colour text       { 0xffc9d1d9 };
const juce::Colour dimText    { 0xff7d8590 };
const juce::Colour accent     { 0xff4fc3f7 };
const juce::Colour second     { 0xffffa657 };
} // namespace colours

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ToggleButton::tickDisabledColourId, dimText);
        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, grid);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::backgroundColourId, grid);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.3f));
        setColour (juce::TextEditor::backgroundColourId, panel);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::TooltipWindow::backgroundColourId, panel);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, grid);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, grid);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::ComboBox::focusedOutlineColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.6f));
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, background);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using namespace colours;
        const auto  bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  centre = bounds.getCentre();
        const float arcR   = radius - 3.0f;
        const float stroke = juce::jlimit (3.0f, 6.0f, radius * 0.09f);
        const float alpha  = slider.isEnabled() ? 1.0f : 0.4f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (grid);
        g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float toAngle = startAngle + sliderPos * (endAngle - startAngle);
        if (toAngle - startAngle > 0.01f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, toAngle, true);
            g.setColour (accent.withAlpha (alpha));
            g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float bodyR = arcR - stroke - 5.0f;
        const auto  body  = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (grid.brighter (0.15f), body.getTopLeft(), panel, body.getBottomRight(), false));
        g.fillEllipse (body);
        g.setColour (background.withAlpha (0.8f));
        g.drawEllipse (body, 1.5f);

        const auto tip   = centre.getPointOnCircumference (bodyR * 0.82f, toAngle);
        const auto start = centre.getPointOnCircumference (bodyR * 0.30f, toAngle);
        g.setColour (text.withAlpha (alpha));
        g.drawLine ({ start, tip }, juce::jmax (2.5f, stroke * 0.7f));
    }
};

//==============================================================================
// Rounded panel with a small title, used as the background of every display.
inline juce::Rectangle<float> drawDisplayPanel (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& title)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (colours::accent);
    g.drawText (title, bounds.reduced (12.0f, 4.0f).withHeight (14.0f).toNearestInt(), juce::Justification::left);
    return bounds.reduced (12.0f, 6.0f).withTrimmedTop (16.0f);
}

//==============================================================================
template <class Processor>
class EditorBase : public juce::AudioProcessorEditor, protected juce::Timer
{
public:
    EditorBase (Processor& p, const juce::String& title) : AudioProcessorEditor (&p), processor (p)
    {
        setLookAndFeel (&lookAndFeel);
        logo = juce::ImageCache::getFromMemory (MelonBranding::melonlogoondarkheader_png,
                                                MelonBranding::melonlogoondarkheader_pngSize);

        titleLabel.setText (title, juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, colours::text);
        addAndMakeVisible (titleLabel);
        setUpVersionLabel (versionLabel);
        addAndMakeVisible (versionLabel);

        addAndMakeVisible (bypassButton);
        bypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "bypass", bypassButton);

        outputSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        outputSlider.setColour (juce::Slider::textBoxOutlineColourId, colours::grid);
        outputSlider.setColour (juce::Slider::textBoxTextColourId, colours::text);
        outputSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        outputSlider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (outputSlider);
        outputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "output", outputSlider);

        outputLabel.setText ("Output", juce::dontSendNotification);
        outputLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (outputLabel);

        addAndMakeVisible (presetBar);
    }

    ~EditorBase() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::background);
        if (logo.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (logo, logoBounds.toFloat(), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
            g.setColour (colours::grid);
            g.fillRect (logoBounds.getRight() + 10, logoBounds.getY() + 6, 1, logoBounds.getHeight() - 12);
        }
        g.setColour (colours::panel);
        g.fillRoundedRectangle (knobPanel.toFloat(), 6.0f);
    }

    void resized() override
    {
        auto area   = getLocalBounds().reduced (12);
        auto header = area.removeFromTop (40);

        const int logoW = logo.isValid() ? juce::roundToInt (40.0f * (float) logo.getWidth() / (float) logo.getHeight()) : 0;
        logoBounds = header.removeFromLeft (logoW);
        header.removeFromLeft (20);
        layoutTitle (header.removeFromLeft (titleWidth), titleLabel, versionLabel);
        header = header.reduced (0, 4);
        bypassButton.setBounds (header.removeFromLeft (90));
        outputSlider.setBounds (header.removeFromRight (190));
        outputLabel.setBounds (header.removeFromRight (50));
        area.removeFromTop (8);
        presetBar.setBounds (area.removeFromTop (kPresetRowHeight));
        area.removeFromTop (10);

        // knob panel
        const int rows   = knobs.empty() ? 0 : (int) ((knobs.size() + (size_t) knobsPerRow - 1) / (size_t) knobsPerRow);
        const int rowH   = rows > 1 ? 158 : 150;
        const int panelH = rows * rowH + 20;
        knobPanel = rows > 0 ? area.removeFromBottom (panelH) : juce::Rectangle<int>();
        layoutKnobs (knobPanel.reduced (8, 10), rows);

        // selector row
        if (! rowItems.empty())
        {
            area.removeFromBottom (8);
            auto row = area.removeFromBottom (30);
            for (auto& item : rowItems)
            {
                if (item.width < 0) // flexible: takes the rest
                {
                    item.comp->setBounds (row);
                    break;
                }
                item.comp->setBounds (row.removeFromLeft (item.width));
                row.removeFromLeft (8);
            }
        }

        area.removeFromBottom (10);
        if (display != nullptr)
            display->setBounds (area);
    }

protected:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider                      slider;
        juce::Label                       label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    Knob& addKnob (const juce::String& paramId, const juce::String& name, const juce::String& tooltip)
    {
        auto& k = *knobs.emplace_back (std::make_unique<Knob>());
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, colours::grid);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::background.withAlpha (0.6f));
        k.slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        k.slider.setTooltip (tooltip);
        addAndMakeVisible (k.slider);

        k.label.setText (name, juce::dontSendNotification);
        k.label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, colours::text);
        k.label.setTooltip (tooltip);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (paramId)))
            k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        return k;
    }

    // Drop-down for a choice parameter, with optional < > buttons to step through the choices.
    juce::ComboBox& addChoiceBox (const juce::String& paramId, int width, bool withArrows, const juce::String& tooltip)
    {
        auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId));
        jassert (param != nullptr);

        auto& box = *combos.emplace_back (std::make_unique<juce::ComboBox>());
        box.addItemList (param->choices, 1);
        box.setTooltip (tooltip);
        box.setJustificationType (juce::Justification::centred);

        if (withArrows)
        {
            auto& prev = makeButton ("<", "Previous");
            auto& next = makeButton (">", "Next");
            prev.onClick = [this, param] { stepChoice (*param, -1); };
            next.onClick = [this, param] { stepChoice (*param, +1); };
            addRowItem (prev, 30);
            addRowItem (box, width);
            addRowItem (next, 30);
        }
        else
        {
            addRowItem (box, width);
        }
        comboAttachments.push_back (std::make_unique<ComboAttachment> (processor.apvts, paramId, box));
        return box;
    }

    juce::ToggleButton& addToggle (const juce::String& paramId, const juce::String& text, int width, const juce::String& tooltip)
    {
        auto& t = *toggles.emplace_back (std::make_unique<juce::ToggleButton> (text));
        t.setTooltip (tooltip);
        addRowItem (t, width);
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (processor.apvts, paramId, t));
        return t;
    }

    juce::TextButton& makeButton (const juce::String& text, const juce::String& tooltip)
    {
        auto& b = *buttons.emplace_back (std::make_unique<juce::TextButton> (text));
        b.setTooltip (tooltip);
        return b;
    }

    // Small text label in the selector row (e.g. "Sync")
    juce::Label& addRowLabel (const juce::String& text, int width)
    {
        auto& l = *labels.emplace_back (std::make_unique<juce::Label> (juce::String(), text));
        l.setJustificationType (juce::Justification::centredRight);
        l.setColour (juce::Label::textColourId, colours::dimText);
        addRowItem (l, width);
        return l;
    }

    // width < 0 = takes the remaining space
    void addRowItem (juce::Component& c, int width)
    {
        addAndMakeVisible (c);
        rowItems.push_back ({ &c, width });
    }

    void setDisplay (juce::Component& c)
    {
        display = &c;
        addAndMakeVisible (c);
    }

    // Sizes are without the preset row; its height is added here.
    void finishSetup (int w, int h, int minW, int minH)
    {
        h += kPresetRowHeight + 8;
        minH += kPresetRowHeight + 8;
        setResizable (true, true);
        setResizeLimits (minW, minH, minW * 2, minH * 2);
        setSize (w, h);
        timerCallback();
        startTimerHz (30);
    }

    void timerCallback() override {}

    static void stepChoice (juce::AudioParameterChoice& p, int delta)
    {
        const int n   = p.choices.size();
        const int idx = (p.getIndex() + delta + n) % n;
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.convertTo0to1 ((float) idx));
        p.endChangeGesture();
    }

    Processor& processor;
    std::vector<std::unique_ptr<Knob>> knobs;
    int knobsPerRow = 6;
    int titleWidth  = 130;

    static constexpr int kPresetRowHeight = 28;

private:
    void layoutKnobs (juce::Rectangle<int> r, int rows)
    {
        if (rows == 0)
            return;
        const int rowH = r.getHeight() / rows;
        for (int row = 0; row < rows; ++row)
        {
            auto line        = r.removeFromTop (rowH).withTrimmedBottom (rows > 1 ? 8 : 0);
            const int first  = row * knobsPerRow;
            const int count  = juce::jmin (knobsPerRow, (int) knobs.size() - first);
            const float colW = (float) line.getWidth() / (float) knobsPerRow;
            const float off  = (float) (knobsPerRow - count) * colW * 0.5f; // centre short rows
            for (int i = 0; i < count; ++i)
            {
                const int x0 = line.getX() + juce::roundToInt (off + colW * (float) i);
                const int x1 = line.getX() + juce::roundToInt (off + colW * (float) (i + 1));
                auto col     = juce::Rectangle<int> (x0, line.getY(), x1 - x0, line.getHeight()).reduced (4, 0);
                auto& k      = *knobs[(size_t) (first + i)];
                k.label.setBounds (col.removeFromTop (22));
                k.slider.setBounds (col);
            }
        }
    }

    struct RowItem
    {
        juce::Component* comp;
        int              width;
    };

    LookAndFeel         lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Image          logo;
    juce::Rectangle<int> logoBounds, knobPanel;
    juce::Label          titleLabel, versionLabel;
    juce::ToggleButton   bypassButton { "Bypass" };
    juce::Slider         outputSlider;
    juce::Label          outputLabel;
    juce::Component*     display = nullptr;
    PresetBar            presetBar { processor.presets };

    std::vector<RowItem>                              rowItems;
    std::vector<std::unique_ptr<juce::ComboBox>>      combos;
    std::vector<std::unique_ptr<juce::ToggleButton>>  toggles;
    std::vector<std::unique_ptr<juce::TextButton>>    buttons;
    std::vector<std::unique_ptr<juce::Label>>         labels;
    std::vector<std::unique_ptr<ComboAttachment>>     comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>>    buttonAttachments;
    std::unique_ptr<ButtonAttachment>                 bypassAttachment;
    std::unique_ptr<SliderAttachment>                 outputAttachment;
};

} // namespace melon
