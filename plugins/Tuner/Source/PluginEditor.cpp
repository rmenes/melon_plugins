#include "PluginEditor.h"

using namespace melon;

namespace
{
constexpr double kHoldSeconds  = 1.5;  // last note stays (dimmed) this long after the signal stops
constexpr double kTickSeconds  = 0.4;  // a string held in tune this long gets its tick
constexpr float  kMeterDegrees = 60.0f; // needle swing for ±50 cents

juce::String centsText (double c)
{
    return (c > 0.05 ? "+" : "") + juce::String (c, 1) + " cents";
}

// Triangle pointing towards the centre: the flat light (left) points right, the sharp light points left.
void drawLight (juce::Graphics& g, juce::Rectangle<float> r, bool pointsRight, juce::Colour c)
{
    juce::Path p;
    if (pointsRight)
        p.addTriangle (r.getX(), r.getY(), r.getRight(), r.getCentreY(), r.getX(), r.getBottom());
    else
        p.addTriangle (r.getRight(), r.getY(), r.getX(), r.getCentreY(), r.getRight(), r.getBottom());
    g.setColour (c);
    g.fillPath (p);
}
} // namespace

void TunerDisplay::update (const tuner::Reading& r, const tuner::Params& p, double dt)
{
    params = p;
    const auto& tun = tuner::tunings()[(size_t) juce::jlimit (0, (int) tuner::tunings().size() - 1, p.tuning)];
    if (p.tuning != lastTuning)
    {
        lastTuning = p.tuning;
        tuned.assign (tun.strings.size(), false);
        inTuneTime = 0.0;
    }

    const double f = r.frequency.load();
    live = f > 0.0;
    if (live)
    {
        const auto t   = tuner::findTarget (f, p.reference, p.tuning);
        const bool jump = ! hasNote || t.midi != target.midi || t.string != target.string || sinceSeen > 0.3;
        // Smoothing: 0 % follows every reading, 100 % settles over about half a second
        const double tau   = 0.01 + 0.45 * p.smoothing;
        const double alpha = 1.0 - std::exp (-dt / tau);
        cents     = jump ? t.cents : cents + alpha * (t.cents - cents);
        frequency = jump ? f : frequency + alpha * (f - frequency);
        target    = t;
        hasNote   = true;
        sinceSeen = 0.0;

        if (std::abs (cents) <= kInTuneCents)
            inTuneTime += dt;
        else
            inTuneTime = 0.0;
        if (target.string >= 0 && target.string < (int) tuned.size())
        {
            if (inTuneTime >= kTickSeconds)
                tuned[(size_t) target.string] = true;
            else if (std::abs (cents) > 5.0)
                tuned[(size_t) target.string] = false;
        }
    }
    else
    {
        sinceSeen += dt;
        inTuneTime = 0.0;
        if (sinceSeen > kHoldSeconds)
            hasNote = false;
    }

    // Strobe: the bands drift at a speed proportional to the error and stop when in tune
    if (live)
        strobePhase += juce::jlimit (-50.0, 50.0, cents) * 4.0 * dt;
    strobePhase = std::fmod (strobePhase, 1000.0);
    repaint();
}

