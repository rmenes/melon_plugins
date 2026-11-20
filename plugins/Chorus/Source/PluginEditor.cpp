#include "PluginEditor.h"
#include <MelonBranding.h>

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
ChorusLookAndFeel::ChorusLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::dimText);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, colours::grid);
    setColour (juce::Slider::thumbColourId, colours::text);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::backgroundColourId, colours::grid);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.3f));
    setColour (juce::TextEditor::backgroundColourId, colours::panel);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    setColour (juce::CaretComponent::caretColourId, colours::accent);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::grid);
}

void ChorusLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto  bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float arcR   = radius - 3.0f;
    const float stroke = juce::jlimit (3.0f, 6.0f, radius * 0.09f);
    const float alpha  = slider.isEnabled() ? 1.0f : 0.4f;

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::grid);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (bipolar knobs fill from the centre)
    const bool  bipolar   = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const float fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const float toAngle   = startAngle + sliderPos * (endAngle - startAngle);
    if (std::abs (toAngle - fromAngle) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (fromAngle, toAngle),
                             juce::jmax (fromAngle, toAngle), true);
        g.setColour (colours::accent.withAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // knob body
    const float bodyR = arcR - stroke - 5.0f;
    const auto  body  = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (colours::grid.brighter (0.15f), body.getTopLeft(),
                                             colours::panel, body.getBottomRight(), false));
    g.fillEllipse (body);
    g.setColour (colours::background.withAlpha (0.8f));
    g.drawEllipse (body, 1.5f);

    // pointer
    const auto tip   = centre.getPointOnCircumference (bodyR * 0.82f, toAngle);
    const auto start = centre.getPointOnCircumference (bodyR * 0.30f, toAngle);
    g.setColour (colours::text.withAlpha (alpha));
    g.drawLine ({ start, tip }, juce::jmax (2.5f, stroke * 0.7f));
}

//==============================================================================
void ModulationDisplay::setState (const chorus::Params& params, double lfoPhase)
{
    p     = params;
    phase = lfoPhase;
    repaint();
}

void ModulationDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto  plot   = bounds.reduced (12.0f, 22.0f).withTrimmedLeft (34.0f);
    const float minMs  = (float) (chorus::kBaseDelayMs - chorus::kMaxSweepMs);
    const float maxMs  = (float) (chorus::kBaseDelayMs + chorus::kMaxSweepMs);
    auto yForMs = [&] (double ms) {
        return juce::jmap ((float) ms, minMs, maxMs, plot.getBottom(), plot.getY());
    };

    // grid: delay in ms
    g.setFont (11.0f);
    const double gridMs[] = { (double) minMs, chorus::kBaseDelayMs, (double) maxMs };
    for (int i = 0; i < 3; ++i)
    {
        const double ms = gridMs[i];
        const float  y  = yForMs (ms);
        g.setColour (i == 1 ? colours::grid.brighter (0.2f) : colours::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (colours::dimText);
        g.drawText (juce::String ((int) ms) + " ms", (int) bounds.getX() + 6, (int) y - 7, 40, 14,
                    juce::Justification::left);
    }

    // legend and readout
    const auto lineAlpha = p.bypassed ? 0.35f : 1.0f;
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (colours::accent.withAlpha (lineAlpha));
    g.drawText ("L", (int) plot.getX(), (int) bounds.getY() + 4, 14, 14, juce::Justification::left);
    g.setColour (colours::second.withAlpha (lineAlpha));
    g.drawText ("R", (int) plot.getX() + 18, (int) bounds.getY() + 4, 14, 14, juce::Justification::left);

    g.setFont (11.0f);
    g.setColour (colours::dimText);
    const double lo = chorus::delayMs (-1.0, p.depth), hi = chorus::delayMs (1.0, p.depth);
    const auto readout = p.bypassed ? juce::String ("Bypassed")
                                    : "Delay sweep " + juce::String (lo, 1) + " - " + juce::String (hi, 1) + " ms   |   "
                                          + juce::String (p.rateHz, 2) + " Hz";
    g.drawText (readout, plot.withY (bounds.getY() + 4).withHeight (14).toNearestInt(), juce::Justification::right);
    g.drawText ("now", plot.withY (plot.getBottom() + 4).withHeight (14).toNearestInt(), juce::Justification::right);
    g.drawText ("2 LFO cycles ago", plot.withY (plot.getBottom() + 4).withHeight (14).toNearestInt(),
                juce::Justification::left);

    // the two delay curves: window covers the last two LFO cycles, ending at the current phase
    constexpr int numPoints = 240;
    for (int ch = 1; ch >= 0; --ch)
    {
        const double offset = ch == 1 ? 0.5 * p.width : 0.0;
        juce::Path path;
        for (int i = 0; i < numPoints; ++i)
        {
            const double t  = (double) i / (numPoints - 1);
            const double ph = phase - 2.0 + 2.0 * t + offset;
            const float  x  = plot.getX() + (float) t * plot.getWidth();
            const float  y  = yForMs (chorus::delayMs (std::sin (2.0 * chorus::kPi * ph), p.depth));
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        const auto colour = (ch == 0 ? colours::accent : colours::second).withAlpha (lineAlpha);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));

        const float yNow = yForMs (chorus::delayMs (std::sin (2.0 * chorus::kPi * (phase + offset)), p.depth));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ plot.getRight(), yNow }));
    }
}

