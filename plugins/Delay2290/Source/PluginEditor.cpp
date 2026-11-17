#include "PluginEditor.h"

using namespace melon;

namespace
{
const juce::Colour ledOn  { 0xffff3b30 };
const juce::Colour ledOff { 0xff3a1512 };

// Seven-segment digit (segments a..g) drawn into `r`
void drawDigit (juce::Graphics& g, juce::Rectangle<float> r, int digit, bool dot)
{
    static const int seg[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
    const int   bits = digit >= 0 && digit <= 9 ? seg[digit] : 0;
    const float t = r.getWidth() * 0.16f, w = r.getWidth(), h = r.getHeight(), x = r.getX(), y = r.getY();
    const juce::Rectangle<float> segs[7] = {
        { x + t, y, w - 2 * t, t },                     // a
        { x + w - t, y + t, t, h / 2 - 1.5f * t },     // b
        { x + w - t, y + h / 2 + 0.5f * t, t, h / 2 - 1.5f * t }, // c
        { x + t, y + h - t, w - 2 * t, t },             // d
        { x, y + h / 2 + 0.5f * t, t, h / 2 - 1.5f * t }, // e
        { x, y + t, t, h / 2 - 1.5f * t },             // f
        { x + t, y + h / 2 - t / 2, w - 2 * t, t },     // g
    };
    for (int i = 0; i < 7; ++i)
    {
        g.setColour ((bits >> i) & 1 ? ledOn : ledOff);
        g.fillRoundedRectangle (segs[i].reduced (0.6f), t * 0.3f);
    }
    g.setColour (dot ? ledOn : ledOff);
    g.fillEllipse (x + w + t * 0.4f, y + h - t, t, t);
}
} // namespace

void DelayDisplay::setState (const delay2290::Params& p, double hostBpm, double duckDb, bool isSynced, const juce::String& name)
{
    params = p; bpm = hostBpm; duck = duckDb; synced = isSynced; syncName = name;
    repaint();
}

void DelayDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto left   = bounds.removeFromLeft (juce::jmin (300.0f, bounds.getWidth() * 0.4f));
    bounds.removeFromLeft (10.0f);

    // --- LED readout ---------------------------------------------------------
    {
        auto area = drawDisplayPanel (g, left, "Delay time");
        auto led  = area.reduced (6.0f).withSizeKeepingCentre (area.getWidth() - 12.0f, juce::jmin (area.getHeight() - 30.0f, 70.0f));
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (led.expanded (6.0f), 4.0f);

        const int  ms = juce::jlimit (0, 9999, juce::roundToInt (params.timeMs));
        const float dw = led.getHeight() * 0.5f, gap = dw * 0.45f;
        const float total = 4 * dw + 3 * gap;
        float x = led.getCentreX() - total * 0.5f;
        const int digits[4] = { ms / 1000, (ms / 100) % 10, (ms / 10) % 10, ms % 10 };
        bool lead = true;
        for (int i = 0; i < 4; ++i)
        {
            const bool blank = lead && digits[i] == 0 && i < 3;
            lead = blank;
            drawDigit (g, { x, led.getY(), dw, led.getHeight() }, blank ? -1 : digits[i], false);
            x += dw + gap;
        }

        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (colours::dimText);
        const auto sub = juce::String ("ms") + (synced ? "   SYNC " + syncName : "")
                         + (bpm > 0.0 ? "   " + juce::String (bpm, 1) + " BPM" : "");
        g.drawText (sub, area.removeFromBottom (16.0f).toNearestInt(), juce::Justification::centred);
    }

    // --- echo pattern ----------------------------------------------------------
    {
        auto plot = drawDisplayPanel (g, bounds, "Repeats").withTrimmedBottom (14.0f);
        auto duckBar = plot.removeFromRight (14.0f);
        plot.removeFromRight (8.0f);

        const double tL = params.timeMs;
        const double tR = params.mode == delay2290::Mode::stereo ? tL * (1.0 + delay2290::kMaxSpread * params.spread) : tL;
        const double span = std::max (200.0, tL * 8.5);
        auto xFor = [&] (double ms) { return plot.getX() + plot.getWidth() * (float) (ms / span); };
        const float mid = plot.getCentreY(), half = plot.getHeight() * 0.5f - 2.0f;

        g.setColour (colours::grid);
        g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());
        g.setFont (11.0f);
        const double step = span > 8000 ? 2000 : span > 4000 ? 1000 : span > 1500 ? 500 : span > 600 ? 200 : 100;
        for (double t = step; t < span; t += step)
        {
            g.setColour (colours::grid);
            g.drawVerticalLine ((int) xFor (t), plot.getY(), plot.getBottom());
            g.setColour (colours::dimText);
            g.drawText (t >= 1000 ? juce::String (t / 1000.0, 1) + " s" : juce::String ((int) t) + " ms",
                        (int) xFor (t) - 26, (int) plot.getBottom() + 2, 52, 12, juce::Justification::centred);
        }

