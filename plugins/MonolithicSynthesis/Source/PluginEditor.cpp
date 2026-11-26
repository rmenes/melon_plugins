#include "PluginEditor.h"

using namespace melon;

namespace
{
juce::String noteName (float midi)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = juce::roundToInt (midi);
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}

// Threshold used by the engine for a Sensitivity value (0..1), linear
float thresholdFor (float sensitivity)
{
    return std::pow (10.0f, (-66.0f + 36.0f * (1.0f - juce::jlimit (0.0f, 1.0f, sensitivity))) / 20.0f);
}

// -72..0 dBFS mapped to 0..1 for the level bar
float levelToBar (float linear)
{
    const float db = 20.0f * std::log10 (juce::jmax (linear, 1.0e-6f));
    return juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 72.0f);
}
} // namespace

//==============================================================================
void MonolithicSynthesisDisplay::setState (const State& s)
{
    state = s;
    repaint();
}

void MonolithicSynthesisDisplay::paint (juce::Graphics& g)
{
    const auto& p     = state.params;
    const auto& infos = mono::getModeInfos();
    const auto& bank  = mono::getPatchBank();
    const int   mode  = juce::jlimit (0, mono::NumModes - 1, p.mode);
    const int   var   = juce::jlimit (0, mono::kVariationsPerMode - 1, p.variation);
    const auto& patch = bank[(size_t) mode][(size_t) var];
    const float alpha = state.bypassed ? 0.35f : 1.0f;

    auto bounds = getLocalBounds().toFloat();
    auto left   = bounds.removeFromLeft (juce::jmin (260.0f, bounds.getWidth() * 0.33f));
    bounds.removeFromLeft (10.0f);

    // --- note ------------------------------------------------------------------
    {
        auto area  = drawDisplayPanel (g, left, p.midiInput ? "Note (MIDI)" : "Note (Guitar)");
        auto meter = area.removeFromBottom (34.0f);

        if (state.active)
        {
            const float cents = (state.pitch - std::round (state.pitch)) * 100.0f;
            g.setColour (colours::accent.withAlpha (alpha));
            g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
            g.drawText (noteName (state.pitch), area.removeFromTop (54.0f).toNearestInt(), juce::Justification::left);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.setColour (colours::dimText);
            g.drawText ((cents >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (cents)) + " cents",
                        area.removeFromTop (18.0f).toNearestInt(), juce::Justification::left);
        }
        else
        {
            g.setColour (colours::dimText.withAlpha (0.6f));
            g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
            g.drawText ("--", area.removeFromTop (54.0f).toNearestInt(), juce::Justification::left);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (p.midiInput ? "waiting for MIDI notes" : "play a single note",
                        area.removeFromTop (18.0f).toNearestInt(), juce::Justification::left);
        }

        // input level with the trigger threshold
        if (! p.midiInput)
        {
            g.setFont (11.0f);
            g.setColour (colours::dimText);
            g.drawText ("Input", meter.removeFromTop (14.0f).toNearestInt(), juce::Justification::left);
            auto bar = meter.removeFromTop (10.0f);
            g.setColour (colours::grid);
            g.fillRoundedRectangle (bar, 3.0f);
            const float lvl = levelToBar (state.level);
            g.setColour ((state.active ? colours::accent : colours::dimText).withAlpha (alpha));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * lvl), 3.0f);
            const float tx = bar.getX() + bar.getWidth() * levelToBar (thresholdFor (p.sensitivity));
            g.setColour (colours::second);
            g.fillRect (tx - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
        }
    }

    // --- sound -----------------------------------------------------------------
    {
        auto area = drawDisplayPanel (g, bounds, "Sound");

        // 12 mode cells, 2 rows of 6
        auto grid = area.removeFromTop (juce::jmin (54.0f, area.getHeight() * 0.4f));
        const float cellW = grid.getWidth() / 6.0f, cellH = grid.getHeight() / 2.0f;
        for (int i = 0; i < mono::NumModes; ++i)
        {
            auto cell = juce::Rectangle<float> (grid.getX() + cellW * (float) (i % 6), grid.getY() + cellH * (float) (i / 6),
                                                cellW, cellH).reduced (2.0f);
            const bool on = i == mode;
            g.setColour (on ? colours::accent.withAlpha (0.85f * alpha) : colours::grid);
            g.fillRoundedRectangle (cell, 4.0f);
            g.setColour (on ? colours::background : colours::dimText);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (infos[(size_t) i].name, cell.toNearestInt(), juce::Justification::centred);
        }

        area.removeFromTop (8.0f);
        g.setColour (colours::text.withAlpha (alpha));
        g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        g.drawText (juce::String (var + 1) + "  " + patch.name, area.removeFromTop (28.0f).toNearestInt(), juce::Justification::left);

        // Param readout
        const auto& info = infos[(size_t) mode];
        const float values[3] { p.param1, p.param2, p.param3 };
        juce::String params;
        for (int i = 0; i < 3; ++i)
            params << info.macros[i].name << " " << juce::roundToInt (values[i] * 100.0f) << " %" << (i < 2 ? "    " : "");
        g.setFont (12.0f);
        g.setColour (colours::dimText);
        g.drawText (params, area.removeFromTop (18.0f).toNearestInt(), juce::Justification::left);

        // Pattern steps (SEQ / ARPEGGIO)
        const auto built = mono::buildPatch (mode, var, p.param1, p.param2, p.param3);
        if (built.pattern != mono::PatternNone && area.getHeight() > 20.0f)
        {
            const int div = juce::jlimit (0, (int) mono::kDivisions.size() - 1, juce::roundToInt (built.rateIdx));
            juce::String sub = juce::String ("Rate ") + mono::kDivisionNames[(size_t) div];
            if (state.bpm > 0.0) sub << "  at " << juce::String (state.bpm, 1) << " BPM";
            g.drawText (sub, area.removeFromTop (16.0f).toNearestInt(), juce::Justification::left);

            auto stepsArea = area.removeFromTop (juce::jmin (26.0f, area.getHeight()));
            int length = 16;
            if (built.pattern == mono::PatternArp)
                length = mono::kArpPatterns[(size_t) juce::jlimit (0, (int) mono::kArpPatterns.size() - 1, built.patternIndex)].length;
            const float w = stepsArea.getWidth() / 16.0f;
            for (int i = 0; i < length; ++i)
            {
                auto cell = juce::Rectangle<float> (stepsArea.getX() + w * (float) i, stepsArea.getY(), w, stepsArea.getHeight()).reduced (2.0f);
                bool  on = true;
                float h  = 1.0f;
                juce::String label;
                if (built.pattern == mono::PatternSeq)
                {
                    const auto& st = mono::kSeqPatterns[(size_t) juce::jlimit (0, (int) mono::kSeqPatterns.size() - 1, built.patternIndex)].steps[i];
                    on = st.on;
                    h  = 0.3f + 0.7f * st.filter;
                }
                else
                {
                    const int semis = mono::kArpPatterns[(size_t) juce::jlimit (0, (int) mono::kArpPatterns.size() - 1, built.patternIndex)].steps[i];
                    h     = 0.35f + 0.65f * (float) semis / 24.0f;
                    label = semis == 0 ? "R" : "+" + juce::String (semis);
                }
                const bool current = i == state.step;
                g.setColour (current ? colours::second.withAlpha (alpha)
                                     : (on ? colours::accent.withAlpha (0.55f * alpha) : colours::grid));
                g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - h)), 2.0f);
                if (label.isNotEmpty() && w > 24.0f)
                {
                    g.setColour (colours::background);
                    g.setFont (10.0f);
                    g.drawText (label, cell.toNearestInt(), juce::Justification::centredBottom);
                }
            }
        }
        else if (state.bypassed)
        {
            g.drawText ("BYPASSED", area.removeFromTop (16.0f).toNearestInt(), juce::Justification::left);
        }
    }
}

