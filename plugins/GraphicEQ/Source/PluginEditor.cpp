#include "PluginEditor.h"
#include <MelonBranding.h>

namespace colours
{
const juce::Colour background { 0xff15181d };
const juce::Colour panel      { 0xff1d2128 };
const juce::Colour grid       { 0xff2c323b };
const juce::Colour text       { 0xffc9d1d9 };
const juce::Colour dimText    { 0xff7d8590 };
const juce::Colour accent     { 0xff4fc3f7 };
const juce::Colour boost      { 0xff4fc3f7 };
const juce::Colour cut        { 0xffffa657 };
} // namespace colours

//==============================================================================
EqLookAndFeel::EqLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::dimText);
    setColour (juce::TextButton::buttonColourId, colours::panel);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::panel);
    setColour (juce::ComboBox::outlineColourId, colours::grid);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::text);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.3f));
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, colours::grid);
    setColour (juce::Slider::thumbColourId, colours::text);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::BubbleComponent::backgroundColourId, colours::panel);
    setColour (juce::BubbleComponent::outlineColourId, colours::accent);
    setColour (juce::TooltipWindow::textColourId, colours::text);
}

void EqLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                      float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
        return;
    }

    const auto bounds  = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float cx     = bounds.getCentreX();
    const float trackW = juce::jlimit (2.0f, 4.0f, bounds.getWidth() * 0.12f);
    const float zeroY  = (float) slider.getPositionOfValue (0.0);

    // track
    g.setColour (colours::grid);
    g.fillRoundedRectangle (cx - trackW * 0.5f, bounds.getY(), trackW, bounds.getHeight(), trackW * 0.5f);

    // 0 dB mark
    g.setColour (colours::dimText);
    g.drawHorizontalLine ((int) zeroY, cx - bounds.getWidth() * 0.3f, cx + bounds.getWidth() * 0.3f);

    // fill from 0 dB
    const bool isBoost = slider.getValue() >= 0.0;
    g.setColour ((isBoost ? colours::boost : colours::cut).withAlpha (slider.isEnabled() ? 1.0f : 0.4f));
    const float top = juce::jmin (zeroY, sliderPos), bottom = juce::jmax (zeroY, sliderPos);
    g.fillRoundedRectangle (cx - trackW * 0.5f, top, trackW, bottom - top, trackW * 0.5f);

    // thumb
    const float thumbW = juce::jlimit (10.0f, 26.0f, bounds.getWidth() * 0.75f);
    const float thumbH = 8.0f;
    auto thumb = juce::Rectangle<float> (thumbW, thumbH).withCentre ({ cx, sliderPos });
    g.setColour (colours::text);
    g.fillRoundedRectangle (thumb, 2.0f);
    g.setColour (colours::background);
    g.drawHorizontalLine ((int) sliderPos, thumb.getX() + 2.0f, thumb.getRight() - 2.0f);
}

//==============================================================================
void ResponseCurve::setData (std::vector<juce::Point<float>> curveDb, std::vector<juce::Point<float>> bandPointsDb,
                             bool bypassed)
{
    curve      = std::move (curveDb);
    points     = std::move (bandPointsDb);
    isBypassed = bypassed;
    repaint();
}

float ResponseCurve::xForFreq (float f) const
{
    const float t = std::log (f / minFreq) / std::log (maxFreq / minFreq);
    return t * (float) getWidth();
}

float ResponseCurve::yForDb (float db) const
{
    const float h = (float) getHeight();
    return juce::jmap (juce::jlimit (-rangeDb, rangeDb, db), rangeDb, -rangeDb, 6.0f, h - 6.0f);
}

