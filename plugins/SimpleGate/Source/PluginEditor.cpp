#include "PluginEditor.h"

using namespace melon;

namespace
{
const juce::Colour ledOpen   { 0xff3fb950 }; // green
const juce::Colour ledClosed { 0xfff85149 }; // red
constexpr float kMeterLo = -90.0f, kMeterHi = 0.0f;
} // namespace

void SimpleGateDisplay::pull (sgate::Meters& m, const sgate::Params& p)
{
    params = p;
    const float in = m.takeInput();
    inputDb = in > inputDb ? in : juce::jmax (in, inputDb - 1.5f); // fast up, ~45 dB/s fall
    if (in >= peakDb || ++peakAge > 45)
    {
        peakDb  = in;
        peakAge = 0;
    }
    grDb = m.grDb.load();
    open = m.open.load();
    repaint();
}

void SimpleGateDisplay::paint (juce::Graphics& g)
{
    auto area = drawDisplayPanel (g, getLocalBounds().toFloat(), "Gate");

    // --- LED ---
    auto ledArea = area.removeFromLeft (juce::jmin (110.0f, area.getHeight() + 20.0f));
    const float d = juce::jmin (ledArea.getWidth(), ledArea.getHeight() - 18.0f) * 0.62f;
    const auto  led = juce::Rectangle<float> (d, d).withCentre ({ ledArea.getCentreX(), ledArea.getY() + (ledArea.getHeight() - 18.0f) * 0.5f });
    const auto  colour = params.bypassed ? colours::grid : (open ? ledOpen : ledClosed);
    if (! params.bypassed)
    {
        g.setColour (colour.withAlpha (0.18f));
        g.fillEllipse (led.expanded (d * 0.22f));
    }
    g.setGradientFill (juce::ColourGradient (colour.brighter (0.6f), led.getCentreX() - d * 0.2f, led.getCentreY() - d * 0.25f,
                                             colour.darker (0.5f), led.getRight(), led.getBottom(), true));
    g.fillEllipse (led);
    g.setColour (colours::background);
    g.drawEllipse (led, 2.0f);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (params.bypassed ? colours::dimText : colour);
    g.drawText (params.bypassed ? "BYPASSED" : (open ? "OPEN" : "CLOSED"),
                ledArea.removeFromBottom (16.0f).toNearestInt(), juce::Justification::centred);

    area.removeFromLeft (16.0f);

    // --- input meter with threshold ---
    auto info  = area.removeFromBottom (18.0f);
    auto scale = area.removeFromBottom (14.0f);
    auto meter = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (26.0f, area.getHeight())).reduced (0.0f, 1.0f);
    auto xForDb = [&] (float db) { return juce::jmap (juce::jlimit (kMeterLo, kMeterHi, db), kMeterLo, kMeterHi, meter.getX(), meter.getRight()); };

    g.setColour (colours::background);
    g.fillRoundedRectangle (meter, 3.0f);
    const float thr = (float) params.thresholdDb;
    const float xIn = xForDb (inputDb), xThr = xForDb (thr);
    // below the threshold: grey (what gets gated); above: blue (what gets through)
    g.setColour (colours::dimText.withAlpha (0.55f));
    g.fillRect (meter.withRight (juce::jmin (xIn, xThr)));
    if (xIn > xThr)
    {
        g.setColour (colours::accent);
        g.fillRect (meter.withLeft (xThr).withRight (xIn));
    }
    if (peakDb > kMeterLo)
    {
        g.setColour (colours::text.withAlpha (0.7f));
        g.fillRect (xForDb (peakDb) - 1.0f, meter.getY(), 2.0f, meter.getHeight());
    }
    // threshold marker (and where it closes, with the hysteresis)
    g.setColour (colours::second);
    g.fillRect (xThr - 1.0f, meter.getY() - 5.0f, 2.0f, meter.getHeight() + 10.0f);
    g.setColour (colours::second.withAlpha (0.35f));
    g.fillRect (xForDb (thr - (float) sgate::kHysteresisDb), meter.getY(), xThr - xForDb (thr - (float) sgate::kHysteresisDb), meter.getHeight());

    g.setFont (11.0f);
    g.setColour (colours::dimText);
    for (float db : { -90.0f, -72.0f, -54.0f, -36.0f, -18.0f, 0.0f })
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (40.0f, 14.0f).withCentre ({ xForDb (db), scale.getCentreY() })
                                                 .constrainedWithin (scale).toNearestInt(), juce::Justification::centred);

    const auto voicing = params.instrument == sgate::Instrument::bass ? juce::String ("Bass") : juce::String ("Guitar");
    g.setColour (colours::dimText);
    g.drawText ("Input " + (inputDb <= -119.0f ? juce::String ("-inf") : juce::String (inputDb, 1)) + " dB", info.toNearestInt(),
                juce::Justification::left);
    g.drawText (voicing + "  |  Threshold " + juce::String (thr, 1) + " dB  |  GR " + juce::String (juce::jmax (grDb, -90.0f), 1) + " dB",
                info.toNearestInt(), juce::Justification::right);
}

//==============================================================================
SimpleGateEditor::SimpleGateEditor (SimpleGateProcessor& p) : EditorBase (p, "Simple Gate")
{
    titleWidth = 150;
    setDisplay (display);
    addRowLabel ("Instrument", 80);
    addChoiceBox ("instrument", 130, false,
                  "Guitar: the detector ignores hum and hiss and follows notes down to a 7-string low B. "
                  "Bass: the detector follows notes down to a 5-string low B without chattering");

    addKnob ("threshold", "Threshold", "Level where the gate opens. Turn it up until the noise between notes disappears");
    addKnob ("hold", "Hold", "Time the gate stays open after you stop playing (bridges short pauses)");
    addKnob ("release", "Release", "Fade-out time once the hold is over: short = tight and percussive, long = natural decay");

    finishSetup (640, 400, 560, 380);
}

void SimpleGateEditor::timerCallback()
{
    display.pull (processor.meters(), processor.currentParams());
}