//==============================================================================
MonolithicSynthesisEditor::MonolithicSynthesisEditor (MonolithicSynthesisProcessor& p) : EditorBase (p, "Monolithic Synthesis")
{
    titleWidth  = 230;
    knobsPerRow = 6;
    setDisplay (display);

    addChoiceBox ("mode", 120, true, "Synth mode (12 types, like a guitar synth pedal). Use the arrows to step through them");
    addRowLabel ("Sound", 50);
    soundBox = &addChoiceBox ("sound", 170, true, "The 6 sounds of the current mode");
    addRowLabel ("Octave", 54);
    addChoiceBox ("octave", 64, false, "Shifts the synth up or down by whole octaves");
    addToggle ("snap", "Pitch Snap", 100, "Rounds the tracked pitch to the nearest semitone (bends then step instead of glide)");
    addToggle ("midi", "MIDI In", 90, "Play the synth from MIDI notes instead of the guitar (e.g. a MIDI pickup or a keyboard)");

    paramKnobs[0] = &addKnob ("param1", "Param 1", "Mode-specific parameter (its name changes with the mode). 50 % = the sound as designed");
    paramKnobs[1] = &addKnob ("param2", "Param 2", "Mode-specific parameter (its name changes with the mode). 50 % = the sound as designed");
    paramKnobs[2] = &addKnob ("param3", "Param 3", "Mode-specific parameter (its name changes with the mode). 50 % = the sound as designed");
    addKnob ("effect", "Effect", "Level of the synth");
    addKnob ("mix", "Mix", "Balance between your dry guitar and the synth: 0 % = guitar only, 100 % = synth only, 50 % = both at equal loudness");
    sensitivityKnob = &addKnob ("sensitivity", "Sensitivity", "How easily notes trigger. Raise it if soft notes are missed, lower it if string noise triggers notes. The orange mark in the Input meter is the trigger level");

    finishSetup (900, 600, 800, 560);
}

void MonolithicSynthesisEditor::updateMode (int mode)
{
    const auto& info = mono::getModeInfos()[(size_t) mode];
    for (int i = 0; i < 3; ++i)
        paramKnobs[i]->label.setText (info.macros[i].name, juce::dontSendNotification);

    const auto& bank = mono::getPatchBank()[(size_t) mode];
    for (int i = 0; i < mono::kVariationsPerMode; ++i)
        soundBox->changeItemText (i + 1, juce::String (i + 1) + "  " + bank[(size_t) i].name);
    soundBox->setText (soundBox->getItemText (soundBox->getSelectedItemIndex()), juce::dontSendNotification);
}

void MonolithicSynthesisEditor::timerCallback()
{
    const auto p = processor.currentParams();
    if (p.mode != shownMode)
    {
        shownMode = p.mode;
        updateMode (shownMode);
    }
    sensitivityKnob->slider.setEnabled (! p.midiInput);

    auto& m = processor.meters();
    MonolithicSynthesisDisplay::State s;
    s.params   = p;
    s.pitch    = m.pitch.load();
    s.level    = m.level.load();
    s.active   = m.active.load();
    s.step     = m.step.load();
    s.bpm      = processor.hostBpm();
    s.bypassed = processor.flag ("bypass");
    display.setState (s);
}