//==============================================================================
ChorusEditor::ChorusEditor (ChorusProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    logo = juce::ImageCache::getFromMemory (MelonBranding::melonlogoondarkheader_png,
                                            MelonBranding::melonlogoondarkheader_pngSize);

    titleLabel.setText ("Chorus", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (titleLabel);

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

    addAndMakeVisible (display);

    setUpKnob (rate, "rate", "Rate", 0.8, false, "Speed of the chorus sweep");
    setUpKnob (depth, "depth", "Depth", 50.0, false, "How far the pitch wobbles. Low = subtle shimmer, high = deep, seasick warble");
    setUpKnob (width, "width", "Width", 70.0, false, "Stereo spread. 0 = mono chorus, 100 = left and right sweep in opposite directions");
    setUpKnob (mix, "mix", "Mix", 50.0, false, "Dry/wet balance. 50 = dry and chorus both at full level (classic pedal sound), 100 = chorus only");
    setUpKnob (tone, "tone", "Tone", 0.0, true, "Tone of the chorus signal. Left = darker and warmer, right = brighter");

    setResizable (true, true);
    setResizeLimits (620, 416, 1400, 896);
    setSize (720, 466);

    timerCallback(); // show the current settings straight away
    startTimerHz (30);
}

ChorusEditor::~ChorusEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ChorusEditor::setUpKnob (Knob& k, const juce::String& paramId, const juce::String& name, double defaultValue,
                              bool bipolar, const juce::String& tooltip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    // set directly on the slider: the value box is created before the editor's look-and-feel reaches it
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, colours::grid);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::background.withAlpha (0.6f));
    k.slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, defaultValue);
    k.slider.getProperties().set ("bipolar", bipolar);
    k.slider.setTooltip (tooltip);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
}

void ChorusEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    if (logo.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (logo, logoBounds.toFloat(), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);

        // thin divider between the Melon logo and the plugin name
        g.setColour (colours::grid);
        g.fillRect (logoBounds.getRight() + 10, logoBounds.getY() + 6, 1, logoBounds.getHeight() - 12);
    }

    g.setColour (colours::panel);
    g.fillRoundedRectangle (knobPanel.toFloat(), 6.0f);
}

void ChorusEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto header = area.removeFromTop (40);

    // logo keeps its aspect ratio (stored at 2x for high-DPI screens)
    const int logoW = logo.isValid() ? juce::roundToInt (40.0f * (float) logo.getWidth() / (float) logo.getHeight()) : 0;
    logoBounds = header.removeFromLeft (logoW);
    header.removeFromLeft (20);
    titleLabel.setBounds (header.removeFromLeft (90));
    header = header.reduced (0, 4);
    bypassButton.setBounds (header.removeFromLeft (90));
    outputSlider.setBounds (header.removeFromRight (190));
    outputLabel.setBounds (header.removeFromRight (50));

    area.removeFromTop (8);
    presetBar.setBounds (area.removeFromTop (28));
    area.removeFromTop (10);
    knobPanel = area.removeFromBottom (juce::jmax (180, area.getHeight() * 11 / 20));
    area.removeFromBottom (10);
    display.setBounds (area);

    auto knobs       = knobPanel.reduced (8, 10);
    Knob* list[]     = { &rate, &depth, &width, &mix, &tone };
    const float colW = (float) knobs.getWidth() / 5.0f;

    for (int i = 0; i < 5; ++i)
    {
        const int x0 = knobs.getX() + juce::roundToInt (colW * (float) i);
        const int x1 = knobs.getX() + juce::roundToInt (colW * (float) (i + 1));
        auto col     = juce::Rectangle<int> (x0, knobs.getY(), x1 - x0, knobs.getHeight()).reduced (4, 0);
        list[i]->label.setBounds (col.removeFromTop (22));
        list[i]->slider.setBounds (col);
    }
}

void ChorusEditor::timerCallback()
{
    display.setState (processor.currentParams(), (double) processor.lfoPhase());
}
