#include "PluginEditor.h"

using namespace melon;
using proeq::Placement;
using proeq::Type;

namespace
{
const juce::Colour bandPalette[] = { juce::Colour (0xff4fc3f7), juce::Colour (0xffffa657), juce::Colour (0xff7ee787),
                                     juce::Colour (0xffd2a8ff), juce::Colour (0xffff7b72), juce::Colour (0xfff2cc60),
                                     juce::Colour (0xff56d4dd), juce::Colour (0xffff9bce) };

juce::Colour bandColour (int band) { return bandPalette[band % 8]; }

constexpr double kDisplayLo = proeq::kMinFreq, kDisplayHi = proeq::kMaxFreq;
constexpr double kSpecTop = 0.0, kSpecBottom = -90.0; // analyzer scale (dBFS)
constexpr double kSpecTilt = 4.5;                     // dB/oct around 1 kHz, so music looks roughly flat
const double     kRanges[] = { 30.0, 12.0, 6.0, 3.0 };

juce::String freqLabel (double f)
{
    return f >= 1000.0 ? juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
}

const char* kHelp = "Double-click to add a band  |  drag a node to move it  |  mouse wheel = Q  |  "
                    "Shift = fine  |  Alt-click = delete  |  right-click for options";
} // namespace

//==============================================================================
int BandOps::create (Type type, float freq, float gainDb) const
{
    for (int b = 0; b < proeq::kNumBands; ++b)
    {
        if (isUsed (b))
            continue;
        const bool cut = type == Type::lowCut || type == Type::highCut;
        setWithGesture (b, "type", (float) (int) type);
        setWithGesture (b, "freq", freq);
        setWithGesture (b, "gain", proeq::hasGain (type) ? gainDb : 0.0f);
        setWithGesture (b, "q", cut || type == Type::lowShelf || type == Type::highShelf || type == Type::tiltShelf ? 0.71f : 1.0f);
        setWithGesture (b, "slope", 1.0f);
        setWithGesture (b, "stereo", 0.0f);
        setWithGesture (b, "dynrange", 0.0f);
        setWithGesture (b, "dynthr", -30.0f);
        setWithGesture (b, "on", 1.0f);
        setWithGesture (b, "used", 1.0f); // last, so the band appears with its final settings
        return b;
    }
    return -1;
}

void BandOps::remove (int band) const
{
    if (band < 0)
        return;
    if (proc.soloBand.load() == band)
        proc.soloBand = -1;
    setWithGesture (band, "used", 0.0f);
}

//==============================================================================
EqGraph::EqGraph (ProEqProcessor& p, BandOps& o) : proc (p), ops (o)
{
    fft.init (kFftSize);
    samples.assign ((size_t) kFftSize, 0.0f);
    fftBuf.assign ((size_t) kFftSize, {});
    window.resize ((size_t) kFftSize);
    double sum = 0.0;
    for (int i = 0; i < kFftSize; ++i)
    {
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * melon::kPi * i / kFftSize));
        sum += window[(size_t) i];
    }
    windowGain = (float) (2.0 / sum); // a full-scale sine reads 0 dB
    setRepaintsOnMouseActivity (false);
}

double EqGraph::rangeDb() const
{
    return kRanges[juce::jlimit (0, 3, (int) proc.apvts.state.getProperty (ProEqProcessor::kRangeProp, 1))];
}

float EqGraph::xForFreq (double f) const
{
    return plot.getX() + plot.getWidth() * (float) (std::log (f / kDisplayLo) / std::log (kDisplayHi / kDisplayLo));
}
double EqGraph::freqForX (float x) const
{
    return kDisplayLo * std::pow (kDisplayHi / kDisplayLo, (double) ((x - plot.getX()) / plot.getWidth()));
}
float EqGraph::yForDb (double db) const
{
    return plot.getCentreY() - (float) (db / rangeDb()) * plot.getHeight() * 0.5f;
}
double EqGraph::dbForY (float y) const
{
    return (double) ((plot.getCentreY() - y) / (plot.getHeight() * 0.5f)) * rangeDb();
}
float EqGraph::yForSpectrum (double db) const
{
    return juce::jmap ((float) juce::jlimit (kSpecBottom, kSpecTop, db), (float) kSpecBottom, (float) kSpecTop, plot.getBottom(), plot.getY());
}

void EqGraph::resized()
{
    plot = getLocalBounds().toFloat().reduced (8.0f).withTrimmedLeft (30.0f).withTrimmedRight (30.0f).withTrimmedBottom (14.0f).withTrimmedTop (6.0f);
    const int cols = juce::jmax (8, (int) plot.getWidth() / 2);
    preCols.assign ((size_t) cols, -200.0f);
    postCols.assign ((size_t) cols, -200.0f);
    colFreqLo.resize ((size_t) cols);
    colFreqHi.resize ((size_t) cols);
    for (int c = 0; c < cols; ++c)
    {
        colFreqLo[(size_t) c] = (float) freqForX (plot.getX() + plot.getWidth() * (float) c / (float) cols);
        colFreqHi[(size_t) c] = (float) freqForX (plot.getX() + plot.getWidth() * (float) (c + 1) / (float) cols);
    }
}