void ResponseCurve::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setFont (11.0f);

    // frequency grid
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (colours::grid);
        g.drawVerticalLine ((int) x, 0.0f, bounds.getHeight());
        g.setColour (colours::dimText);
        g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f) + "k" : juce::String ((int) f),
                    (int) x + 3, getHeight() - 16, 40, 14, juce::Justification::left);
    }

    // dB grid
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yForDb (db);
        g.setColour (db == 0.0f ? colours::dimText : colours::grid);
        g.drawHorizontalLine ((int) y, 0.0f, bounds.getWidth());
        g.setColour (colours::dimText);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), 4, (int) y - 13, 30, 12, juce::Justification::left);
    }

    if (curve.empty())
        return;

    juce::Path path;
    for (size_t i = 0; i < curve.size(); ++i)
    {
        const juce::Point<float> p { xForFreq (curve[i].x), yForDb (curve[i].y) };
        if (i == 0)
            path.startNewSubPath (p);
        else
            path.lineTo (p);
    }

    const auto lineColour = isBypassed ? colours::dimText : colours::accent;

    juce::Path fill (path);
    fill.lineTo (bounds.getRight(), yForDb (0.0f));
    fill.lineTo (bounds.getX(), yForDb (0.0f));
    fill.closeSubPath();
    g.setColour (lineColour.withAlpha (0.12f));
    g.fillPath (fill);

    g.setColour (lineColour);
    g.strokePath (path, juce::PathStrokeType (2.0f));

    // slider values
    g.setColour (colours::text.withAlpha (isBypassed ? 0.3f : 0.8f));
    for (auto& p : points)
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ xForFreq (p.x), yForDb (p.y) }));
}

//==============================================================================
GraphicEqEditor::GraphicEqEditor (GraphicEqProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    logo = juce::ImageCache::getFromMemory (MelonBranding::melonlogoondarkheader_png,
                                            MelonBranding::melonlogoondarkheader_pngSize);

    titleLabel.setText ("GraphicEQ", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, colours::text);
    addAndMakeVisible (titleLabel);

    modeBox.addItem ("10 bands", 1);
    modeBox.addItem ("31 bands", 2);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<ComboBoxAttachment> (processor.apvts, "mode", modeBox);

    preciseButton.setTooltip ("Compensates for the interaction between neighbouring bands so the curve follows the sliders");
    addAndMakeVisible (preciseButton);
    preciseAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "precise", preciseButton);

    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "bypass", bypassButton);

    resetButton.onClick = [this] { resetBands(); };
    addAndMakeVisible (resetButton);

    outputSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    outputSlider.setColour (juce::Slider::trackColourId, colours::accent);
    outputSlider.setColour (juce::Slider::backgroundColourId, colours::grid);
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (outputSlider);
    outputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "output", outputSlider);

    outputLabel.setText ("Output", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (outputLabel);

    addAndMakeVisible (curve);

    for (int n : { 10, 31 })
    {
        auto& list        = (n == 31 ? bands31 : bands10);
        const auto& freqs = geq::bandFrequencies (n);

        for (int i = 0; i < n; ++i)
        {
            Band b;
            b.slider = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
            b.slider->setDoubleClickReturnValue (true, 0.0);
            b.slider->setPopupDisplayEnabled (true, true, this);
            b.slider->setMouseDragSensitivity (300);
            addChildComponent (*b.slider);

            b.label = std::make_unique<juce::Label> ("", geq::frequencyLabel (freqs[(size_t) i]));
            b.label->setJustificationType (juce::Justification::centred);
            b.label->setFont (juce::FontOptions (n == 31 ? 10.0f : 13.0f));
            b.label->setColour (juce::Label::textColourId, colours::dimText);
            b.label->setMinimumHorizontalScale (0.7f);
            addChildComponent (*b.label);

            b.attachment = std::make_unique<SliderAttachment> (processor.apvts,
                                                               GraphicEqProcessor::bandParamId (n, i), *b.slider);
            list.push_back (std::move (b));
        }
    }

    setResizable (true, true);
    setResizeLimits (860, 450, 1800, 1000);
    setSize (1000, 550);

    updateVisibleBands();
    updateCurve();
    startTimerHz (30);
}

GraphicEqEditor::~GraphicEqEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GraphicEqEditor::paint (juce::Graphics& g)
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
}

void GraphicEqEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto header = area.removeFromTop (40);

    // logo keeps its aspect ratio (stored at 2x for high-DPI screens)
    const int logoW = logo.isValid() ? juce::roundToInt (40.0f * (float) logo.getWidth() / (float) logo.getHeight()) : 0;
    logoBounds = header.removeFromLeft (logoW);
    header.removeFromLeft (20);
    titleLabel.setBounds (header.removeFromLeft (110));
    header = header.reduced (0, 4);
    modeBox.setBounds (header.removeFromLeft (120).reduced (0, 2));
    header.removeFromLeft (12);
    preciseButton.setBounds (header.removeFromLeft (130));
    bypassButton.setBounds (header.removeFromLeft (90));
    resetButton.setBounds (header.removeFromLeft (70).reduced (0, 2));
    outputSlider.setBounds (header.removeFromRight (190));
    outputLabel.setBounds (header.removeFromRight (50));

    area.removeFromTop (10);
    curve.setBounds (area.removeFromTop (juce::jmax (120, area.getHeight() * 2 / 5)));
    area.removeFromTop (10);

    auto& list = (shownBands == 31 ? bands31 : bands10);
    if (list.empty())
        return;

    auto labels      = area.removeFromBottom (18);
    const float colW = (float) area.getWidth() / (float) list.size();

    for (size_t i = 0; i < list.size(); ++i)
    {
        const int x0 = area.getX() + juce::roundToInt (colW * (float) i);
        const int x1 = area.getX() + juce::roundToInt (colW * (float) (i + 1));
        list[i].slider->setBounds (x0, area.getY(), x1 - x0, area.getHeight());
        list[i].label->setBounds (x0 - 2, labels.getY(), x1 - x0 + 4, labels.getHeight());
    }
}

void GraphicEqEditor::updateVisibleBands()
{
    const int n = processor.currentNumBands();
    if (n == shownBands)
        return;

    shownBands = n;
    for (auto& b : bands10)
    {
        b.slider->setVisible (n == 10);
        b.label->setVisible (n == 10);
    }
    for (auto& b : bands31)
    {
        b.slider->setVisible (n == 31);
        b.label->setVisible (n == 31);
    }
    resized();
}

void GraphicEqEditor::updateCurve()
{
    const int    n       = processor.currentNumBands();
    const double fs      = processor.currentSampleRate();
    const bool   precise = processor.isPrecise();
    const bool   bypass  = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    double sliders[geq::kMaxBands] {};
    processor.readSliderValues (n, sliders);

    // only recompute when something changed
    std::vector<double> snapshot (sliders, sliders + n);
    snapshot.push_back (n);
    snapshot.push_back (fs);
    snapshot.push_back (precise ? 1.0 : 0.0);
    snapshot.push_back (bypass ? 1.0 : 0.0);
    if (snapshot == lastSnapshot)
        return;
    lastSnapshot = snapshot;

    if (designBands != n || designFs != fs)
    {
        design.configure (n, fs);
        designBands = n;
        designFs    = fs;
    }

    double gains[geq::kMaxBands] {};
    design.filterGains (sliders, precise, gains);

    std::vector<juce::Point<float>> curvePts, bandPts;
    constexpr int numPoints = 400;
    const double  maxF      = juce::jmin ((double) ResponseCurve::maxFreq, fs * 0.49);
    for (int i = 0; i < numPoints; ++i)
    {
        const double f = ResponseCurve::minFreq * std::pow (maxF / ResponseCurve::minFreq, i / (double) (numPoints - 1));
        curvePts.push_back ({ (float) f, (float) (bypass ? 0.0 : design.responseDb (gains, f)) });
    }
    for (int i = 0; i < n; ++i)
        bandPts.push_back ({ (float) design.frequency (i), (float) sliders[i] });

    curve.setData (std::move (curvePts), std::move (bandPts), bypass);
}

void GraphicEqEditor::resetBands()
{
    const int n = processor.currentNumBands();
    for (int i = 0; i < n; ++i)
    {
        if (auto* param = processor.apvts.getParameter (GraphicEqProcessor::bandParamId (n, i)))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
            param->endChangeGesture();
        }
    }
}

void GraphicEqEditor::timerCallback()
{
    updateVisibleBands();
    updateCurve();
}
