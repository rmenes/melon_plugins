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
FlangerLookAndFeel::FlangerLookAndFeel()
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

void FlangerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                           float startAngle, float endAngle, juce::Slider& slider)
{
    const auto  bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float arcR   = radius - 3.0f;
    const float stroke = juce::jlimit (3.0f, 6.0f, radius * 0.09f);
    const float alpha  = slider.isEnabled() ? 1.0f : 0.4f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::grid);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float toAngle = startAngle + sliderPos * (endAngle - startAngle);
    if (toAngle - startAngle > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, toAngle, true);
        g.setColour (colours::accent.withAlpha (alpha));
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = arcR - stroke - 5.0f;
    const auto  body  = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (colours::grid.brighter (0.15f), body.getTopLeft(),
                                             colours::panel, body.getBottomRight(), false));
    g.fillEllipse (body);
    g.setColour (colours::background.withAlpha (0.8f));
    g.drawEllipse (body, 1.5f);

    const auto tip   = centre.getPointOnCircumference (bodyR * 0.82f, toAngle);
    const auto start = centre.getPointOnCircumference (bodyR * 0.30f, toAngle);
    g.setColour (colours::text.withAlpha (alpha));
    g.drawLine ({ start, tip }, juce::jmax (2.5f, stroke * 0.7f));
}

//==============================================================================
void CombDisplay::setState (const flanger::Params& params, double delayNowMs, double sampleRate)
{
    p       = params;
    delayMs = delayNowMs;
    fs      = sampleRate > 0.0 ? sampleRate : 48000.0;
    repaint();
}

void CombDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto plot = bounds.reduced (12.0f, 22.0f).withTrimmedLeft (34.0f);
    constexpr double fLo = 20.0, fHi = 20000.0, dbLo = -30.0, dbHi = 18.0;
    const double     fMax = juce::jmin (fHi, 0.49 * fs);

    auto xForHz = [&] (double f) {
        return plot.getX() + plot.getWidth() * (float) (std::log (f / fLo) / std::log (fHi / fLo));
    };
    auto yForDb = [&] (double db) {
        return juce::jmap ((float) juce::jlimit (dbLo, dbHi, db), (float) dbLo, (float) dbHi, plot.getBottom(), plot.getY());
    };

    // band covered by the first notch over one sweep
    if (! p.bypassed && p.width > 0.001)
    {
        const double lo = flanger::firstNotchHz (flanger::delayMs (1.0, p.manualMs, p.width));
        const double hi = flanger::firstNotchHz (flanger::delayMs (-1.0, p.manualMs, p.width));
        const float  x0 = xForHz (juce::jlimit (fLo, fHi, lo));
        const float  x1 = xForHz (juce::jlimit (fLo, fHi, hi));
        g.setColour (colours::accent.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (x0, plot.getY(), x1 - x0, plot.getHeight()));
    }

    // grid
    g.setFont (11.0f);
    for (double db : { -24.0, -12.0, 0.0, 12.0 })
    {
        const float y = yForDb (db);
        g.setColour (juce::exactlyEqual (db, 0.0) ? colours::grid.brighter (0.2f) : colours::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (colours::dimText);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) bounds.getX() + 6, (int) y - 7, 30, 14,
                    juce::Justification::left);
    }
    const std::pair<double, const char*> freqs[] = { { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } };
    for (auto [f, name] : freqs)
    {
        const float x = xForHz (f);
        g.setColour (colours::grid);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (colours::dimText);
        g.drawText (name, (int) x - 20, (int) plot.getBottom() + 4, 40, 14, juce::Justification::centred);
    }

    // readout
    g.setColour (colours::dimText);
    const double dLo = flanger::delayMs (-1.0, p.manualMs, p.width), dHi = flanger::delayMs (1.0, p.manualMs, p.width);
    const auto   readout = p.bypassed ? juce::String ("Bypassed")
                                      : "Delay " + juce::String (dLo, 2) + " - " + juce::String (dHi, 2) + " ms   |   "
                                            + juce::String (p.speedHz, 2) + " Hz   |   now " + juce::String (delayMs, 2) + " ms";
    g.drawText (readout, plot.withY (bounds.getY() + 4).withHeight (14).toNearestInt(), juce::Justification::right);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (colours::accent);
    g.drawText ("Response", (int) plot.getX(), (int) bounds.getY() + 4, 80, 14, juce::Justification::left);

    // response curve: enough points to resolve the narrow notches
    const int numPoints = juce::jmax (200, (int) plot.getWidth() * 2);
    juce::Path curve, fill;
    bool       started = false;
    for (int i = 0; i < numPoints; ++i)
    {
        const double f = fLo * std::pow (fHi / fLo, (double) i / (numPoints - 1));
        if (f > fMax)
            break;
        const double db = p.bypassed ? 0.0 : flanger::responseDb (f, fs, p, delayMs);
        const float  x = xForHz (f), y = yForDb (db);
        if (! started)
        {
            curve.startNewSubPath (x, y);
            fill.startNewSubPath (x, plot.getBottom());
            started = true;
        }
        curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (curve.getCurrentPosition().x, plot.getBottom());
    fill.closeSubPath();

    const float alpha = p.bypassed ? 0.35f : 1.0f;
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.25f * alpha), 0.0f, plot.getY(),
                                             colours::accent.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (colours::accent.withAlpha (alpha));
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // marker on the first notch
    if (! p.bypassed && p.mix > 0.0)
    {
        const double f = flanger::firstNotchHz (delayMs);
        if (f >= fLo && f <= fMax)
        {
            const float x = xForHz (f);
            g.setColour (colours::second);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x, plot.getBottom() }));
        }
    }
}