//==============================================================================
void EqGraph::refresh()
{
    params = proc.currentParams();
    fs     = proc.sampleRateForDisplay();
    for (int b = 0; b < proeq::kNumBands; ++b)
        dyn[(size_t) b] = proc.engine().dynamicOffsetDb (b);
    updateAnalyzer();
    repaint();
}

void EqGraph::updateAnalyzer()
{
    const int mode = (int) proc.apvts.state.getProperty (ProEqProcessor::kAnalyzerProp, 3);
    if (mode == 1 || mode == 3) computeSpectrum (proc.engine().preFifo(), lastPreWrite, preCols);
    if (mode == 2 || mode == 3) computeSpectrum (proc.engine().postFifo(), lastPostWrite, postCols);
}

void EqGraph::computeSpectrum (const proeq::AnalyzerFifo& fifo, int& lastWrite, std::vector<float>& cols)
{
    const int w = fifo.copyLatest (samples.data(), kFftSize);
    if (w == lastWrite) // no audio running: let the spectrum fall
    {
        for (auto& c : cols) c -= 2.0f;
        return;
    }
    lastWrite = w;

    for (int i = 0; i < kFftSize; ++i)
        fftBuf[(size_t) i] = samples[(size_t) i] * window[(size_t) i];
    fft.transform (fftBuf.data(), false);

    const double binHz = fs / kFftSize;
    auto magAt = [&] (double bin) {
        const int    b0 = juce::jlimit (0, kFftSize / 2 - 1, (int) bin);
        const double t  = juce::jlimit (0.0, 1.0, bin - b0);
        return (1.0 - t) * std::abs (fftBuf[(size_t) b0]) + t * std::abs (fftBuf[(size_t) (b0 + 1)]);
    };

    for (size_t c = 0; c < cols.size(); ++c)
    {
        const double lo = colFreqLo[c] / binHz, hi = colFreqHi[c] / binHz;
        double m = 0.0;
        if (hi - lo < 1.0)
            m = magAt (0.5 * (lo + hi));
        else
            for (int b = (int) std::ceil (lo); b <= (int) hi && b < kFftSize / 2; ++b)
                m = std::max (m, std::abs (fftBuf[(size_t) b]));
        const double f  = 0.5 * (colFreqLo[c] + colFreqHi[c]);
        double db = 20.0 * std::log10 (std::max (1.0e-9, m * windowGain)) + kSpecTilt * std::log2 (f / 1000.0);
        if (f > 0.5 * fs)
            db = -200.0;
        cols[c] = std::max ((float) db, cols[c] - 1.2f); // instant rise, ~36 dB/s fall
    }
}

//==============================================================================
juce::Point<float> EqGraph::nodePosition (int b) const
{
    const auto& band = params.bands[(size_t) b];
    const float x = xForFreq (band.freq);
    const float y = proeq::hasGain (band.type) ? yForDb (band.gainDb * params.gainScale + dyn[(size_t) b]) : yForDb (0.0);
    return { x, juce::jlimit (plot.getY() + 6.0f, plot.getBottom() - 6.0f, y) };
}

int EqGraph::nodeAt (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 11.0f;
    for (int b = 0; b < proeq::kNumBands; ++b)
        if (params.bands[(size_t) b].used)
        {
            const float d = nodePosition (b).getDistanceFrom (p);
            if (d < bestD || (d <= bestD && b == selected))
            {
                bestD = d;
                best  = b;
            }
        }
    return best;
}

void EqGraph::select (int band)
{
    selected = band;
    if (onSelect) onSelect (band);
    repaint();
}

