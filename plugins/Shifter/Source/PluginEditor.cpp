#include "PluginEditor.h"

using namespace melon;

void PitchDisplay::update (const shifter::Params& p)
{
    const bool changed = ! valid || p.pitchA != params.pitchA || p.detuneA != params.detuneA || p.pitchB != params.pitchB
                         || p.detuneB != params.detuneB || p.voiceB != params.voiceB || p.mode != params.mode
                         || p.bypassed != params.bypassed || (p.feedback > 0.0) != (params.feedback > 0.0)
                         || std::abs (p.feedback - params.feedback) > 0.02;
    params = p;
    valid  = true;
    if (changed)
        repaint();
}

static juce::String semitoneText (double semis)
{
    const int s = (int) std::lround (semis);
    return (s > 0 ? "+" : "") + juce::String (s);
}

void PitchDisplay::paint (juce::Graphics& g)
{
    auto plot = drawDisplayPanel (g, getLocalBounds().toFloat(), "Pitch").reduced (14.0f, 0.0f);

    const float maxS = (float) shifter::kMaxSemitones;
    const float y    = plot.getCentreY();
    auto xFor = [&] (double semis) {
        return juce::jmap ((float) juce::jlimit (-(double) maxS, (double) maxS, semis), -maxS, maxS, plot.getX(), plot.getRight());
    };

    // ruler: a tick per semitone, taller at fifths/fourths and octaves
    g.setColour (colours::grid);
    g.fillRect (plot.getX(), y - 1.0f, plot.getWidth(), 2.0f);
    g.setFont (11.0f);
    for (int s = -shifter::kMaxSemitones; s <= shifter::kMaxSemitones; ++s)
    {
        const int   m = ((s % 12) + 12) % 12;
        const float h = m == 0 ? 10.0f : (m == 7 || m == 5) ? 6.0f : 3.0f;
        g.setColour (m == 0 ? colours::dimText : colours::grid);
        g.fillRect (xFor (s) - 0.5f, y - h, 1.0f, 2.0f * h);
        if (m == 0)
        {
            g.setColour (colours::dimText);
            g.drawText (s == 0 ? "0" : semitoneText (s), juce::Rectangle<float> (xFor (s) - 20.0f, y + 12.0f, 40.0f, 13.0f),
                        juce::Justification::centred);
        }
    }

    const bool  bypassed = params.bypassed;
    const float alpha    = bypassed ? 0.35f : 1.0f;
    const double a = params.pitchA + params.detuneA * 0.01;
    const double b = params.pitchB + params.detuneB * 0.01;

    // climbing echoes (each repeat goes through the shifter again)
    if (params.feedback > 0.0 && ! bypassed)
    {
        for (int k = 2; k <= 6; ++k)
        {
            const float fade = (float) std::pow (params.feedback * shifter::kMaxFeedback, k - 1);
            if (fade < 0.03f)
                break;
            for (int v = 0; v < (params.voiceB ? 2 : 1); ++v)
            {
                const double s = (v == 0 ? a : b) * k;
                if (std::abs (s) > maxS)
                    continue;
                g.setColour ((v == 0 ? colours::accent : colours::second).withAlpha (0.8f * fade));
                g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ xFor (s), y }));
            }
        }
    }

    // dry note
    g.setColour (colours::text.withAlpha (0.8f * alpha));
    g.fillRect (xFor (0.0) - 1.0f, y - 16.0f, 2.0f, 32.0f);

    auto drawVoice = [&] (double semis, int whole, double cents, juce::Colour c, bool above, float a2) {
        const float x = xFor (semis);
        g.setColour (c.withAlpha (a2));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ x, y }));
        g.drawLine (x, y + (above ? -7.0f : 7.0f), x, y + (above ? -22.0f : 22.0f), 1.5f);

        juce::String label = semitoneText (whole) + " st";
        if (std::abs (cents) >= 0.5)
            label << "  " << (cents > 0 ? "+" : "") << juce::String ((int) std::lround (cents)) << " ct";
        label << "   " << shifter::intervalName (whole);

        const float w  = 260.0f;
        const float lx = juce::jlimit (plot.getX(), plot.getRight() - w, x - w * 0.5f);
        g.setFont (juce::FontOptions (13.0f, above ? juce::Font::bold : juce::Font::plain));
        g.drawText (label, juce::Rectangle<float> (lx, above ? y - 42.0f : y + 26.0f, w, 16.0f), juce::Justification::centred);
    };

    if (params.voiceB)
        drawVoice (b, (int) params.pitchB, params.detuneB, colours::second, false, alpha);
    drawVoice (a, (int) params.pitchA, params.detuneA, colours::accent, true, alpha);

    const auto info = bypassed ? juce::String ("Bypassed")
                               : "Latency ~" + juce::String (juce::roundToInt (shifter::latencyMs (params.mode))) + " ms";
    g.setColour (colours::dimText);
    g.setFont (11.0f);
    g.drawText (info, getLocalBounds().reduced (12, 4).withHeight (14), juce::Justification::right);
}

//==============================================================================
ShifterEditor::ShifterEditor (ShifterProcessor& p) : EditorBase (p, "Shifter")
{
    knobsPerRow = 5;
    setDisplay (display);

    addRowLabel ("Mode", 50);
    addChoiceBox ("mode", 120, true,
                  "Tight: shortest splice window and lowest latency, best for single notes and riffs. "
                  "Normal: all-round. Smooth: longest window, best for chords and pads");
    addToggle ("voiceB", "Voice B", 100, "Turns on the second pitch voice (for harmonies, octaves both ways and doubling)");

    addKnob ("pitchA", "Pitch A", "Pitch of voice A in semitones (-24 to +24). Double-click for an octave up");
    addKnob ("detuneA", "Detune A", "Fine tuning of voice A in cents. Small amounts (5-15) give a doubling / chorus effect");
    addKnob ("pitchB", "Pitch B", "Pitch of voice B in semitones (-24 to +24)");
    addKnob ("detuneB", "Detune B", "Fine tuning of voice B in cents. Detune A and B in opposite directions for a wide doubler");
    addKnob ("width", "Width", "Stereo spread with Voice B on: voice A moves left and voice B right");
    addKnob ("delay", "Delay", "Delay before the shifted signal (0 - 1000 ms). Short delays thicken, long ones give pitched echoes");
    addKnob ("feedback", "Feedback", "Feeds the shifted echoes back into the shifter, so each repeat climbs (or falls) another step");
    addKnob ("tone", "Tone", "High cut on the shifted signal. Turn it down to soften the octave-up fizz");
    addKnob ("mix", "Mix", "Balance of dry and shifted signal. 100 % = shifted only (for retuning)");

    finishSetup (760, 610, 680, 570);
}

void ShifterEditor::timerCallback()
{
    const auto p = processor.currentParams();
    display.update (p);

    // Pitch B, Detune B and Width only matter with Voice B on
    for (size_t i : { (size_t) 2, (size_t) 3, (size_t) 4 })
    {
        knobs[i]->slider.setEnabled (p.voiceB);
        knobs[i]->label.setAlpha (p.voiceB ? 1.0f : 0.45f);
    }
}