//==============================================================================
FlangerEditor::FlangerEditor (FlangerProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    logo = juce::ImageCache::getFromMemory (MelonBranding::melonlogoondarkheader_png,
                                            MelonBranding::melonlogoondarkheader_pngSize);

    titleLabel.setText ("Flanger", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (titleLabel);
    melon::setUpVersionLabel (versionLabel);
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

    addAndMakeVisible (display);

    setUpKnob (manual, "manual", "Manual", 2.0,
               "Centre of the sweep (the delay time). Short = high, airy notches; long = low, hollow notches. With Width at 0 this sets a fixed flange");
    setUpKnob (width, "width", "Width", 60.0,
               "How far the sweep travels around Manual (up to 1.5 octaves either way). 0 = no movement");
    setUpKnob (speed, "speed", "Speed", 0.4,
               "Speed of the sweep. Slow = the classic jet-plane whoosh; fast = warble");
    setUpKnob (regen, "regen", "Regen", 50.0,
               "Feedback. Turn up for a more resonant, metallic, ringing flange");
    setUpKnob (mix, "mix", "Mix", 50.0,
               "Dry/wet balance. 50 = dry and flanged both at full level (deepest notches, like the pedal), 100 = flanged only (vibrato-like)");

    setResizable (true, true);
    setResizeLimits (620, 416, 1400, 896);
    setSize (720, 466);

    timerCallback();
    startTimerHz (30);
}

FlangerEditor::~FlangerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FlangerEditor::setUpKnob (Knob& k, const juce::String& paramId, const juce::String& name, double defaultValue,
                               const juce::String& tooltip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    // set directly on the slider: the value box is created before the editor's look-and-feel reaches it
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, colours::grid);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::background.withAlpha (0.6f));
    k.slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, defaultValue);
    k.slider.setTooltip (tooltip);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
}

void FlangerEditor::paint (juce::Graphics& g)
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

void FlangerEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto header = area.removeFromTop (40);

    // logo keeps its aspect ratio (stored at 2x for high-DPI screens)
    const int logoW = logo.isValid() ? juce::roundToInt (40.0f * (float) logo.getWidth() / (float) logo.getHeight()) : 0;
    logoBounds = header.removeFromLeft (logoW);
    header.removeFromLeft (20);
    melon::layoutTitle (header.removeFromLeft (90), titleLabel, versionLabel);
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
    Knob* list[]     = { &manual, &width, &speed, &regen, &mix };
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

void FlangerEditor::timerCallback()
{
    display.setState (processor.currentParams(), (double) processor.currentDelayMs(), processor.sampleRateForDisplay());
}