void TunerDisplay::paint (juce::Graphics& g)
{
    auto area = drawDisplayPanel (g, getLocalBounds().toFloat(), "Tuner");
    const auto& tun = tuner::tunings()[(size_t) juce::jlimit (0, (int) tuner::tunings().size() - 1, params.tuning)];

    // info line (top right)
    juce::String info = juce::String (tun.name) + "  |  A4 = " + juce::String (params.reference, 1) + " Hz";
    if (params.bypassed)
        info << "  |  Bypassed";
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (colours::dimText);
    g.drawText (info, getLocalBounds().reduced (12, 4).withHeight (14), juce::Justification::right);

    // "MUTED" badge next to the panel title while the output is silenced
    if (params.mute && ! params.bypassed)
    {
        const auto badge = juce::Rectangle<float> (62.0f, 4.0f, 58.0f, 16.0f);
        g.setColour (colours::second);
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (colours::background);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("MUTED", badge, juce::Justification::centred);
    }

    const bool  inTune = hasNote && std::abs (cents) <= kInTuneCents;
    const auto  state  = ! hasNote ? colours::dimText : inTune ? colours::accent : colours::second;
    const float alpha  = live ? 1.0f : 0.45f;

    auto strings = tun.strings.empty() ? juce::Rectangle<float>() : area.removeFromBottom (40.0f);
    if (! strings.isEmpty())
        area.removeFromBottom (8.0f);

    auto noteArea  = area.removeFromTop (area.getHeight() * 0.5f);
    auto meterArea = area.reduced (0.0f, 4.0f);

    // --- note -----------------------------------------------------------------
    auto textLine = noteArea.removeFromBottom (20.0f);
    const float noteSize = juce::jlimit (36.0f, 96.0f, noteArea.getHeight() * 0.95f);
    const juce::String name = hasNote ? juce::String (tuner::noteName (target.midi, tun.flats)) : juce::String ("-");
    const juce::String oct  = hasNote ? juce::String (tuner::octaveOf (target.midi)) : juce::String();

    const juce::Font bigFont (juce::FontOptions (noteSize, juce::Font::bold));
    const juce::Font octFont (juce::FontOptions (noteSize * 0.33f, juce::Font::bold));
    const float nameW = juce::GlyphArrangement::getStringWidth (bigFont, name);
    const float octW  = juce::GlyphArrangement::getStringWidth (octFont, oct);
    const float total = nameW + octW + 4.0f;
    const auto  c     = noteArea.getCentre();
    const float x0    = c.x - total * 0.5f;

    g.setColour ((hasNote ? (inTune ? colours::accent : colours::text) : colours::dimText).withAlpha (alpha));
    g.setFont (bigFont);
    g.drawText (name, juce::Rectangle<float> (x0, noteArea.getY(), nameW + 2.0f, noteArea.getHeight()), juce::Justification::centredLeft);
    g.setFont (octFont);
    g.drawText (oct, juce::Rectangle<float> (x0 + nameW + 4.0f, c.y, octW + 2.0f, noteSize * 0.4f), juce::Justification::topLeft);

    // flat / sharp lights either side of the note
    const float ls   = juce::jlimit (14.0f, 30.0f, noteSize * 0.32f);
    const float gap  = juce::jmax (total * 0.5f + 30.0f, 70.0f);
    const bool  flat = hasNote && cents < -kInTuneCents, sharp = hasNote && cents > kInTuneCents;
    drawLight (g, juce::Rectangle<float> (ls, ls * 1.2f).withCentre ({ c.x - gap, c.y }), true,
               (flat ? colours::second : inTune ? colours::accent : colours::grid).withAlpha (alpha));
    drawLight (g, juce::Rectangle<float> (ls, ls * 1.2f).withCentre ({ c.x + gap, c.y }), false,
               (sharp ? colours::second : inTune ? colours::accent : colours::grid).withAlpha (alpha));

    g.setFont (juce::FontOptions (13.0f));
    if (hasNote)
    {
        juce::String line = centsText (cents) + "   " + juce::String (frequency, 2) + " Hz";
        if (std::abs (cents) > 50.0)
            line << "   " << (cents < 0.0 ? "Tune up" : "Tune down");
        g.setColour (colours::text.withAlpha (alpha));
        g.drawText (line, textLine, juce::Justification::centred);
    }
    else
    {
        g.setColour (colours::dimText);
        g.drawText (params.bypassed ? "Bypassed" : "Play a single note", textLine, juce::Justification::centred);
    }

    // --- meter ------------------------------------------------------------------
    if (params.display == 1)
        drawStrobe (g, meterArea, hasNote, state.withAlpha (alpha));
    else
        drawNeedle (g, meterArea, hasNote, state.withAlpha (alpha));

    if (! strings.isEmpty())
        drawStrings (g, strings);
}

void TunerDisplay::drawNeedle (juce::Graphics& g, juce::Rectangle<float> r, bool active, juce::Colour colour)
{
    const float radius = juce::jmin (r.getWidth() * 0.5f / std::sin (juce::degreesToRadians (kMeterDegrees)) - 10.0f,
                                     r.getHeight() - 8.0f);
    const juce::Point<float> centre (r.getCentreX(), r.getBottom() - 2.0f);
    auto angleFor = [] (double ct) { return juce::degreesToRadians ((float) (juce::jlimit (-50.0, 50.0, ct) / 50.0) * kMeterDegrees); };

    // in-tune zone
    juce::Path zone;
    zone.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, angleFor (-kInTuneCents), angleFor (kInTuneCents), true);
    g.setColour (colours::accent.withAlpha (0.35f));
    g.strokePath (zone, juce::PathStrokeType (8.0f));

    juce::Path scale;
    scale.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, angleFor (-50), angleFor (50), true);
    g.setColour (colours::grid);
    g.strokePath (scale, juce::PathStrokeType (2.0f));

    g.setFont (juce::FontOptions (11.0f));
    for (int ct = -50; ct <= 50; ct += 5)
    {
        const bool  major = ct % 25 == 0;
        const float a     = angleFor (ct);
        const float len   = ct == 0 ? 16.0f : major ? 11.0f : 6.0f;
        g.setColour (ct == 0 ? colours::accent : colours::grid.brighter (0.3f));
        g.drawLine ({ centre.getPointOnCircumference (radius - len, a), centre.getPointOnCircumference (radius, a) }, ct == 0 ? 2.5f : 1.5f);
        if (major)
        {
            const auto p = centre.getPointOnCircumference (radius + 12.0f, a);
            g.setColour (colours::dimText);
            g.drawText (ct == 0 ? "0" : (ct > 0 ? "+" : "") + juce::String (ct),
                        juce::Rectangle<float> (40.0f, 14.0f).withCentre (p), juce::Justification::centred);
        }
    }

    if (! active)
        return;
    const float a = angleFor (cents);
    g.setColour (colour);
    g.drawLine ({ centre.getPointOnCircumference (radius * 0.12f, a), centre.getPointOnCircumference (radius + 4.0f, a) }, 3.0f);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre));
}

