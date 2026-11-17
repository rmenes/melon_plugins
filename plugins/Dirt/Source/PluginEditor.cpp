#include "PluginEditor.h"

using namespace melon;

void DirtDisplay::setState (const dirt::Params& p, double sampleRate)
{
    params = p;
    fs     = sampleRate > 0.0 ? sampleRate : 48000.0;
    repaint();
}

void DirtDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto left   = bounds.removeFromLeft (juce::jmin (bounds.getHeight() * 1.2f, bounds.getWidth() * 0.4f));
    bounds.removeFromLeft (10.0f);

    const auto& model = dirt::models()[(size_t) juce::jlimit (0, dirt::kNumModels - 1, params.model)];
    const float alpha = params.bypassed ? 0.35f : 1.0f;

    // --- drive curve ---------------------------------------------------------
    {
        auto plot = drawDisplayPanel (g, left, "Drive curve").reduced (4.0f);
        g.setColour (colours::grid);
        g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
        g.drawVerticalLine ((int) plot.getCentreX(), plot.getY(), plot.getBottom());
        g.drawLine ({ plot.getBottomLeft(), plot.getTopRight() }, 1.0f);

        juce::Path curve;
        double     maxOut = 1e-6;
        for (int i = 0; i <= 200; ++i)
            maxOut = std::max (maxOut, std::abs (dirt::staticCurve (model, params.drive, -1.0 + i * 0.01)));
        for (int i = 0; i <= 200; ++i)
        {
            const double x = -1.0 + i * 0.01;
            const double y = dirt::staticCurve (model, params.drive, x) / maxOut;
            const float  px = juce::jmap ((float) x, -1.0f, 1.0f, plot.getX(), plot.getRight());
            const float  py = juce::jmap ((float) y, -1.0f, 1.0f, plot.getBottom(), plot.getY());
            if (i == 0) curve.startNewSubPath (px, py);
            else        curve.lineTo (px, py);
        }
        g.setColour (colours::second.withAlpha (alpha));
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }

    // --- tone response ---------------------------------------------------------
    {
        auto plot = drawDisplayPanel (g, bounds, "Tone").withTrimmedLeft (28.0f).withTrimmedBottom (14.0f);
        constexpr double fLo = 20.0, fHi = 20000.0, dbLo = -24.0, dbHi = 12.0;
        auto xForHz = [&] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log (f / fLo) / std::log (fHi / fLo)); };
        auto yForDb = [&] (double db) { return juce::jmap ((float) juce::jlimit (dbLo, dbHi, db), (float) dbLo, (float) dbHi, plot.getBottom(), plot.getY()); };

        g.setFont (11.0f);
        for (double db : { -18.0, -12.0, -6.0, 0.0, 6.0 })
        {
            const float y = yForDb (db);
            g.setColour (juce::exactlyEqual (db, 0.0) ? colours::grid.brighter (0.2f) : colours::grid);
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (colours::dimText);
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), (int) plot.getX() - 30, (int) y - 7, 26, 14, juce::Justification::right);
        }
        const std::pair<double, const char*> freqs[] = { { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } };
        for (auto [f, name] : freqs)
        {
            const float x = xForHz (f);
            g.setColour (colours::grid);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (colours::dimText);
            g.drawText (name, (int) x - 20, (int) plot.getBottom() + 2, 40, 12, juce::Justification::centred);
        }

        dirt::ToneStage tone;
        tone.set (model, fs, params.tone);
        juce::Path curve;
        for (int i = 0; i < 300; ++i)
        {
            const double f = fLo * std::pow (fHi / fLo, i / 299.0);
            if (f > 0.49 * fs) break;
            const float x = xForHz (f), y = yForDb (tone.responseDb (f, fs));
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }
        g.setColour (colours::accent.withAlpha (alpha));
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        g.setColour (colours::dimText);
        g.drawText (params.bypassed ? juce::String ("Bypassed") : juce::String (model.name),
                    plot.withHeight (14.0f).translated (0.0f, -18.0f).toNearestInt(), juce::Justification::right);
    }
}

//==============================================================================
DirtEditor::DirtEditor (DirtProcessor& p) : EditorBase (p, "Dirt")
{
    setDisplay (display);
    addChoiceBox ("model", 220, true, "Overdrive model. Use the arrows to step through them");

    addKnob ("drive", "Drive", "Amount of overdrive. Each model has its own gain range");
    addKnob ("tone", "Tone", "Brightness. Depending on the model, a treble cut or a bass/treble tilt");
    addKnob ("level", "Level", "Output level of the pedal. 50 % = roughly the same loudness as the input");
    addKnob ("mix", "Mix", "Blend of dry and overdriven signal. 100 % = like the pedal");

    finishSetup (720, 470, 620, 430);
}

void DirtEditor::timerCallback()
{
    display.setState (processor.currentParams(), processor.sampleRateForDisplay());
}
