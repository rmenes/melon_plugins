#include "PluginEditor.h"

using namespace melon;

void MultiModDisplay::setState (const mmod::Params& p, double lfoPosition, double hostBpm, bool isSynced, const juce::String& name)
{
    params = p; position = lfoPosition; bpm = hostBpm; synced = isSynced; syncName = name;
    repaint();
}

void MultiModDisplay::paint (juce::Graphics& g)
{
    const int   idx  = juce::jlimit (0, mmod::kNumMachines - 1, params.machine);
    const auto& info = mmod::machines()[(size_t) idx];

    auto bounds = getLocalBounds().toFloat();
    auto left   = bounds.removeFromLeft (juce::jmin (280.0f, bounds.getWidth() * 0.36f));
    bounds.removeFromLeft (10.0f);

    // --- machine and speed -----------------------------------------------------
    {
        auto area = drawDisplayPanel (g, left, "Machine");
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        g.drawText (info.name, area.removeFromTop (30.0f).toNearestInt(), juce::Justification::left);

        g.setFont (12.0f);
        g.setColour (colours::dimText);
        g.drawFittedText (info.description, area.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 2);

        auto speedArea = area.removeFromBottom (juce::jmin (area.getHeight(), 64.0f));
        juce::String big, sub;
        if (idx == mmod::autoswell)
        {
            big = juce::String (mmod::paramText (idx, 1, params.param1));
            sub = "RISE TIME";
        }
        else
        {
            const double hz = params.speedHz;
            if (idx == mmod::quadrature)
            {
                const double f = hz * mmod::quadRangeMul (mmod::stepIndex (params.param2, 3));
                big = f >= 100.0 ? juce::String (juce::roundToInt (f)) + " Hz" : juce::String (f, 2) + " Hz";
            }
            else
            {
                big = juce::String (hz, hz >= 10.0 ? 1 : 2) + " Hz";
            }
            if (idx == mmod::patternTrem) sub << "PER STEP   ";
            if (synced) sub << "SYNC " << syncName << "   ";
            if (bpm > 0.0) sub << juce::String (bpm, 1) << " BPM";
        }
        g.setColour (colours::accent);
        g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
        g.drawText (big, speedArea.removeFromTop (40.0f).toNearestInt(), juce::Justification::left);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (colours::dimText);
        g.drawText (sub, speedArea.toNearestInt(), juce::Justification::left);
    }

    // --- modulation ------------------------------------------------------------
    {
        auto plot = drawDisplayPanel (g, bounds, "Modulation").reduced (0.0f, 2.0f);
        const float alpha = params.bypassed ? 0.35f : 1.0f;
        auto yFor = [&] (double v) { return plot.getBottom() - plot.getHeight() * (float) juce::jlimit (0.0, 1.0, v); };

        g.setColour (colours::grid);
        for (int i = 0; i <= 4; ++i)
            g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 4.0f), plot.getX(), plot.getRight());

        if (idx == mmod::autoswell)
        {
            // the fade-in of each note
            const double d = params.depth;
            juce::Path p;
            for (int i = 0; i <= 200; ++i)
            {
                const double t = i / 200.0, v = (1.0 - d) + d * t * t * (3.0 - 2.0 * t);
                const float  x = plot.getX() + plot.getWidth() * (float) t;
                if (i == 0) p.startNewSubPath (x, yFor (v)); else p.lineTo (x, yFor (v));
            }
            g.setColour (colours::accent.withAlpha (alpha));
            g.strokePath (p, juce::PathStrokeType (2.0f));
            g.setFont (11.0f);
            g.setColour (colours::dimText);
            g.drawText ("note starts", plot.withHeight (14.0f).translated (4.0f, plot.getHeight() - 16.0f).toNearestInt(), juce::Justification::left);
            g.drawText ("full level after " + juce::String (mmod::paramText (idx, 1, params.param1)),
                        plot.withHeight (14.0f).translated (-4.0f, 2.0f).toNearestInt(), juce::Justification::right);
            return;
        }

        // Two cycles (Pattern Trem: the whole pattern), scrolling with the LFO
        const int    steps = mmod::stepPatterns()[(size_t) mmod::stepIndex (params.param1, 8)].count;
        const double span  = idx == mmod::patternTrem ? (double) steps : 2.0;
        const double start = idx == mmod::patternTrem ? std::floor (position / span) * span : std::floor (position);
        const double offset = idx == mmod::patternTrem ? params.width : 0.5 * params.width;

        for (int c = 1; c >= 0; --c)
        {
            juce::Path p;
            const int n = 400;
            for (int i = 0; i <= n; ++i)
            {
                const double pos = start + span * i / n - (c == 1 ? offset : 0.0);
                const float  x   = plot.getX() + plot.getWidth() * (float) i / (float) n;
                const float  y   = yFor (mmod::sketchValue (params, pos));
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour ((c == 0 ? colours::accent : colours::second).withAlpha (alpha * (c == 0 ? 1.0f : 0.8f)));
            g.strokePath (p, juce::PathStrokeType (c == 0 ? 2.0f : 1.5f));
        }

        // playhead
        if (idx != mmod::autoswell && params.speedHz > 0.0)
        {
            const double rel = juce::jlimit (0.0, 1.0, (position - start) / span);
            const float  x   = plot.getX() + plot.getWidth() * (float) rel;
            g.setColour (colours::text.withAlpha (0.5f * alpha));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (colours::accent.withAlpha (alpha));
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x, yFor (mmod::sketchValue (params, position)) }));
        }

        g.setFont (11.0f);
        g.setColour (colours::dimText);
        g.drawText (idx == mmod::patternTrem ? juce::String (steps) + " steps" : juce::String ("2 cycles"),
                    plot.withHeight (14.0f).translated (-4.0f, plot.getHeight() - 16.0f).toNearestInt(), juce::Justification::right);
        g.drawText ("L", (int) plot.getX() + 4, (int) plot.getY(), 12, 14, juce::Justification::left);
        g.setColour (colours::second);
        g.drawText ("R", (int) plot.getX() + 16, (int) plot.getY(), 12, 14, juce::Justification::left);
    }
}

