#include "PluginEditor.h"

using namespace melon;

void MultiDelayDisplay::setState (const mdelay::Params& p, double hostBpm, bool isSynced, const juce::String& name)
{
    params = p; bpm = hostBpm; synced = isSynced; syncName = name;
    repaint();
}

void MultiDelayDisplay::paint (juce::Graphics& g)
{
    const int   idx  = juce::jlimit (0, mdelay::kNumMachines - 1, params.machine);
    const auto& info = mdelay::machines()[(size_t) idx];

    auto bounds = getLocalBounds().toFloat();
    auto left   = bounds.removeFromLeft (juce::jmin (280.0f, bounds.getWidth() * 0.36f));
    bounds.removeFromLeft (10.0f);

    // --- machine and time ------------------------------------------------------
    {
        auto area = drawDisplayPanel (g, left, "Machine");
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        g.drawText (info.name, area.removeFromTop (30.0f).toNearestInt(), juce::Justification::left);

        g.setFont (12.0f);
        g.setColour (colours::dimText);
        g.drawFittedText (info.description, area.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 2);

        auto timeArea = area.removeFromBottom (juce::jmin (area.getHeight(), 64.0f));
        const double ms = params.timeMs;
        const auto timeText = ms >= 1000.0 ? juce::String (ms / 1000.0, 2) + " s" : juce::String (juce::roundToInt (ms)) + " ms";
        g.setColour (params.infinite ? colours::second : colours::accent);
        g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
        g.drawText (timeText, timeArea.removeFromTop (40.0f).toNearestInt(), juce::Justification::left);

        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (colours::dimText);
        juce::String sub;
        if (synced) sub << "SYNC " << syncName << "   ";
        if (bpm > 0.0) sub << juce::String (bpm, 1) << " BPM   ";
        if (params.infinite) sub << "INFINITE";
        g.drawText (sub, timeArea.toNearestInt(), juce::Justification::left);
    }

    // --- repeats ---------------------------------------------------------------
    {
        auto plot = drawDisplayPanel (g, bounds, "Repeats").withTrimmedBottom (14.0f);
        const double T    = params.timeMs;
        const double span = std::max (200.0, (idx == mdelay::dual ? T * 1.6 : T) * 6.5);
        auto xFor = [&] (double ms) { return plot.getX() + plot.getWidth() * (float) (ms / span); };
        const float mid = plot.getCentreY(), half = plot.getHeight() * 0.5f - 2.0f;

        g.setColour (colours::grid);
        g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());
        g.setFont (11.0f);
        const double step = span > 16000 ? 4000 : span > 8000 ? 2000 : span > 4000 ? 1000 : span > 1500 ? 500 : span > 600 ? 200 : 100;
        for (double t = step; t < span; t += step)
        {
            g.setColour (colours::grid);
            g.drawVerticalLine ((int) xFor (t), plot.getY(), plot.getBottom());
            g.setColour (colours::dimText);
            g.drawText (t >= 1000 ? juce::String (t / 1000.0, 1) + " s" : juce::String ((int) t) + " ms",
                        (int) xFor (t) - 26, (int) plot.getBottom() + 2, 52, 12, juce::Justification::centred);
        }

        const float alpha = params.bypassed && ! params.trails ? 0.35f : 1.0f;
        g.setColour (colours::text.withAlpha (alpha));
        g.fillRect (juce::Rectangle<float> (xFor (0.0), mid - half, 2.0f, 2.0f * half));

        for (const auto& e : mdelay::echoSketch (params, span))
        {
            const float h = half * (float) e.amp;
            const auto  c = (e.side > 0 ? colours::second : colours::accent).withAlpha (alpha);
            g.setColour (c);
            auto drawAt = [&] (bool up) {
                if (e.reversed)
                {
                    // backwards: the sound swells up to the end of the slice
                    const float x1 = xFor (e.timeMs), x0 = juce::jmax (plot.getX(), xFor (e.timeMs - T * 0.8));
                    juce::Path p;
                    p.startNewSubPath (x0, mid);
                    p.lineTo (x1, up ? mid - h : mid + h);
                    p.lineTo (x1, mid);
                    p.closeSubPath();
                    g.setColour (c.withAlpha (0.35f * alpha));
                    g.fillPath (p);
                    g.setColour (c);
                    g.strokePath (p, juce::PathStrokeType (1.5f));
                }
                else
                {
                    g.fillRect (juce::Rectangle<float> (xFor (e.timeMs) - 1.5f, up ? mid - h : mid, 3.0f, h));
                }
            };
            if (e.side <= 0) drawAt (true);
            if (e.side >= 0) drawAt (false);
        }

        g.setColour (colours::dimText);
        g.drawText ("L", (int) plot.getX() + 4, (int) plot.getY(), 12, 14, juce::Justification::left);
        g.drawText ("R", (int) plot.getX() + 4, (int) plot.getBottom() - 14, 12, 14, juce::Justification::left);
    }
}

