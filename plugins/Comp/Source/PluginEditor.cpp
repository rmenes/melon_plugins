#include "PluginEditor.h"

using namespace melon;

void CompDisplay::pull (const comp::MeterQueue& q, const comp::Params& p)
{
    params = p;
    float latestGr = 0.0f;
    bool  any      = false;
    q.drain (readIndex, [&] (const comp::MeterPoint& m) {
        history[(size_t) head] = m;
        head     = (head + 1) % kHistory;
        latestGr = any ? std::min (latestGr, m.grDb) : m.grDb;
        any      = true;
    });
    if (any)
        grBar = latestGr < grBar ? latestGr : grBar + 0.25f * (latestGr - grBar); // fast down, eased return
    repaint();
}

void CompDisplay::paint (juce::Graphics& g)
{
    auto plot = drawDisplayPanel (g, getLocalBounds().toFloat(), "Compression").withTrimmedLeft (30.0f);

    // gain-reduction bar on the right
    auto bar = plot.removeFromRight (34.0f);
    plot.removeFromRight (14.0f);

    constexpr float dbLo = -72.0f, dbHi = 0.0f;
    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (dbLo, dbHi, db), dbLo, dbHi, plot.getBottom(), plot.getY()); };

    g.setFont (11.0f);
    for (float db : { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
    {
        const float y = yForDb (db);
        g.setColour (colours::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (colours::dimText);
        g.drawText (juce::String ((int) db), (int) plot.getX() - 32, (int) y - 7, 28, 14, juce::Justification::right);
    }

    const float dx = plot.getWidth() / (float) (kHistory - 1);
    juce::Path inFill, outLine, grLine;
    inFill.startNewSubPath (plot.getX(), plot.getBottom());
    for (int i = 0; i < kHistory; ++i)
    {
        const auto& m = history[(size_t) ((head + i) % kHistory)];
        const float x = plot.getX() + dx * (float) i;
        inFill.lineTo (x, yForDb (m.inDb));
        if (i == 0)
        {
            outLine.startNewSubPath (x, yForDb (m.outDb));
            grLine.startNewSubPath (x, yForDb (m.grDb));
        }
        else
        {
            outLine.lineTo (x, yForDb (m.outDb));
            grLine.lineTo (x, yForDb (m.grDb));
        }
    }
    inFill.lineTo (plot.getRight(), plot.getBottom());
    inFill.closeSubPath();

    g.setColour (colours::dimText.withAlpha (0.25f));
    g.fillPath (inFill);
    g.setColour (colours::accent);
    g.strokePath (outLine, juce::PathStrokeType (1.5f));
    g.setColour (colours::second);
    g.strokePath (grLine, juce::PathStrokeType (2.0f));

    // GR bar: 0 dB at the top, 30 dB at the bottom
    constexpr float grRange = 30.0f;
    auto meter = bar.withTrimmedBottom (16.0f);
    g.setColour (colours::background);
    g.fillRoundedRectangle (meter, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, -grBar / grRange);
    g.setColour (colours::second);
    g.fillRoundedRectangle (meter.withHeight (meter.getHeight() * frac), 3.0f);
    g.setColour (colours::grid);
    for (float db : { 10.0f, 20.0f })
        g.drawHorizontalLine ((int) (meter.getY() + meter.getHeight() * db / grRange), meter.getX(), meter.getRight());
    g.setColour (colours::dimText);
    g.drawText ("GR", bar.withTop (meter.getBottom()).toNearestInt(), juce::Justification::centred);

    const auto info = params.bypassed ? juce::String ("Bypassed")
                      : juce::String (params.pickup == comp::Pickup::humbucker ? "Humbucker" : "Single Coil")
                            + "  |  GR " + juce::String (grBar, 1) + " dB";
    g.setColour (colours::dimText);
    g.drawText (info, getLocalBounds().reduced (12, 4).withHeight (14), juce::Justification::right);
}

//==============================================================================
CompEditor::CompEditor (CompProcessor& p) : EditorBase (p, "Comp")
{
    knobsPerRow = 4;
    setDisplay (display);
    addRowLabel ("Release", 70);
    addChoiceBox ("pickup", 150, false,
                  "Single Coil: slower release for a long, smooth sustain. "
                  "Humbucker: quicker attack and release, so hotter pickups keep their note definition");

    addKnob ("sustain", "Sustain", "Amount of compression and sustain. Higher settings squeeze the peaks and lift fading notes");
    addKnob ("level", "Level", "Output volume of the pedal (noon = unity for the dry signal)");
    addKnob ("blend", "Blend", "Mix of dry (in phase) and compressed signal. Lower it to let the pick attack through");
    addKnob ("tone", "Tone", "Treble emphasis on the compressed signal: darker to the left, brighter to the right, flat at noon");

    finishSetup (680, 520, 600, 480);
}

void CompEditor::timerCallback()
{
    display.pull (processor.meters(), processor.currentParams());
}
