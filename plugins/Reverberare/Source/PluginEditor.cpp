#include "PluginEditor.h"

using namespace melon;

void ReverbDisplay::setState (const reverb::Params& p)
{
    params = p;
    repaint();
}

void ReverbDisplay::paint (juce::Graphics& g)
{
    const int   idx  = juce::jlimit (0, reverb::kNumMachines - 1, params.machine);
    const auto& info = reverb::machines()[(size_t) idx];
    auto plot = drawDisplayPanel (g, getLocalBounds().toFloat(), juce::String (info.name)).withTrimmedBottom (14.0f).withTrimmedLeft (28.0f);

    // description under the title
    g.setFont (12.0f);
    g.setColour (colours::dimText);
    g.drawText (info.description, plot.withHeight (14.0f).toNearestInt(), juce::Justification::left);
    plot.removeFromTop (20.0f);

    const bool   shortOnes = idx == reverb::nonlinear || idx == reverb::reflections;
    const double pre   = params.preDelayMs * 0.001;
    const double decay = params.infinite ? 1e9 : params.decayS;
    const double span  = std::max (0.5, pre + (params.infinite ? 4.0 : (shortOnes ? std::min (params.decayS * 0.4, 1.6) + 0.3 : params.decayS * 1.1)));
    constexpr double dbLo = -60.0;

    auto xFor = [&] (double t) { return plot.getX() + plot.getWidth() * (float) (t / span); };
    auto yFor = [&] (double db) { return juce::jmap ((float) juce::jlimit (dbLo, 0.0, db), (float) dbLo, 0.0f, plot.getBottom(), plot.getY()); };

    g.setFont (11.0f);
    for (double db : { -48.0, -36.0, -24.0, -12.0, 0.0 })
    {
        g.setColour (colours::grid);
        g.drawHorizontalLine ((int) yFor (db), plot.getX(), plot.getRight());
        g.setColour (colours::dimText);
        g.drawText (juce::String ((int) db), (int) plot.getX() - 30, (int) yFor (db) - 7, 26, 14, juce::Justification::right);
    }
    const double step = span > 20 ? 5.0 : span > 8 ? 2.0 : span > 3 ? 1.0 : span > 1.2 ? 0.5 : 0.1;
    for (double t = step; t < span; t += step)
    {
        g.setColour (colours::grid);
        g.drawVerticalLine ((int) xFor (t), plot.getY(), plot.getBottom());
        g.setColour (colours::dimText);
        g.drawText (juce::String (t, step < 1.0 ? 1 : 0) + " s", (int) xFor (t) - 24, (int) plot.getBottom() + 2, 48, 12, juce::Justification::centred);
    }

    const float alpha = params.bypassed ? 0.35f : 1.0f;

    // direct sound
    g.setColour (colours::text.withAlpha (alpha));
    g.fillRect (juce::Rectangle<float> (xFor (0.0), yFor (0.0), 2.0f, plot.getBottom() - yFor (0.0)));

    // early reflections
    const bool hasEr = idx == reverb::room || idx == reverb::hall || idx == reverb::reflections;
    if (hasEr)
    {
        const double scale = idx == reverb::reflections ? 0.3 + 2.2 * params.param1 : idx == reverb::room ? 0.4 + 0.6 * params.param1 : 1.0;
        const double erMs[] = { 7.1, 11.3, 17.9, 23.5, 31.1, 37.9, 46.3, 55.1, 67.7, 79.3, 93.1, 109.7 };
        g.setColour (colours::second.withAlpha (alpha));
        for (int k = 0; k < 12; ++k)
        {
            const double t  = pre + erMs[k] * scale * 0.001;
            const double db = -3.0 - 1.8 * k;
            g.fillRect (juce::Rectangle<float> (xFor (t), yFor (db), 2.0f, plot.getBottom() - yFor (db)));
        }
    }

    // tail envelope
    if (idx != reverb::reflections)
    {
        juce::Path tail, fill;
        const int N = 200;
        for (int i = 0; i <= N; ++i)
        {
            const double t = pre + (span - pre) * i / N;
            const double age = t - pre;
            double db;
            if (idx == reverb::nonlinear)
            {
                const double win = std::clamp (params.decayS * 0.25, 0.08, 1.5);
                const double pos = age / win;
                const double s   = params.param1;
                const double env = s < 0.5 ? (1.0 - 2.0 * s) * pos + 2.0 * s : (2.0 - 2.0 * s) + (2.0 * s - 1.0) * (1.0 - pos);
                db = pos <= 1.0 ? gainToDb (std::max (1e-3, env)) - 6.0 : -100.0;
            }
            else
            {
                double build = idx == reverb::bloom ? std::min (1.0, age / (0.25 + 0.8 * params.param1))
                             : idx == reverb::swell ? std::min (1.0, age / (0.05 + params.param1 * params.param1 * 2.95))
                             : idx == reverb::cloud ? std::min (1.0, age / 0.3)
                             : std::min (1.0, age / 0.04);
                db = gainToDb (std::max (1e-4, build)) - 6.0 - 60.0 * age / decay;
            }
            const float x = xFor (t), y = yFor (db);
            if (i == 0) { tail.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom()); }
            tail.lineTo (x, y);
            fill.lineTo (x, y);
        }
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.28f * alpha), 0.0f, plot.getY(),
                                                 colours::accent.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colours::accent.withAlpha (alpha));
        g.strokePath (tail, juce::PathStrokeType (2.0f));
    }

    g.setColour (params.infinite ? colours::second : colours::dimText);
    const auto readout = params.bypassed ? juce::String ("Bypassed")
                         : params.infinite ? juce::String ("INFINITE")
                                           : "Decay " + juce::String (params.decayS, 2) + " s  |  Pre-delay " + juce::String ((int) params.preDelayMs) + " ms";
    g.drawText (readout, getLocalBounds().reduced (12, 4).withHeight (14), juce::Justification::right);
}