void EqGraph::drawCurve (juce::Graphics& g, const std::vector<proeq::Cascade>& cascades, const std::vector<int>& bands,
                         juce::Colour colour, float thickness, bool fill)
{
    juce::Path path;
    const float step = 2.0f;
    bool first = true;
    for (float x = plot.getX(); x <= plot.getRight() + 0.5f; x += step)
    {
        const double f = freqForX (x);
        double db = 0.0;
        for (int b : bands)
            db += cascades[(size_t) b].magnitudeDb (f, fs);
        const float y = juce::jlimit (plot.getY() - 2.0f, plot.getBottom() + 2.0f, yForDb (db));
        if (first) path.startNewSubPath (x, y);
        else       path.lineTo (x, y);
        first = false;
    }
    if (fill)
    {
        juce::Path area (path);
        area.lineTo (plot.getRight(), yForDb (0.0));
        area.lineTo (plot.getX(), yForDb (0.0));
        area.closeSubPath();
        g.setColour (colour.withAlpha (0.18f));
        g.fillPath (area);
    }
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EqGraph::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    // --- grid ------------------------------------------------------------------
    g.setFont (juce::FontOptions (11.0f));
    for (double f : { 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0,
                      1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0, 10000.0, 20000.0 })
    {
        const bool major = f == 20.0 || f == 50.0 || f == 100.0 || f == 200.0 || f == 500.0 || f == 1000.0 || f == 2000.0
                           || f == 5000.0 || f == 10000.0 || f == 20000.0;
        const float x = xForFreq (f);
        g.setColour (colours::grid.withAlpha (major ? 1.0f : 0.45f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        if (major)
        {
            g.setColour (colours::dimText);
            g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 13.0f), juce::Justification::centred);
        }
    }
    const double range = rangeDb();
    const double stepDb = range >= 30.0 ? 6.0 : (range >= 12.0 ? 3.0 : (range >= 6.0 ? 1.0 : 0.5));
    for (double db = -range; db <= range + 1.0e-6; db += stepDb)
    {
        const float y = yForDb (db);
        g.setColour (std::abs (db) < 1.0e-6 ? colours::dimText.withAlpha (0.6f) : colours::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (colours::dimText);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db, stepDb < 1.0 ? 1 : 0), juce::Rectangle<float> (plot.getX() - 32.0f, y - 7.0f, 28.0f, 14.0f),
                    juce::Justification::right);
    }
    for (double db = -10.0; db >= kSpecBottom + 1.0; db -= 20.0) // analyzer scale (right side)
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (plot.getRight() + 4.0f, yForSpectrum (db) - 7.0f, 28.0f, 14.0f),
                    juce::Justification::left);

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    // --- analyzer ----------------------------------------------------------------
    const int mode = (int) proc.apvts.state.getProperty (ProEqProcessor::kAnalyzerProp, 3);
    auto drawSpectrum = [&] (const std::vector<float>& cols, juce::Colour fillColour, juce::Colour line) {
        juce::Path p;
        const float dx = plot.getWidth() / (float) cols.size();
        p.startNewSubPath (plot.getX(), plot.getBottom());
        for (size_t c = 0; c < cols.size(); ++c)
            p.lineTo (plot.getX() + dx * ((float) c + 0.5f), yForSpectrum (cols[c]));
        p.lineTo (plot.getRight(), plot.getBottom());
        p.closeSubPath();
        g.setColour (fillColour);
        g.fillPath (p);
        g.setColour (line);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    };
    if (mode == 1 || mode == 3) drawSpectrum (preCols, colours::dimText.withAlpha (0.12f), colours::dimText.withAlpha (0.25f));
    if (mode == 2 || mode == 3) drawSpectrum (postCols, colours::accent.withAlpha (0.10f), colours::accent.withAlpha (0.30f));

    // --- band curves ---------------------------------------------------------------
    std::vector<proeq::Cascade> live ((size_t) proeq::kNumBands), full ((size_t) proeq::kNumBands);
    std::vector<int> stereoBands;
    std::array<std::vector<int>, 5> byPlacement;
    bool any = false;
    for (int b = 0; b < proeq::kNumBands; ++b)
    {
        const auto& band = params.bands[(size_t) b];
        if (! band.used)
            continue;
        any = true;
        proeq::designBand (band, band.gainDb * params.gainScale + dyn[(size_t) b], fs, live[(size_t) b]);
        if (band.dynamic())
            proeq::designBand (band, (band.gainDb + band.dynRangeDb) * params.gainScale, fs, full[(size_t) b]);
        if (band.on)
            byPlacement[(size_t) band.placement].push_back (b);
    }

    for (int b = 0; b < proeq::kNumBands; ++b)
    {
        const auto& band = params.bands[(size_t) b];
        if (! band.used)
            continue;
        const auto col = band.on ? bandColour (b) : colours::dimText;
        if (band.dynamic() && band.on) // shaded dynamic range
        {
            proeq::Cascade st;
            proeq::designBandStatic (band, params.gainScale, fs, st);
            juce::Path area;
            std::vector<juce::Point<float>> lower;
            bool first = true;
            for (float x = plot.getX(); x <= plot.getRight() + 0.5f; x += 3.0f)
            {
                const double f = freqForX (x);
                const float y1 = yForDb (st.magnitudeDb (f, fs)), y2 = yForDb (full[(size_t) b].magnitudeDb (f, fs));
                if (first) area.startNewSubPath (x, y1); else area.lineTo (x, y1);
                lower.push_back ({ x, y2 });
                first = false;
            }
            for (auto it = lower.rbegin(); it != lower.rend(); ++it)
                area.lineTo (*it);
            area.closeSubPath();
            g.setColour (col.withAlpha (0.12f));
            g.fillPath (area);
        }
        drawCurve (g, live, { b }, col.withAlpha (b == selected ? 0.95f : 0.45f), b == selected ? 1.6f : 1.0f, b == selected && band.on);
    }

    // --- total curve(s) --------------------------------------------------------------
    // Left / Right / Mid / Side bands: one extra curve per placement (stereo bands + that placement's bands)
    const auto& st = byPlacement[(size_t) Placement::stereo];
    const char* names[] = { "", "Left", "Right", "Mid", "Side" };
    const juce::Colour placementColours[] = { colours::text, colours::second, juce::Colour (0xffff7b72), juce::Colour (0xffd2a8ff),
                                              juce::Colour (0xff7ee787) };
    float legendY = plot.getY() + 22.0f;
    for (int pl = 1; pl < 5; ++pl)
        if (! byPlacement[(size_t) pl].empty())
        {
            auto bands = st;
            bands.insert (bands.end(), byPlacement[(size_t) pl].begin(), byPlacement[(size_t) pl].end());
            const auto c = placementColours[pl];
            drawCurve (g, live, bands, c.withAlpha (0.85f), 1.6f, false);
            g.setColour (c);
            g.fillRect (juce::Rectangle<float> (plot.getRight() - 64.0f, legendY + 6.0f, 14.0f, 2.0f));
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (names[pl], juce::Rectangle<float> (plot.getRight() - 46.0f, legendY, 44.0f, 14.0f), juce::Justification::left, false);
            legendY += 15.0f;
        }
    drawCurve (g, live, st, colours::text, 2.2f, false);

    // --- nodes -----------------------------------------------------------------------
    for (int b = 0; b < proeq::kNumBands; ++b)
    {
        const auto& band = params.bands[(size_t) b];
        if (! band.used)
            continue;
        const auto pos = nodePosition (b);
        const auto col = band.on ? bandColour (b) : colours::dimText;
        const float r  = b == selected ? 9.0f : 7.5f;
        g.setColour (colours::background.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f + 4.0f, r * 2.0f + 4.0f).withCentre (pos));
        g.setColour (band.on ? col : col.withAlpha (0.6f));
        if (band.on) g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos));
        else         g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos), 1.5f);
        if (b == selected || b == hover)
        {
            g.setColour (colours::text);
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f + 5.0f, r * 2.0f + 5.0f).withCentre (pos), 1.3f);
        }
        if (band.dynamic() && band.on) // dynamic range marker
        {
            const float y2 = juce::jlimit (plot.getY(), plot.getBottom(), yForDb ((band.gainDb + band.dynRangeDb) * params.gainScale));
            g.setColour (col.withAlpha (0.8f));
            g.drawLine (pos.x, pos.y, pos.x, y2, 1.5f);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ pos.x, y2 }));
        }
        g.setColour (band.on ? colours::background : col);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos), juce::Justification::centred);
        if (proc.soloBand.load() == b)
        {
            g.setColour (colours::second);
            g.drawText ("SOLO", juce::Rectangle<float> (40.0f, 12.0f).withCentre (pos.translated (0.0f, -r - 10.0f)), juce::Justification::centred);
        }
    }

    // --- readout of the hovered / dragged band ------------------------------------------------
    const int info = dragBand >= 0 ? dragBand : hover;
    if (info >= 0 && params.bands[(size_t) info].used)
    {
        const auto& band = params.bands[(size_t) info];
        juce::String text = ProEqProcessor::typeNames()[(int) band.type] + "  " + freqLabel (band.freq);
        if (proeq::hasGain (band.type))
            text << "  " << (band.gainDb > 0 ? "+" : "") << juce::String (band.gainDb, 1) << " dB";
        text << "  Q " << juce::String (band.q, 2);
        if (proeq::usesSlope (band.type))
            text << "  " << proeq::kSlopes[band.slope] << " dB/oct";
        if (band.placement != Placement::stereo)
            text << "  " << ProEqProcessor::placementNames()[(int) band.placement];
        const auto pos = nodePosition (info);
        g.setFont (juce::FontOptions (12.0f));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 16.0f;
        auto box = juce::Rectangle<float> (w, 20.0f).withCentre (pos.translated (0.0f, pos.y > plot.getCentreY() ? -28.0f : 28.0f));
        box = box.constrainedWithin (plot.reduced (2.0f));
        g.setColour (colours::background.withAlpha (0.9f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (bandColour (info));
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (colours::text);
        g.drawText (text, box, juce::Justification::centred);
    }

    g.restoreState();

    if (! any)
    {
        g.setColour (colours::dimText);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Double-click anywhere to add a band", plot.toNearestInt(), juce::Justification::centred);
    }
    g.setColour (colours::dimText.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (kHelp, plot.withHeight (14.0f).translated (0.0f, 2.0f).toNearestInt(), juce::Justification::centredTop);
}

//==============================================================================
void EqGraph::mouseMove (const juce::MouseEvent& e)
{
    const int h = nodeAt (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EqGraph::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void EqGraph::mouseDown (const juce::MouseEvent& e)
{
    params = proc.currentParams();
    const int b = nodeAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (b >= 0)
        {
            select (b);
            showBandMenu (b);
        }
        else
        {
            showEmptyMenu (e.position);
        }
        return;
    }
    if (b < 0)
    {
        select (-1);
        return;
    }
    if (e.mods.isAltDown() || e.mods.isMiddleButtonDown())
    {
        ops.remove (b);
        select (-1);
        return;
    }

    select (b);
    dragBand  = b;
    dragStart = e.position;
    startFreq = ops.get (b, "freq");
    startGain = ops.get (b, "gain");
    startQ    = ops.get (b, "q");
    ops.begin (b, "freq");
    ops.begin (b, proeq::hasGain (params.bands[(size_t) b].type) ? "gain" : "q");
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0)
        return;
    const auto&  band  = params.bands[(size_t) dragBand];
    const double fine  = e.mods.isShiftDown() ? 0.15 : 1.0;
    const auto   delta = e.position - dragStart;

    const double octaves = (double) delta.x / plot.getWidth() * std::log2 (kDisplayHi / kDisplayLo) * fine;
    ops.set (dragBand, "freq", (float) juce::jlimit (proeq::kMinFreq, proeq::kMaxFreq, startFreq * std::pow (2.0, octaves)));

    if (proeq::hasGain (band.type))
    {
        const double dbPerPixel = 2.0 * rangeDb() / plot.getHeight();
        const double scale      = std::abs (params.gainScale) > 0.05 ? params.gainScale : 1.0;
        ops.set (dragBand, "gain", (float) juce::jlimit (-30.0, 30.0, startGain - delta.y * dbPerPixel * fine / scale));
    }
    else // cuts, notch, band pass: vertical drag = Q
    {
        ops.set (dragBand, "q", (float) juce::jlimit (proeq::kMinQ, proeq::kMaxQ, startQ * std::pow (2.0, -delta.y / 60.0 * fine)));
    }
    refresh();
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragBand < 0)
        return;
    ops.end (dragBand, "freq");
    ops.end (dragBand, proeq::hasGain (params.bands[(size_t) dragBand].type) ? "gain" : "q");
    dragBand = -1;
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! plot.contains (e.position) || e.mods.isPopupMenu())
        return;
    params = proc.currentParams();
    const int b = nodeAt (e.position);
    if (b >= 0) // double-click a node: switch it on / off
    {
        ops.setWithGesture (b, "on", ops.get (b, "on") > 0.5f ? 0.0f : 1.0f);
        return;
    }
    const double f = freqForX (e.position.x);
    const Type type = f < 30.0 ? Type::lowCut : (f > 15000.0 ? Type::highCut : Type::bell);
    const double scale = std::abs (params.gainScale) > 0.05 ? params.gainScale : 1.0;
    const int nb = ops.create (type, (float) f, (float) juce::jlimit (-30.0, 30.0, dbForY (e.position.y) / scale));
    if (nb >= 0)
        select (nb);
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = nodeAt (e.position);
    if (b < 0)
        return;
    const float q = ops.get (b, "q") * std::exp ((w.isReversed ? -w.deltaY : w.deltaY) * 1.5f);
    ops.setWithGesture (b, "q", juce::jlimit ((float) proeq::kMinQ, (float) proeq::kMaxQ, q));
    select (b);
    params = proc.currentParams();
}

