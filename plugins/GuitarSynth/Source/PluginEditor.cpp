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
GuitarSynthLookAndFeel::GuitarSynthLookAndFeel()
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

int GuitarSynthLookAndFeel::getSliderThumbRadius (juce::Slider& s)
{
    return s.isVertical() ? 11 : LookAndFeel_V4::getSliderThumbRadius (s); // half the fader cap's height
}

// Vertical faders in the style of the pedal's sliders; anything else uses the standard drawing
void GuitarSynthLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                               float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                                               juce::Slider& slider)
{
    if (! slider.isVertical())
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto  bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float cx     = bounds.getCentreX();
    const float top    = (float) y + (float) getSliderThumbRadius (slider);
    const float bottom = (float) (y + height) - (float) getSliderThumbRadius (slider);
    const float alpha  = slider.isEnabled() ? 1.0f : 0.4f;

    // scale: a tick every 10 %, longer at 0, 50 and 100 %
    g.setColour (colours::grid);
    for (int i = 0; i <= 10; ++i)
    {
        const float ty  = juce::jmap ((float) i, 0.0f, 10.0f, bottom, top);
        const float len = (i % 5 == 0) ? 9.0f : 5.0f;
        g.fillRect (cx - 13.0f - len, ty - 0.5f, len, 1.0f);
        g.fillRect (cx + 13.0f, ty - 0.5f, len, 1.0f);
    }

    // groove, filled from the bottom up to the cap
    const auto groove = juce::Rectangle<float> (cx - 3.0f, top - 4.0f, 6.0f, bottom - top + 8.0f);
    g.setColour (colours::background);
    g.fillRoundedRectangle (groove, 3.0f);
    g.setColour (colours::accent.withAlpha (alpha));
    g.fillRoundedRectangle (groove.withTop (sliderPos), 3.0f);

    // fader cap
    const float capW = juce::jmin (36.0f, bounds.getWidth() - 10.0f), capH = 22.0f;
    const auto  cap  = juce::Rectangle<float> (capW, capH).withCentre ({ cx, sliderPos });
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (colours::grid.brighter (0.35f), cap.getX(), cap.getY(),
                                             colours::panel.brighter (0.05f), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (colours::background.withAlpha (0.8f));
    g.drawRoundedRectangle (cap, 4.0f, 1.2f);
    g.setColour ((slider.isMouseOverOrDragging() ? colours::accent : colours::text).withAlpha (alpha));
    g.fillRect (cap.getX() + 5.0f, sliderPos - 1.0f, cap.getWidth() - 10.0f, 2.0f);
}

//==============================================================================
static juce::String hzLabel (double hz)
{
    return hz >= 1000.0 ? juce::String (hz / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
}

static juce::String noteName (double hz)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int midi = juce::roundToInt (69.0 + 12.0 * std::log2 (hz / 440.0));
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

void FilterDisplay::setState (const State& newState)
{
    s = newState;
    repaint();
}

void FilterDisplay::paint (juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const auto& p      = s.params;
    const double fs    = s.sampleRate > 0.0 ? s.sampleRate : 48000.0;
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    auto inner = bounds.reduced (12.0f, 22.0f);

    // --- input meter with the Trigger threshold ---------------------------------------
    constexpr double meterLo = -66.0, meterHi = 0.0;
    const auto meter = inner.removeFromLeft (12.0f).withTrimmedBottom (-18.0f).withTrimmedTop (2.0f);
    auto yForMeterDb = [&] (double db) {
        return juce::jmap ((float) juce::jlimit (meterLo, meterHi, db), (float) meterLo, (float) meterHi, meter.getBottom(), meter.getY());
    };
    g.setColour (colours::background);
    g.fillRoundedRectangle (meter, 3.0f);
    const double levelDb = 20.0 * std::log10 (s.level + 1e-9);
    if (levelDb > meterLo)
    {
        g.setColour (colours::accent.withAlpha (p.bypassed ? 0.35f : 0.85f));
        g.fillRoundedRectangle (meter.withTop (yForMeterDb (levelDb)), 3.0f);
    }
    const double thrDb = 20.0 * std::log10 (gsynth::triggerThreshold (p.trigger));
    const float  thrY  = yForMeterDb (thrDb);
    g.setColour (colours::second);
    g.fillRect (meter.getX() - 3.0f, thrY - 1.0f, meter.getWidth() + 6.0f, 2.0f);
    g.setFont (10.0f);
    g.setColour (colours::dimText);
    g.drawText ("IN", juce::Rectangle<float> (meter.getX() - 8.0f, meter.getBottom() + 2.0f, meter.getWidth() + 16.0f, 12.0f),
                juce::Justification::centred);

    inner.removeFromLeft (14.0f);
    const auto plot = inner.withTrimmedLeft (30.0f);

    constexpr double fLo = 20.0, fHi = 20000.0, dbLo = -36.0, dbHi = 24.0;
    const double     fMax = juce::jmin (fHi, 0.49 * fs);
    auto xForHz = [&] (double f) {
        return plot.getX() + plot.getWidth() * (float) (std::log (f / fLo) / std::log (fHi / fLo));
    };
    auto yForDb = [&] (double db) {
        return juce::jmap ((float) juce::jlimit (dbLo, dbHi, db), (float) dbLo, (float) dbHi, plot.getBottom(), plot.getY());
    };

    // range the sweep covers
    {
        const float x0 = xForHz (juce::jmin (p.startHz, p.stopHz)), x1 = xForHz (juce::jmax (p.startHz, p.stopHz));
        g.setColour (colours::accent.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (x0, plot.getY(), juce::jmax (2.0f, x1 - x0), plot.getHeight()));
    }

    // grid
    g.setFont (11.0f);
    for (double db : { -24.0, -12.0, 0.0, 12.0 })
    {
        const float y = yForDb (db);
        g.setColour (juce::exactlyEqual (db, 0.0) ? colours::grid.brighter (0.2f) : colours::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (colours::dimText);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) inner.getX(), (int) y - 7, 28, 14, juce::Justification::left);
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

    // Start / Stop markers
    auto marker = [&] (double hz, const char* name, bool above) {
        const float x = xForHz (hz);
        g.setColour (colours::dimText.withAlpha (0.8f));
        for (float y = plot.getY(); y < plot.getBottom(); y += 6.0f)
            g.drawVerticalLine ((int) x, y, juce::jmin (y + 3.0f, plot.getBottom()));
        g.setFont (10.0f);
        g.drawText (name, juce::Rectangle<float> (x + 3.0f, above ? plot.getY() : plot.getBottom() - 14.0f, 40.0f, 14.0f),
                    juce::Justification::left);
    };
    marker (p.startHz, "Start", true);
    marker (p.stopHz, "Stop", false);

    // header: title, trigger LED, readout
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (colours::accent);
    g.drawText ("Filter", (int) plot.getX(), (int) bounds.getY() + 4, 50, 14, juce::Justification::left);
    const auto led = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ plot.getX() + 56.0f, bounds.getY() + 11.0f });
    g.setColour (colours::grid);
    g.fillEllipse (led);
    if (s.ledGlow > 0.01f)
    {
        g.setColour (colours::second.withAlpha (s.ledGlow));
        g.fillEllipse (led);
        g.setColour (colours::second.withAlpha (0.25f * s.ledGlow));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setFont (11.0f);
    g.setColour (colours::dimText);
    g.drawText ("Trigger", juce::Rectangle<float> (led.getRight() + 5.0f, bounds.getY() + 4.0f, 60.0f, 14.0f), juce::Justification::left);

    const bool   sweeping = std::abs (std::log (p.startHz / p.stopHz)) > 0.01;
    const bool   hasNote  = s.pitchHz > 0.0 && 20.0 * std::log10 (s.level + 1e-9) > gsynth::kGateDb + 6.0;
    juce::String readout  = p.bypassed ? juce::String ("Bypassed")
                          : (sweeping ? "Sweep " + hzLabel (p.startHz) + " > " + hzLabel (p.stopHz) + " in "
                                            + juce::String (juce::roundToInt (1000.0 * gsynth::sweepSeconds (p.rate))) + " ms"
                                      : "Fixed at " + hzLabel (p.stopHz))
                                + "   |   now " + hzLabel (s.cutoffHz) + "   |   note " + (hasNote ? noteName (s.pitchHz) : juce::String ("-"));
    g.drawText (readout, plot.withY (bounds.getY() + 4).withHeight (14).toNearestInt(), juce::Justification::right);

    // response curve at the current cutoff
    const int numPoints = juce::jmax (200, (int) plot.getWidth());
    juce::Path curve, fill;
    bool       started = false;
    for (int i = 0; i < numPoints; ++i)
    {
        const double f = fLo * std::pow (fHi / fLo, (double) i / (numPoints - 1));
        if (f > fMax)
            break;
        const double db = p.bypassed ? 0.0 : gsynth::responseDb (f, fs, s.cutoffHz, p.resonance) + p.outputDb;
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

    // current cutoff
    if (! p.bypassed && s.cutoffHz >= fLo && s.cutoffHz <= fMax)
    {
        g.setColour (colours::second);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ xForHz (s.cutoffHz), plot.getBottom() }));
    }
}