void TunerDisplay::drawStrobe (juce::Graphics& g, juce::Rectangle<float> r, bool active, juce::Colour colour)
{
    r = r.reduced (juce::jmax (0.0f, (r.getWidth() - 520.0f) * 0.5f), 0.0f);
    g.setColour (colours::background);
    g.fillRoundedRectangle (r, 4.0f);

    constexpr int   rows   = 3;
    constexpr float stripe = 18.0f;
    const float     rowH   = (r.getHeight() - 8.0f) / (float) rows;
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (r.reduced (2.0f).toNearestInt());
    for (int row = 0; row < rows; ++row)
    {
        // rows move at 1x, 2x and 4x, like the octave rows of a classic strobe tuner
        const float speed = (float) (1 << row);
        const float off   = std::fmod ((float) strobePhase * stripe * speed, stripe * 2.0f);
        const float y     = r.getY() + 4.0f + rowH * (float) row;
        g.setColour (active ? colour.withMultipliedAlpha (1.0f - 0.2f * (float) row) : colours::grid);
        for (float x = r.getX() - stripe * 2.0f + off; x < r.getRight(); x += stripe * 2.0f)
            g.fillRect (juce::Rectangle<float> (x, y + 2.0f, stripe, rowH - 4.0f));
    }

    // centre marker
    g.setColour (colours::text.withAlpha (0.6f));
    g.fillRect (r.getCentreX() - 1.0f, r.getY(), 2.0f, 6.0f);
    g.fillRect (r.getCentreX() - 1.0f, r.getBottom() - 6.0f, 2.0f, 6.0f);
}

void TunerDisplay::drawStrings (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& tun = tuner::tunings()[(size_t) juce::jlimit (0, (int) tuner::tunings().size() - 1, params.tuning)];
    const int   n   = (int) tun.strings.size();
    const float w   = juce::jmin (64.0f, (r.getWidth() - 8.0f * (float) (n - 1)) / (float) n);
    float       x   = r.getCentreX() - ((float) n * w + 8.0f * (float) (n - 1)) * 0.5f;

    for (int i = 0; i < n; ++i, x += w + 8.0f)
    {
        const auto box     = juce::Rectangle<float> (x, r.getY(), w, r.getHeight());
        const bool current = hasNote && target.string == i;
        const bool inTune  = current && std::abs (cents) <= kInTuneCents;
        const bool done    = i < (int) tuned.size() && tuned[(size_t) i];

        g.setColour (inTune ? colours::accent.withAlpha (0.35f) : colours::background);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (current ? (inTune ? colours::accent : colours::second) : colours::grid);
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, current ? 2.0f : 1.0f);

        const int m = tun.strings[(size_t) i];
        g.setColour (current ? colours::text : colours::dimText);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (juce::String (tuner::noteName (m, tun.flats)) + juce::String (tuner::octaveOf (m)),
                    box.withTrimmedRight (done ? 12.0f : 0.0f), juce::Justification::centred);

        if (done) // tick
        {
            const auto t = juce::Rectangle<float> (10.0f, 8.0f).withCentre ({ box.getRight() - 11.0f, box.getCentreY() });
            juce::Path tick;
            tick.startNewSubPath (t.getX(), t.getCentreY());
            tick.lineTo (t.getX() + t.getWidth() * 0.4f, t.getBottom());
            tick.lineTo (t.getRight(), t.getY());
            g.setColour (colours::accent);
            g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
}

//==============================================================================
TunerEditor::TunerEditor (TunerProcessor& p) : EditorBase (p, "Tuner")
{
    knobsPerRow = 3;
    setDisplay (display);

    addRowLabel ("Tuning", 56);
    addChoiceBox ("tuning", 200, true,
                  "Chromatic shows the nearest note. A guitar or bass tuning shows the nearest string, "
                  "so a string tuned far off still points to the note it should be");
    addRowLabel ("Display", 60);
    addChoiceBox ("display", 100, false, "Needle: cents meter. Strobe: bands that drift while out of tune and stand still when in tune");
    addToggle ("mute", "Mute when engaged", 150,
               "Silences the output while the tuner is engaged, like a tuner pedal. "
               "Bypass the plugin to let the signal through again (the tuner keeps listening either way)");

    addKnob ("reference", "Reference", "Concert pitch: the frequency of A4 (440 Hz standard; 442 Hz for many orchestras)");
    addKnob ("threshold", "Threshold", "Input level below which the tuner ignores the signal. Lower it for quiet pickups, raise it on a noisy stage");
    addKnob ("smoothing", "Smoothing", "How steady the reading is: low = fast and jumpy, high = slow and calm");

    finishSetup (740, 560, 700, 500);
    startTimerHz (60); // smoother needle and strobe
    lastTime = juce::Time::getMillisecondCounterHiRes();
}

void TunerEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt  = juce::jlimit (0.001, 0.1, (now - lastTime) * 0.001);
    lastTime = now;
    display.update (processor.reading(), processor.currentParams(), dt);
}