void EqGraph::showBandMenu (int b)
{
    const auto& band = params.bands[(size_t) b];
    juce::PopupMenu types, slopes, places, menu;
    for (int i = 0; i < proeq::kNumTypes; ++i)
        types.addItem (100 + i, ProEqProcessor::typeNames()[i], true, (int) band.type == i);
    for (int i = 0; i < proeq::kNumSlopes; ++i)
        slopes.addItem (200 + i, ProEqProcessor::slopeNames()[i], true, band.slope == i);
    for (int i = 0; i < 5; ++i)
        places.addItem (300 + i, ProEqProcessor::placementNames()[i], true, (int) band.placement == i);

    menu.addSectionHeader ("Band " + juce::String (b + 1));
    menu.addSubMenu ("Type", types);
    menu.addSubMenu ("Slope", slopes, proeq::usesSlope (band.type));
    menu.addSubMenu ("Stereo Placement", places);
    menu.addSeparator();
    menu.addItem (1, "Enabled", true, band.on);
    menu.addItem (2, "Solo", true, proc.soloBand.load() == b);
    menu.addItem (3, "Make Dynamic (-6 dB range)", proeq::hasGain (band.type) && ! band.dynamic());
    menu.addItem (4, "Make Static", band.dynamic());
    menu.addSeparator();
    menu.addItem (5, "Delete");

    juce::Component::SafePointer<EqGraph> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, b] (int r) {
        if (safe == nullptr || r == 0)
            return;
        auto& o = safe->ops;
        if (r >= 300)      o.setWithGesture (b, "stereo", (float) (r - 300));
        else if (r >= 200) o.setWithGesture (b, "slope", (float) (r - 200));
        else if (r >= 100) o.setWithGesture (b, "type", (float) (r - 100));
        else if (r == 1)   o.setWithGesture (b, "on", o.get (b, "on") > 0.5f ? 0.0f : 1.0f);
        else if (r == 2)   safe->proc.soloBand = safe->proc.soloBand.load() == b ? -1 : b;
        else if (r == 3)   o.setWithGesture (b, "dynrange", -6.0f);
        else if (r == 4)   o.setWithGesture (b, "dynrange", 0.0f);
        else if (r == 5)   { o.remove (b); safe->select (-1); }
        safe->refresh();
    });
}