//==============================================================================
MultiDelayEditor::MultiDelayEditor (MultiDelayProcessor& p) : EditorBase (p, "Multi Delay")
{
    titleWidth  = 150;
    knobsPerRow = 5;
    setDisplay (display);

    addChoiceBox ("machine", 150, true, "Delay machine. Use the arrows to step through them");
    auto& tapButton = makeButton ("TAP", "Tap the tempo: click 2 or more times in rhythm. Sets Time and turns Sync off");
    tapButton.onClick = [this] { processor.tap(); };
    addRowItem (tapButton, 70);
    addRowLabel ("Sync", 40);
    addChoiceBox ("sync", 100, false, "Sync the delay to the host tempo (Off = use the Time knob)");
    addToggle ("infinite", "Infinite", 96, "Holds the current repeats forever (new notes play dry over them)");
    addToggle ("trails", "Trails", 86, "When bypassed, the repeats keep ringing out instead of stopping");

    timeKnob = &addKnob ("time", "Time", "Delay time (ignored while Sync is on)");
    addKnob ("repeats", "Repeats", "Number of repeats. 100 % = (almost) endless");
    addKnob ("mix", "Mix", "Dry/delay balance. 50 % = both at full level; above 50 % the dry fades out");
    addKnob ("filter", "Filter", "Darker (0 %) to full-range (100 %) repeats. Inside the loop: each repeat gets darker");
    addKnob ("grit", "Grit", "Saturation of the repeats, from clean to driven");
    addKnob ("speed", "Speed", "Speed of the delay-time modulation (0.05 to 10 Hz)");
    addKnob ("depth", "Depth", "Amount of delay-time modulation: subtle chorus to deep warble");
    param1 = &addKnob ("param1", "Param 1", "Machine-specific parameter (its name changes with the machine)");
    param2 = &addKnob ("param2", "Param 2", "Machine-specific parameter (its name changes with the machine)");

    finishSetup (900, 640, 800, 600);
}

void MultiDelayEditor::updateParamLabels (int machine)
{
    const auto& info = mdelay::machines()[(size_t) machine];
    param1->label.setText (info.param1, juce::dontSendNotification);
    param2->label.setText (info.param2, juce::dontSendNotification);
    param1->slider.textFromValueFunction = [machine] (double v) { return juce::String (mdelay::paramText (machine, 1, v * 0.01)); };
    param2->slider.textFromValueFunction = [machine] (double v) { return juce::String (mdelay::paramText (machine, 2, v * 0.01)); };
    param1->slider.updateText();
    param2->slider.updateText();
}

void MultiDelayEditor::timerCallback()
{
    const auto p      = processor.currentParams();
    const int  sync   = processor.choice ("sync");
    const bool synced = sync > 0 && processor.hostBpm() > 0.0;
    timeKnob->slider.setEnabled (! synced);
    if (p.machine != shownMachine)
    {
        shownMachine = p.machine;
        updateParamLabels (shownMachine);
    }
    display.setState (p, processor.hostBpm(), synced,
                      mdelay::syncValues()[(size_t) juce::jlimit (0, (int) mdelay::syncValues().size() - 1, sync)].first);
}
