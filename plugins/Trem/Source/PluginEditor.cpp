#include "PluginEditor.h"

using namespace melon;

namespace
{
const juce::Colour ledRed { 0xfff85149 }; // the pedal's red Rate LED
} // namespace

void TremDisplay::setState (const trem::Params& p, double ph, float gl, float gr, double hostBpm, bool isSynced,
                            const juce::String& name)
{
    params = p; phase = ph; gainL = gl; gainR = gr; bpm = hostBpm; synced = isSynced; syncName = name;
    repaint();
}

void TremDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto left   = bounds.removeFromLeft (juce::jmin (230.0f, bounds.getWidth() * 0.36f));
    bounds.removeFromLeft (10.0f);

    // --- rate LED and readout -------------------------------------------------
    {
        auto area    = drawDisplayPanel (g, left, "Rate");
        auto ledArea = area.removeFromLeft (juce::jmin (70.0f, area.getHeight()));
        const float d   = juce::jmin (ledArea.getWidth(), ledArea.getHeight()) * 0.55f;
        const auto  led = juce::Rectangle<float> (d, d).withCentre (ledArea.getCentre());

        // brightness follows the volume, like the pedal's LED
        const float level = params.bypassed ? 0.0f : juce::jlimit (0.0f, 1.0f, juce::jmax (gainL, gainR));
        const auto  on    = ledRed.withMultipliedBrightness (0.35f + 0.65f * level);
        if (level > 0.05f)
        {
            g.setColour (ledRed.withAlpha (0.25f * level));
            g.fillEllipse (led.expanded (d * 0.3f * level));
        }
        g.setGradientFill (juce::ColourGradient (on.brighter (0.5f * level), led.getCentreX() - d * 0.2f, led.getCentreY() - d * 0.25f,
                                                 on.darker (0.6f), led.getRight(), led.getBottom(), true));
        g.fillEllipse (led);
        g.setColour (colours::background);
        g.drawEllipse (led, 2.0f);

        area.removeFromLeft (8.0f);
        const double hz = params.rateHz;
        juce::String sub;
        if (params.bypassed)  sub << "BYPASSED   ";
        else if (synced)      sub << "SYNC " << syncName << "   ";
        if (bpm > 0.0)        sub << juce::String (bpm, 1) << " BPM";

        auto text = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 60.0f));
        g.setColour (colours::accent);
        g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
        g.drawText (juce::String (hz, hz >= 10.0 ? 1 : 2) + " Hz", text.removeFromTop (36.0f).toNearestInt(), juce::Justification::left);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (colours::dimText);
        g.drawText (sub, text.toNearestInt(), juce::Justification::left);
    }

    // --- volume curve ---------------------------------------------------------
    {
        auto area = drawDisplayPanel (g, bounds, "Wave");
        auto info = area.removeFromBottom (16.0f);
        auto plot = area.reduced (2.0f, 4.0f);

        g.setColour (colours::grid);
        for (float f : { 0.0f, 0.5f, 1.0f })
            g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * f), plot.getX(), plot.getRight());
        g.drawVerticalLine ((int) plot.getCentreX(), plot.getY(), plot.getBottom());

        auto curve = [&] (double offset) {
            juce::Path p;
            const int steps = juce::jmax (2, (int) plot.getWidth());
            for (int i = 0; i <= steps; ++i)
            {
                const double cyc = 2.0 * i / steps; // two cycles
                const double v   = trem::tremGain (trem::lfoShape (cyc + offset, params.wave), params.depth);
                const float  x   = plot.getX() + plot.getWidth() * (float) i / (float) steps;
                const float  y   = plot.getBottom() - plot.getHeight() * (float) v;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };

        const float alpha = params.bypassed ? 0.35f : 1.0f;
        if (params.stereo == trem::pan)
        {
            g.setColour (colours::second.withAlpha (alpha));
            g.strokePath (curve (0.5), juce::PathStrokeType (2.0f));
        }
        g.setColour (colours::accent.withAlpha (alpha));
        g.strokePath (curve (0.0), juce::PathStrokeType (2.0f));

        // playhead (in the first of the two cycles shown)
        const float xp = plot.getX() + plot.getWidth() * 0.5f * (float) (phase - std::floor (phase));
        g.setColour (colours::text.withAlpha (0.6f * alpha));
        g.fillRect (xp - 1.0f, plot.getY(), 2.0f, plot.getHeight());

        const int wavePct  = juce::roundToInt (params.wave * 100.0);
        const juce::String shape = wavePct <= 15 ? "Triangle" : wavePct >= 85 ? "Square" : "Triangle / Square";
        g.setFont (11.0f);
        g.setColour (colours::dimText);
        g.drawText (shape + "  |  Depth " + juce::String (juce::roundToInt (params.depth * 100.0)) + " %",
                    info.toNearestInt(), juce::Justification::left);
        g.drawText (params.stereo == trem::pan ? "Stereo: Pan (L blue, R orange)" : "Stereo: Mono",
                    info.toNearestInt(), juce::Justification::right);
    }
}

//==============================================================================
TremEditor::TremEditor (TremProcessor& p) : EditorBase (p, "Trem")
{
    setDisplay (display);

    auto& tapButton = makeButton ("TAP", "Tap the tempo: click 2 or more times in rhythm. Sets Rate (one cycle per tap) and turns Sync off");
    tapButton.onClick = [this] { processor.tap(); };
    addRowItem (tapButton, 70);
    addRowLabel ("Sync", 40);
    addChoiceBox ("sync", 100, false, "Sync the tremolo to the host tempo (Off = use the Rate knob). The value is the length of one cycle");
    addRowLabel ("Stereo", 60);
    addChoiceBox ("stereo", 100, false, "Mono: both channels pulse together, like the pedal. Pan: the right channel pulses in anti-phase (auto-pan)");

    knobsPerRow = 3;
    rateKnob = &addKnob ("rate", "Rate", "Speed of the tremolo, 0.3 to 15 Hz (ignored while Sync is on)");
    addKnob ("wave", "Wave", "Shape of the tremolo: from a smooth triangle (left) to a choppy square (right)");
    addKnob ("depth", "Depth", "How much the volume drops: from a gentle shimmer to a full on/off chop");

    finishSetup (640, 400, 560, 380);
}

void TremEditor::timerCallback()
{
    const bool synced = processor.isSynced();
    rateKnob->slider.setEnabled (! synced);

    const auto& sv   = trem::syncValues();
    const int   sync = juce::jlimit (0, (int) sv.size() - 1, processor.choice ("sync"));
    auto&       m    = processor.meters();
    display.setState (processor.currentParams(), m.phase.load(), m.gainL.load(), m.gainR.load(),
                      processor.hostBpm(), synced, sv[(size_t) sync].first);
}