void EqGraph::showEmptyMenu (juce::Point<float> p)
{
    if (! plot.contains (p))
        return;
    const double f  = freqForX (p.x);
    const double db = dbForY (p.y);
    juce::PopupMenu menu;
    menu.addSectionHeader ("Add band at " + freqLabel (f));
    for (int i = 0; i < proeq::kNumTypes; ++i)
        menu.addItem (100 + i, ProEqProcessor::typeNames()[i]);

    juce::Component::SafePointer<EqGraph> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, f, db] (int r) {
        if (safe == nullptr || r < 100)
            return;
        const double scale = std::abs (safe->params.gainScale) > 0.05 ? safe->params.gainScale : 1.0;
        const int nb = safe->ops.create ((Type) (r - 100), (float) f, (float) juce::jlimit (-30.0, 30.0, db / scale));
        if (nb >= 0)
            safe->select (nb);
    });
}

//==============================================================================
BandPanel::BandPanel (ProEqProcessor& p, BandOps& o) : proc (p), ops (o)
{
    title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    title.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (title);

    hint.setText ("No band selected.\nDouble-click the display to add one, or click a node to edit it.", juce::dontSendNotification);
    hint.setJustificationType (juce::Justification::centred);
    hint.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (hint);

    globalTitle.setText ("Global", juce::dontSendNotification);
    globalTitle.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    globalTitle.setColour (juce::Label::textColourId, colours::accent);
    globalTitle.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (globalTitle);

    for (auto* b : { &prev, &next })
        addAndMakeVisible (*b);
    prev.setTooltip ("Previous band");
    next.setTooltip ("Next band");
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (+1); };

    del.setTooltip ("Delete this band");
    del.onClick = [this] {
        const int b = band;
        ops.remove (b);
        if (onSelect) onSelect (-1);
    };
    addAndMakeVisible (del);

    onButton.setTooltip ("Switch the band on or off (double-click its node does the same)");
    addAndMakeVisible (onButton);
    soloButton.setTooltip ("Listen only to the region of the spectrum this band works on");
    soloButton.onClick = [this] { proc.soloBand = soloButton.getToggleState() ? band : -1; };
    addAndMakeVisible (soloButton);

    setupCombo (typeBox, ProEqProcessor::typeNames(), "Filter type");
    setupCombo (slopeBox, ProEqProcessor::slopeNames(), "Steepness of cuts, band pass and shelves (shelves: 6 dB/oct or 12 dB/oct)");
    setupCombo (stereoBox, ProEqProcessor::placementNames(), "Which part of the stereo signal the band works on");

    setupKnob (freq, "Frequency", "Centre / corner frequency");
    setupKnob (gain, "Gain", "Boost or cut (bell, shelves and tilt)");
    setupKnob (q, "Q", "Bandwidth. On cuts it sets the resonance (0.71 = no bump)");
    setupKnob (dynRange, "Dyn Range", "Dynamic EQ: how far the gain moves when the input goes above the threshold "
                                      "(negative = compress the band, positive = expand it, 0 = static band)");
    setupKnob (dynThr, "Threshold", "Dynamic EQ: level of the input around this band's frequency where the dynamics start");
    setupKnob (scale, "Gain Scale", "Scales the gain of every band (100 % = as set, 0 % = flat, -100 % = inverted)");
    setupKnob (attack, "Attack", "Dynamic EQ: how fast dynamic bands react");
    setupKnob (release, "Release", "Dynamic EQ: how fast dynamic bands recover");
    scale.attachment   = std::make_unique<SliderAttachment> (proc.apvts, "gainscale", scale.slider);
    attack.attachment  = std::make_unique<SliderAttachment> (proc.apvts, "attack", attack.slider);
    release.attachment = std::make_unique<SliderAttachment> (proc.apvts, "release", release.slider);
    scale.slider.setDoubleClickReturnValue (true, 100.0);
    attack.slider.setDoubleClickReturnValue (true, 5.0);
    release.slider.setDoubleClickReturnValue (true, 120.0);

    setBand (-1);
}