        const float alpha = params.bypassed ? 0.35f : 1.0f;
        g.setColour (colours::text.withAlpha (alpha));
        g.fillRect (juce::Rectangle<float> (xFor (0.0), mid - half, 2.0f, 2.0f * half));

        auto bar = [&] (double t, double amp, bool up, juce::Colour c) {
            if (t > span || amp < 0.01) return;
            const float h = half * (float) amp;
            g.setColour (c.withAlpha (alpha));
            g.fillRect (juce::Rectangle<float> (xFor (t) - 1.5f, up ? mid - h : mid, 3.0f, h));
        };
        const double fb = params.feedback;
        for (int k = 1; k < 64; ++k)
        {
            const double amp = std::pow (fb, k - 1);
            switch (params.mode)
            {
                case delay2290::Mode::mono:     bar (k * tL, amp, true, colours::accent); bar (k * tL, amp, false, colours::accent); break;
                case delay2290::Mode::stereo:   bar (k * tL, amp, true, colours::accent); bar (k * tR, amp, false, colours::second); break;
                case delay2290::Mode::pingPong: bar (k * tL, amp, (k & 1) != 0, (k & 1) ? colours::accent : colours::second); break;
            }
            if (fb <= 0.0) break;
        }
        g.setColour (colours::dimText);
        g.drawText ("L", (int) plot.getX() + 4, (int) plot.getY(), 12, 14, juce::Justification::left);
        g.drawText ("R", (int) plot.getX() + 4, (int) plot.getBottom() - 14, 12, 14, juce::Justification::left);

        // ducking meter (0 .. -40 dB)
        g.setColour (colours::grid);
        g.fillRoundedRectangle (duckBar, 3.0f);
        const float frac = (float) juce::jlimit (0.0, 1.0, -duck / 40.0);
        g.setColour (colours::second);
        g.fillRoundedRectangle (duckBar.withHeight (duckBar.getHeight() * frac), 3.0f);
        g.setColour (colours::dimText);
        g.drawText ("Duck", duckBar.withY (duckBar.getBottom() + 1.0f).withHeight (12.0f).expanded (12.0f, 0.0f).toNearestInt(),
                    juce::Justification::centred);
    }
}

//==============================================================================
Delay2290Editor::Delay2290Editor (Delay2290Processor& p) : EditorBase (p, "Delay 2290")
{
    titleWidth = 140;
    setDisplay (display);

    auto& tapButton = makeButton ("TAP", "Tap the tempo: click 2 or more times in rhythm. Sets Time and turns Sync off");
    tapButton.onClick = [this] { processor.tap(); };
    addRowItem (tapButton, 80);
    addRowLabel ("Sync", 40);
    addChoiceBox ("sync", 110, false, "Sync the delay to the host tempo (Off = use the Time knob)");
    addRowLabel ("Mode", 46);
    addChoiceBox ("mode", 130, false, "Mono, Stereo (right delay longer by Spread) or Ping-pong (repeats bounce between left and right)");

    timeKnob = &addKnob ("time", "Time", "Delay time (ignored while Sync is on)");
    addKnob ("feedback", "Feedback", "Number of repeats. 100 % = (almost) endless");
    addKnob ("mix", "Mix", "Dry/delay balance. 50 % = both at full level; above 50 % the dry fades out");
    addKnob ("hicut", "Hi Cut", "Low-pass on the repeats (inside the feedback loop: each repeat gets darker)");
    addKnob ("locut", "Lo Cut", "High-pass on the repeats (inside the feedback loop: each repeat gets thinner)");
    addKnob ("spread", "Spread", "Stereo mode: makes the right delay up to 50 % longer, for width");
    addKnob ("moddepth", "Mod Depth", "Modulation of the delay time: subtle chorus to deep warble");
    addKnob ("modspeed", "Mod Speed", "Speed of the modulation");
    addKnob ("duckthresh", "Duck Thresh", "Dynamic delay: input level above which the repeats are turned down");
    addKnob ("duckamount", "Duck Amount", "Dynamic delay: how much the repeats are turned down while you play (0 = off)");
    addKnob ("duckrelease", "Duck Release", "Dynamic delay: how fast the repeats come back when you stop playing");

    finishSetup (860, 640, 760, 580);
}

void Delay2290Editor::timerCallback()
{
    const auto p      = processor.currentParams();
    const int  sync   = processor.choice ("sync");
    const bool synced = sync > 0 && processor.hostBpm() > 0.0;
    timeKnob->slider.setEnabled (! synced);
    display.setState (p, processor.hostBpm(), processor.duckDb(), synced,
                      delay2290::syncValues()[(size_t) juce::jlimit (0, (int) delay2290::syncValues().size() - 1, sync)].first);
}
