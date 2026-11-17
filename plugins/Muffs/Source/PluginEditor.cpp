#include "PluginEditor.h"

using namespace melon;

void MuffsDisplay::setState (const muffs::Params& p)
{
    params = p;
    repaint();
}

void MuffsDisplay::paint (juce::Graphics& g)
{
    const auto& model = muffs::models()[(size_t) juce::jlimit (0, muffs::kNumModels - 1, params.model)];
    auto plot = drawDisplayPanel (g, getLocalBounds().toFloat(), "Tone stack").withTrimmedLeft (28.0f).withTrimmedBottom (14.0f);

    constexpr double fLo = 20.0, fHi = 20000.0, dbLo = -30.0, dbHi = 3.0;
    auto xForHz = [&] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log (f / fLo) / std::log (fHi / fLo)); };
    auto yForDb = [&] (double db) { return juce::jmap ((float) juce::jlimit (dbLo, dbHi, db), (float) dbLo, (float) dbHi, plot.getBottom(), plot.getY()); };

    g.setFont (11.0f);
    for (double db : { -24.0, -18.0, -12.0, -6.0, 0.0 })
    {
        const float y = yForDb (db);
        g.setColour (juce::exactlyEqual (db, 0.0) ? colours::grid.brighter (0.2f) : colours::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (colours::dimText);
        g.drawText (juce::String ((int) db), (int) plot.getX() - 30, (int) y - 7, 26, 14, juce::Justification::right);
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

    auto curveFor = [&] (double tone) {
        juce::Path p;
        for (int i = 0; i < 300; ++i)
        {
            const double f = fLo * std::pow (fHi / fLo, i / 299.0);
            const float  x = xForHz (f), y = yForDb (muffs::toneStackDb (model, tone, f));
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        return p;
    };

    g.setColour (colours::dimText.withAlpha (0.35f));
    g.strokePath (curveFor (0.0), juce::PathStrokeType (1.0f));
    g.strokePath (curveFor (1.0), juce::PathStrokeType (1.0f));

    const float alpha = params.bypassed ? 0.35f : 1.0f;
    g.setColour (colours::second.withAlpha (alpha));
    g.strokePath (curveFor (params.tone), juce::PathStrokeType (2.0f));

    // centre of the scoop
    const double fScoop = std::sqrt (model.toneLpHz * model.toneHpHz);
    g.setColour (colours::accent.withAlpha (alpha));
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ xForHz (fScoop), yForDb (muffs::toneStackDb (model, params.tone, fScoop)) }));

    g.setColour (colours::dimText);
    const auto info = params.bypassed ? juce::String ("Bypassed")
                                      : juce::String (model.name) + "   |   low-pass " + juce::String ((int) model.toneLpHz)
                                            + " Hz, high-pass " + juce::String ((int) model.toneHpHz) + " Hz";
    g.drawText (info, plot.withHeight (14.0f).translated (0.0f, -18.0f).toNearestInt(), juce::Justification::right);
}

//==============================================================================
MuffsEditor::MuffsEditor (MuffsProcessor& p) : EditorBase (p, "Muffs")
{
    setDisplay (display);
    addChoiceBox ("model", 220, true, "Big Muff version. Use the arrows to step through them");

    addKnob ("volume", "Volume", "Output level of the pedal. 50 % = roughly the same loudness as the input");
    addKnob ("tone", "Tone", "Low-pass (bass, 0 %) to high-pass (treble, 100 %). At noon: the classic mid scoop");
    addKnob ("sustain", "Sustain", "Amount of fuzz (gain before the two clipping stages)");
    addKnob ("mix", "Mix", "Blend of dry and fuzz. 100 % = like the pedal; lower it to keep the attack of the dry note");

    finishSetup (720, 470, 620, 430);
}

void MuffsEditor::timerCallback()
{
    display.setState (processor.currentParams());
}