//==============================================================================
ReverberareEditor::ReverberareEditor (ReverberareProcessor& p) : EditorBase (p, "Reverberare")
{
    titleWidth = 150;
    knobsPerRow = 7;
    setDisplay (display);
    addChoiceBox ("machine", 200, true, "Reverb machine. Use the arrows to step through them");
    addToggle ("infinite", "Infinite", 110, "Freezes the current tail and holds it forever (new notes play dry over it)");

    addKnob ("decay", "Decay", "Reverb time (RT60): how long the tail takes to fall by 60 dB");
    addKnob ("predelay", "Pre-delay", "Gap between the dry note and the start of the reverb");
    addKnob ("tone", "Tone", "Darker (0 %) to brighter (100 %) tail: high-frequency damping");
    addKnob ("mod", "Mod", "Slow modulation of the tail: from static to lush and chorused");
    param1 = &addKnob ("param1", "Param 1", "Machine-specific parameter (its name changes with the machine)");
    param2 = &addKnob ("param2", "Param 2", "Machine-specific parameter (its name changes with the machine)");
    addKnob ("mix", "Mix", "Dry/wet balance (equal-power)");

    finishSetup (860, 480, 760, 440);
}

void ReverberareEditor::updateParamLabels (int machine)
{
    const auto& info = reverb::machines()[(size_t) machine];
    param1->label.setText (info.param1, juce::dontSendNotification);
    param2->label.setText (info.param2, juce::dontSendNotification);
    param1->slider.textFromValueFunction = [machine] (double v) { return juce::String (reverb::paramText (machine, 1, v * 0.01)); };
    param2->slider.textFromValueFunction = [machine] (double v) { return juce::String (reverb::paramText (machine, 2, v * 0.01)); };
    param1->slider.updateText();
    param2->slider.updateText();
}

void ReverberareEditor::timerCallback()
{
    const auto p = processor.currentParams();
    if (p.machine != shownMachine)
    {
        shownMachine = p.machine;
        updateParamLabels (shownMachine);
    }
    display.setState (p);
}