void BandPanel::setupKnob (Knob& k, const juce::String& name, const juce::String& tooltip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, colours::grid);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::background.withAlpha (0.6f));
    k.slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setTooltip (tooltip);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setTooltip (tooltip);
    addAndMakeVisible (k.label);
}

void BandPanel::setupCombo (juce::ComboBox& c, const juce::StringArray& items, const juce::String& tooltip)
{
    c.addItemList (items, 1);
    c.setJustificationType (juce::Justification::centred);
    c.setTooltip (tooltip);
    addAndMakeVisible (c);
}

void BandPanel::step (int delta)
{
    // next / previous band in use
    for (int i = 1; i <= proeq::kNumBands; ++i)
    {
        const int b = ((band < 0 ? (delta > 0 ? -1 : 0) : band) + delta * i + proeq::kNumBands * 4) % proeq::kNumBands;
        if (ops.isUsed (b))
        {
            if (onSelect) onSelect (b);
            return;
        }
    }
}

void BandPanel::setBand (int b)
{
    if (b == band && (b < 0 || freq.attachment != nullptr))
        return;
    if (proc.soloBand.load() >= 0 && proc.soloBand.load() != b)
        proc.soloBand = -1; // solo follows the band being edited

    band = b;
    onAttachment.reset();
    typeAttachment.reset();
    slopeAttachment.reset();
    stereoAttachment.reset();
    for (auto* k : { &freq, &gain, &q, &dynRange, &dynThr })
        k->attachment.reset();

    const bool show = b >= 0;
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &title, &del, &onButton, &soloButton, &typeBox, &slopeBox, &stereoBox, &freq.slider, &freq.label, &gain.slider,
             &gain.label, &q.slider, &q.label, &dynRange.slider, &dynRange.label, &dynThr.slider, &dynThr.label })
        c->setVisible (show);
    hint.setVisible (! show);
    if (! show)
        return;

    auto& s = proc.apvts;
    title.setText ("Band " + juce::String (b + 1), juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, bandPalette[b % 8]);
    onAttachment     = std::make_unique<ButtonAttachment> (s, ProEqProcessor::bandId (b, "on"), onButton);
    typeAttachment   = std::make_unique<ComboAttachment> (s, ProEqProcessor::bandId (b, "type"), typeBox);
    slopeAttachment  = std::make_unique<ComboAttachment> (s, ProEqProcessor::bandId (b, "slope"), slopeBox);
    stereoAttachment = std::make_unique<ComboAttachment> (s, ProEqProcessor::bandId (b, "stereo"), stereoBox);
    freq.attachment     = std::make_unique<SliderAttachment> (s, ProEqProcessor::bandId (b, "freq"), freq.slider);
    gain.attachment     = std::make_unique<SliderAttachment> (s, ProEqProcessor::bandId (b, "gain"), gain.slider);
    q.attachment        = std::make_unique<SliderAttachment> (s, ProEqProcessor::bandId (b, "q"), q.slider);
    dynRange.attachment = std::make_unique<SliderAttachment> (s, ProEqProcessor::bandId (b, "dynrange"), dynRange.slider);
    dynThr.attachment   = std::make_unique<SliderAttachment> (s, ProEqProcessor::bandId (b, "dynthr"), dynThr.slider);
    freq.slider.setDoubleClickReturnValue (true, 1000.0);
    gain.slider.setDoubleClickReturnValue (true, 0.0);
    q.slider.setDoubleClickReturnValue (true, 1.0);
    dynRange.slider.setDoubleClickReturnValue (true, 0.0);
    dynThr.slider.setDoubleClickReturnValue (true, -30.0);
    refresh();
}