//==============================================================================
GuitarSynthEditor::GuitarSynthEditor (GuitarSynthProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    logo = juce::ImageCache::getFromMemory (MelonBranding::melonlogoondarkheader_png,
                                            MelonBranding::melonlogoondarkheader_pngSize);

    titleLabel.setText ("Guitar Synth", juce::dontSendNotification);
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

    addAndMakeVisible (display);

    setUpFader (trigger, "trigger", "Trigger", 50.0,
               "How loud a note must be to restart the filter sweep (orange line on the input meter). "
               "Too high and the sweep can stutter on chords; set it just sensitive enough for your playing");
    setUpFader (subOctave, "suboctave", "Sub\nOctave", 40.0, "Level of a synth tone one octave below the note you play");
    setUpFader (guitar, "guitar", "Guitar", 30.0, "Level of your guitar signal");
    setUpFader (octave, "octave", "Octave", 0.0, "Level of a tone one octave above the note you play");
    setUpFader (square, "square", "Square\nWave", 60.0,
               "Level of a square wave at the note you play: the fat, buzzy synth lead. On its own it is also a fuzz");
    setUpFader (attack, "attack", "Attack\nDelay", 0.0,
               "Fades every note in, removing the pick attack. Short = soft attack; long = bowed or backwards-sounding swells");
    setUpFader (resonance, "resonance", "Resonance", 60.0,
               "Sharpness of the filter peak. High settings give the vocal, \"wow\" synth sweep");
    setUpFader (start, "start", "Start\nFreq", 3000.0,
               "Where the filter sweep begins when a note is triggered. Start above Stop = the classic closing synth pluck; below = an opening swell");
    setUpFader (stop, "stop", "Stop\nFreq", 300.0,
               "Where the sweep ends, and where the filter rests between notes. Set Start = Stop for a fixed filter");
    setUpFader (rate, "rate", "Rate", 50.0, "Speed of the sweep from Start to Stop (shown as the sweep time)");
    setUpFader (mix, "mix", "Mix", 100.0,
                "Dry/wet balance. 100 % = synth only, like the pedal (blend your guitar in with the Guitar fader); "
                "lower it to add the dry signal back in parallel");

    setResizable (true, true);
    setResizeLimits (820, 580, 1600, 1100);
    setSize (940, 640);

    timerCallback();
    startTimerHz (30);
}