//==============================================================================
MultiModEditor::MultiModEditor (MultiModProcessor& p) : EditorBase (p, "MultiMod")
{
    titleWidth  = 140;
    knobsPerRow = 6;
    setDisplay (display);

    addChoiceBox ("machine", 150, true, "Modulation machine. Use the arrows to step through them");
    tapButton = &makeButton ("TAP", "Tap the tempo: click 2 or more times in rhythm. Sets Speed (one cycle per tap; one step in Pattern Trem) and turns Sync off");
    tapButton->onClick = [this] { processor.tap(); };
    addRowItem (*tapButton, 70);
    addRowLabel ("Sync", 40);
    syncBox = &addChoiceBox ("sync", 100, false, "Sync the LFO to the host tempo (Off = use the Speed knob). Pattern Trem: the length of one step");
    addRowLabel ("Wave", 44);
    waveBox = &addChoiceBox ("wave", 120, false, "LFO waveform (machines without an LFO wave choice ignore it)");

    speedKnob = &addKnob ("speed", "Speed", "LFO speed, 0.05 to 15 Hz (ignored while Sync is on). Pattern Trem: steps per second");
    depthKnob = &addKnob ("depth", "Depth", "Amount of modulation");
    addKnob ("mix", "Mix", "Dry/effect balance. 50 % = equal parts; 100 % = effect only (use it for the trems and filters)");
    addKnob ("width", "Width", "Stereo: offsets the right channel's LFO by up to half a cycle (one step in Pattern Trem; mic spread in Rotary)");
    param1 = &addKnob ("param1", "Param 1", "Machine-specific parameter (its name changes with the machine)");
    param2 = &addKnob ("param2", "Param 2", "Machine-specific parameter (its name changes with the machine)");

    finishSetup (900, 640, 800, 600);
}

void MultiModEditor::updateMachine (int machine)
{
    const auto& info = mmod::machines()[(size_t) machine];
    param1->label.setText (info.param1, juce::dontSendNotification);
    param2->label.setText (info.param2, juce::dontSendNotification);
    param1->slider.textFromValueFunction = [machine] (double v) { return juce::String (mmod::paramText (machine, 1, v * 0.01)); };
    param2->slider.textFromValueFunction = [machine] (double v) { return juce::String (mmod::paramText (machine, 2, v * 0.01)); };
    param1->slider.updateText();
    param2->slider.updateText();

    depthKnob->slider.setEnabled (info.usesDepth);
    waveBox->setEnabled (info.usesWave);
    syncBox->setEnabled (info.usesSpeed);
    tapButton->setEnabled (info.usesSpeed);
}

void MultiModEditor::timerCallback()
{
    const auto p      = processor.currentParams();
    const int  sync   = processor.choice ("sync");
    const bool synced = sync > 0 && processor.hostBpm() > 0.0;
    if (p.machine != shownMachine)
    {
        shownMachine = p.machine;
        updateMachine (shownMachine);
    }
    speedKnob->slider.setEnabled (! synced && mmod::machines()[(size_t) shownMachine].usesSpeed);
    display.setState (p, processor.lfoPosition(), processor.hostBpm(), synced,
                      mmod::syncValues()[(size_t) juce::jlimit (0, (int) mmod::syncValues().size() - 1, sync)].first);
}