void BandPanel::refresh()
{
    if (band >= 0 && ! ops.isUsed (band)) // deleted (e.g. by automation or undo)
    {
        if (onSelect) onSelect (-1);
        return;
    }
    soloButton.setToggleState (band >= 0 && proc.soloBand.load() == band, juce::dontSendNotification);
    if (band < 0)
        return;
    const auto t = (Type) juce::roundToInt (ops.get (band, "type"));
    const int  slope = juce::roundToInt (ops.get (band, "slope"));
    gain.slider.setEnabled (proeq::hasGain (t));
    slopeBox.setEnabled (proeq::usesSlope (t));
    q.slider.setEnabled (! ((t == Type::lowCut || t == Type::highCut || t == Type::lowShelf || t == Type::highShelf || t == Type::tiltShelf) && slope == 0));
    dynRange.slider.setEnabled (proeq::hasGain (t));
    dynThr.slider.setEnabled (proeq::hasGain (t) && std::abs (ops.get (band, "dynrange")) > 0.05f);
}

void BandPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (colours::grid);
    g.fillRect (globalArea.getX() - 8, globalArea.getY() + 4, 1, globalArea.getHeight() - 8);
    if (band >= 0)
    {
        g.setColour (bandPalette[band % 8]);
        g.fillRoundedRectangle (getLocalBounds().toFloat().removeFromLeft (4.0f).reduced (0.0f, 10.0f), 2.0f);
    }
}

void BandPanel::layoutKnob (Knob& k, juce::Rectangle<int> r)
{
    k.label.setBounds (r.removeFromTop (18));
    k.slider.setBounds (r);
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);

    // right: global section
    const int knobW = juce::jlimit (70, 96, r.getWidth() / 11);
    globalArea = r.removeFromRight (knobW * 3);
    {
        auto gArea = globalArea;
        globalTitle.setBounds (gArea.removeFromTop (14));
        layoutKnob (scale, gArea.removeFromLeft (knobW));
        layoutKnob (attack, gArea.removeFromLeft (knobW));
        layoutKnob (release, gArea.removeFromLeft (knobW));
    }
    r.removeFromRight (16);

    hint.setBounds (r);

    // left: band selector and switches
    auto left = r.removeFromLeft (150);
    auto row = left.removeFromTop (28);
    prev.setBounds (row.removeFromLeft (28));
    next.setBounds (row.removeFromRight (28));
    title.setBounds (row);
    left.removeFromTop (6);
    row = left.removeFromTop (26);
    onButton.setBounds (row.removeFromLeft (70));
    soloButton.setBounds (row);
    left.removeFromTop (6);
    del.setBounds (left.removeFromTop (26).reduced (0, 0));
    r.removeFromLeft (12);

    // combos
    auto combos = r.removeFromLeft (150);
    const int ch = juce::jmin (28, (combos.getHeight() - 12) / 3);
    typeBox.setBounds (combos.removeFromTop (ch));
    combos.removeFromTop (6);
    slopeBox.setBounds (combos.removeFromTop (ch));
    combos.removeFromTop (6);
    stereoBox.setBounds (combos.removeFromTop (ch));
    r.removeFromLeft (12);

    // band knobs
    const int w = r.getWidth() / 5;
    for (auto* k : { &freq, &gain, &q, &dynRange, &dynThr })
        layoutKnob (*k, r.removeFromLeft (w));
}

