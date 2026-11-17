#include "PluginEditor.h"

using namespace melon;

void GateDisplay::pull (const gate::MeterQueue& q, const gate::Params& p)
{
    params = p;
    q.drain (readIndex, [this] (const gate::MeterPoint& m) {
        history[(size_t) head] = m;
        head = (head + 1) % kHistory;
    });
    repaint();
}

void GateDisplay::paint (juce::Graphics& g)
{
    auto plot = drawDisplayPanel (g, getLocalBounds().toFloat(), "Levels").withTrimmedLeft (30.0f);
    constexpr float dbLo = -84.0f, dbHi = 0.0f;
    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (dbLo, dbHi, db), dbLo, dbHi, plot.getBottom(), plot.getY()); };

    g.setFont (11.0f);
    for (float db : { -72.0f, -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
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

    // threshold (and where it drops to with hysteresis)
    const float yThr = yForDb ((float) params.thresholdDb);
    g.setColour (colours::text.withAlpha (0.8f));
    const float dashes[] = { 6.0f, 4.0f };
    g.drawDashedLine ({ plot.getX(), yThr, plot.getRight(), yThr }, dashes, 2, 1.2f);
    g.setColour (colours::dimText);
    g.drawText ("Threshold " + juce::String (params.thresholdDb, 1) + " dB", plot.withY (yThr - 15.0f).withHeight (14.0f).toNearestInt(),
                juce::Justification::right);

    const auto& last = history[(size_t) ((head + kHistory - 1) % kHistory)];
    const juce::String modes[] = { "Classic", "Clean", "Guitar", "Ducking" };
    const auto info = params.bypassed ? juce::String ("Bypassed")
                      : modes[(int) params.mode] + (params.audition ? "  |  AUDITION" : "") + "  |  GR "
                            + juce::String (last.grDb, 1) + " dB";
    g.setColour (params.audition ? colours::second : colours::dimText);
    g.drawText (info, getLocalBounds().reduced (12, 4).withHeight (14), juce::Justification::right);
}

//==============================================================================
GatesOfDoomEditor::GatesOfDoomEditor (GatesOfDoomProcessor& p) : EditorBase (p, "Gates of Doom")
{
    titleWidth = 160;
    setDisplay (display);
    addChoiceBox ("mode", 160, false,
                  "Classic: fast peak detection. Clean: smoother RMS detection and slower fades. "
                  "Guitar: extra hysteresis so decaying notes don't chatter. Ducking: turns the signal DOWN when it is above the threshold");
    addToggle ("audition", "Audition", 110, "Listen to the filtered side-chain (to set the side-chain filters)");

    addKnob ("threshold", "Threshold", "Level where the gate opens");
    addKnob ("range", "Range", "Maximum attenuation when the gate is closed. Lower values leave some of the signal through");
    addKnob ("ratio", "Ratio", "Expansion ratio below the threshold. Fully right = gate; low values = gentle expander");
    addKnob ("knee", "Knee", "Softens the transition around the threshold");
    addKnob ("hysteresis", "Hysteresis", "Once open, the gate closes this many dB below the threshold (prevents chatter)");
    addKnob ("mix", "Mix", "Blend of gated and dry signal");
    addKnob ("attack", "Attack", "Time to open fully");
    addKnob ("hold", "Hold", "Time the gate stays open after the signal falls under the threshold");
    addKnob ("release", "Release", "Time to close fully");
    addKnob ("lookahead", "Lookahead", "Opens the gate slightly before the note arrives (adds this much latency)");
    addKnob ("schp", "SC HP", "Side-chain high-pass: ignore low rumble when detecting (20 Hz = off)");
    addKnob ("sclp", "SC LP", "Side-chain low-pass: ignore hiss/cymbals when detecting (20 kHz = off)");

    finishSetup (820, 640, 720, 580);
}

void GatesOfDoomEditor::timerCallback()
{
    display.pull (processor.meters(), processor.currentParams());
}