GuitarSynthEditor::~GuitarSynthEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GuitarSynthEditor::setUpFader (Fader& k, const juce::String& paramId, const juce::String& name, double defaultValue,
                                    const juce::String& tooltip)
{
    k.slider.setSliderStyle (juce::Slider::LinearVertical);
    // set directly on the slider: the value box is created before the editor's look-and-feel reaches it
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, colours::grid);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::background.withAlpha (0.6f));
    k.slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    k.slider.setDoubleClickReturnValue (true, defaultValue);
    k.slider.setTooltip (tooltip);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    k.label.setJustificationType (juce::Justification::centredBottom); // one- and two-line names line up
    k.label.setColour (juce::Label::textColourId, colours::text);
    k.label.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
}

void GuitarSynthEditor::paint (juce::Graphics& g)
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
    g.fillRoundedRectangle (faderPanel.toFloat(), 6.0f);

    // section titles with a rule under them, like the printing on the pedal
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (auto [r, name] : { std::pair { voiceCaption, "VOICE MIX" }, std::pair { filterCaption, "FILTER SWEEP" } })
    {
        g.setColour (colours::dimText);
        g.drawText (name, r, juce::Justification::centredTop);
        g.setColour (colours::grid);
        g.fillRect (r.getX() + 6, r.getBottom() - 3, r.getWidth() - 12, 1);
    }

    // lines between the sections: Trigger | voices | Attack Delay | filter sweep | Mix
    g.setColour (colours::grid);
    for (int x : dividers)
        g.fillRect (x, faderPanel.getY() + 14, 1, faderPanel.getHeight() - 28);
}

void GuitarSynthEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto header = area.removeFromTop (40);

    // logo keeps its aspect ratio (stored at 2x for high-DPI screens)
    const int logoW = logo.isValid() ? juce::roundToInt (40.0f * (float) logo.getWidth() / (float) logo.getHeight()) : 0;
    logoBounds = header.removeFromLeft (logoW);
    header.removeFromLeft (20);
    titleLabel.setBounds (header.removeFromLeft (140));
    header = header.reduced (0, 4);
    bypassButton.setBounds (header.removeFromLeft (90));
    outputSlider.setBounds (header.removeFromRight (190));
    outputLabel.setBounds (header.removeFromRight (50));

    area.removeFromTop (10);
    faderPanel = area.removeFromBottom (juce::jmax (300, area.getHeight() * 11 / 20));
    area.removeFromBottom (10);
    display.setBounds (area);

    Fader* const faders[] = { &trigger, &subOctave, &guitar, &octave, &square, &attack,
                              &resonance, &start, &stop, &rate, &mix };
    constexpr int numFaders = 11;

    auto        row  = faderPanel.reduced (8, 10);
    auto        caps = row.removeFromTop (18);
    const float colW = (float) row.getWidth() / (float) numFaders;
    auto colX = [&] (int i) { return row.getX() + juce::roundToInt (colW * (float) i); };

    for (int i = 0; i < numFaders; ++i)
    {
        auto col = juce::Rectangle<int> (colX (i), row.getY(), colX (i + 1) - colX (i), row.getHeight()).reduced (3, 0);
        faders[i]->label.setBounds (col.removeFromTop (34));
        col.removeFromTop (4);
        faders[i]->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (84, col.getWidth() - 2), 20);
        faders[i]->slider.setBounds (col);
    }

    voiceCaption  = caps.withX (colX (1)).withRight (colX (5));
    filterCaption = caps.withX (colX (6)).withRight (colX (10));
    dividers      = { colX (1), colX (5), colX (6), colX (10) };
}

void GuitarSynthEditor::timerCallback()
{
    const int t = processor.triggerCount();
    if (t != lastTriggers)
    {
        lastTriggers = t;
        ledGlow      = 1.0f;
    }
    else
    {
        ledGlow *= 0.82f;
    }

    FilterDisplay::State st;
    st.params     = processor.currentParams();
    st.cutoffHz   = processor.currentCutoffHz();
    st.level      = processor.currentLevel();
    st.pitchHz    = processor.currentPitchHz();
    st.sampleRate = processor.sampleRateForDisplay();
    st.ledGlow    = ledGlow;
    display.setState (st);
}