//==============================================================================
ProEqMain::ProEqMain (ProEqProcessor& p) : proc (p), ops (p), graph (p, ops), panel (p, ops)
{
    addAndMakeVisible (graph);
    addAndMakeVisible (panel);
    graph.onSelect = [this] (int b) { select (b); };
    panel.onSelect = [this] (int b) { select (b); };

    const int saved = (int) proc.apvts.state.getProperty (ProEqProcessor::kSelectedProp, -1);
    select (ops.isUsed (saved) ? saved : -1);
}

void ProEqMain::select (int b)
{
    proc.apvts.state.setProperty (ProEqProcessor::kSelectedProp, b, nullptr);
    graph.setSelected (b);
    panel.setBand (b);
}

void ProEqMain::refresh()
{
    graph.refresh();
    panel.refresh();
}

void ProEqMain::resized()
{
    auto r = getLocalBounds();
    panel.setBounds (r.removeFromBottom (juce::jlimit (130, 160, r.getHeight() / 4)));
    r.removeFromBottom (8);
    graph.setBounds (r);
}

//==============================================================================
ProEqEditor::ProEqEditor (ProEqProcessor& p) : EditorBase (p, "Pro EQ"), main (p)
{
    titleWidth = 120;
    setDisplay (main);

    addChoiceBox ("phase", 140, false,
                  "Zero Latency: minimum-phase filters, no delay. "
                  "Linear Phase: no phase shift at all, at the cost of latency (good for mastering and parallel processing)");
    resolutionBox = &addChoiceBox ("resolution", 100, false,
                                   "Linear Phase resolution: higher = more precise low frequencies, more latency");
    addToggle ("autogain", "Auto Gain", 100, "Compensates the output level for the overall boost or cut of the curve");

    addRowLabel ("Analyzer", 66);
    analyzerBox.addItemList ({ "Off", "Pre", "Post", "Pre + Post" }, 1);
    analyzerBox.setTooltip ("Spectrum analyzer: input (Pre, grey), output (Post, blue) or both. Tilted +4.5 dB/oct");
    analyzerBox.setSelectedItemIndex ((int) p.apvts.state.getProperty (ProEqProcessor::kAnalyzerProp, 3), juce::dontSendNotification);
    analyzerBox.onChange = [this] { processor.apvts.state.setProperty (ProEqProcessor::kAnalyzerProp, analyzerBox.getSelectedItemIndex(), nullptr); };
    addRowItem (analyzerBox, 110);

    addRowLabel ("Range", 50);
    rangeBox.addItemList ({ "+/- 30 dB", "+/- 12 dB", "+/- 6 dB", "+/- 3 dB" }, 1);
    rangeBox.setTooltip ("Vertical zoom of the EQ curve");
    rangeBox.setSelectedItemIndex ((int) p.apvts.state.getProperty (ProEqProcessor::kRangeProp, 1), juce::dontSendNotification);
    rangeBox.onChange = [this] { processor.apvts.state.setProperty (ProEqProcessor::kRangeProp, rangeBox.getSelectedItemIndex(), nullptr); };
    addRowItem (rangeBox, 100);

    latencyLabel.setJustificationType (juce::Justification::centredRight);
    latencyLabel.setColour (juce::Label::textColourId, colours::dimText);
    addRowItem (latencyLabel, -1);

    finishSetup (1120, 740, 980, 640);
}

void ProEqEditor::timerCallback()
{
    main.refresh();

    const bool linear = processor.apvts.getRawParameterValue ("phase")->load() > 0.5f;
    if (resolutionBox != nullptr)
        resolutionBox->setEnabled (linear);
    const int lat = processor.getLatencySamples();
    latencyLabel.setText (lat > 0 ? "Latency " + juce::String (1000.0 * lat / processor.sampleRateForDisplay(), 1) + " ms (" + juce::String (lat) + " samples)"
                                  : juce::String ("Zero latency"),
                          juce::dontSendNotification);

    // the session may have been reloaded: keep the interface-only boxes in sync
    analyzerBox.setSelectedItemIndex ((int) processor.apvts.state.getProperty (ProEqProcessor::kAnalyzerProp, 3), juce::dontSendNotification);
    rangeBox.setSelectedItemIndex ((int) processor.apvts.state.getProperty (ProEqProcessor::kRangeProp, 1), juce::dontSendNotification);
}
