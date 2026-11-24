// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp
#include "TremDsp.h"
#include <cstdio>
#include <vector>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

// Runs a constant 1.0 input through the engine: the output is the tremolo gain itself.
static std::vector<std::vector<float>> run (const trem::Params& p, double fs, double seconds, int nch = 1)
{
    const size_t len = (size_t) (seconds * fs);
    std::vector<std::vector<float>> out ((size_t) nch, std::vector<float> (len, 1.0f));
    trem::Engine e;
    e.prepare (fs, nch);
    e.setParams (p);
    e.snapToTargets();
    for (size_t pos = 0; pos < len; pos += 256)
    {
        float* blk[2] = { out[0].data() + pos, out[(size_t) nch - 1].data() + pos };
        e.process (blk, nch, (int) std::min<size_t> (256, len - pos));
    }
    return out;
}

static float minOf (const std::vector<float>& v, size_t from) { float m = 10; for (size_t i = from; i < v.size(); ++i) m = std::min (m, v[i]); return m; }
static float maxOf (const std::vector<float>& v, size_t from) { float m = -10; for (size_t i = from; i < v.size(); ++i) m = std::max (m, v[i]); return m; }

// Cycles per second, counted from upward crossings of the middle of the swing
static double measuredRate (const std::vector<float>& v, double fs, size_t from)
{
    const float mid = 0.5f * (minOf (v, from) + maxOf (v, from));
    int ups = 0; size_t first = 0, last = 0;
    for (size_t i = from + 1; i < v.size(); ++i)
        if (v[i - 1] < mid && v[i] >= mid) { if (ups == 0) first = i; last = i; ++ups; }
    return ups > 1 ? (ups - 1) * fs / (double) (last - first) : 0.0;
}

// Share of the time the gain sits in the top or bottom 10 % of its swing (square = most of the time)
static double timeAtExtremes (const std::vector<float>& v, size_t from)
{
    const float lo = minOf (v, from), hi = maxOf (v, from), r = hi - lo;
    size_t n = 0;
    for (size_t i = from; i < v.size(); ++i)
        if (v[i] < lo + 0.1f * r || v[i] > hi - 0.1f * r) ++n;
    return (double) n / (double) (v.size() - from);
}

int main()
{
    const double fs = 48000.0;
    const size_t skip = (size_t) (0.1 * fs);

    std::printf ("Melon Trem DSP test\n");

    {
        trem::Params p; p.depth = 0.0;
        auto o = run (p, fs, 1.0);
        check (minOf (o[0], 0) > 0.9999f && maxOf (o[0], 0) < 1.0001f, "Depth 0: no change in level");
    }
    {
        trem::Params p; p.depth = 1.0; p.wave = 1.0; p.rateHz = 4.0;
        auto o = run (p, fs, 2.0);
        check (minOf (o[0], skip) < 0.01f, "Depth 100: volume drops to silence");
        check (maxOf (o[0], skip) > 0.99f && maxOf (o[0], skip) < 1.0001f, "Peak level stays at unity (no boost)");
    }
    {
        trem::Params p; p.depth = 0.5; p.wave = 0.0;
        auto o = run (p, fs, 2.0);
        check (std::abs (minOf (o[0], skip) - 0.5f) < 0.02f, "Depth 50: bottom of the cycle at half volume");
    }
    for (double hz : { 0.5, 4.0, 12.0 })
    {
        trem::Params p; p.rateHz = hz; p.depth = 1.0; p.wave = 0.3;
        auto o = run (p, fs, hz < 1.0 ? 8.0 : 3.0);
        const double m = measuredRate (o[0], fs, skip);
        char msg[96]; std::snprintf (msg, sizeof msg, "Rate %.1f Hz measured %.3f Hz", hz, m);
        check (std::abs (m - hz) < 0.02 * hz, msg);
    }
    {
        trem::Params tri; tri.depth = 1.0; tri.wave = 0.0; tri.rateHz = 4.0;
        trem::Params sq = tri; sq.wave = 1.0;
        const double a = timeAtExtremes (run (tri, fs, 2.0)[0], skip);
        const double b = timeAtExtremes (run (sq, fs, 2.0)[0], skip);
        std::printf ("     time at extremes: triangle %.2f, square %.2f\n", a, b);
        check (a < 0.35 && b > 0.8, "Wave morphs from triangle to square");
    }
    {
        // the square's edges are smoothed: no jump bigger than a few % per sample
        trem::Params p; p.depth = 1.0; p.wave = 1.0; p.rateHz = 15.0;
        auto o = run (p, fs, 1.0);
        float maxStep = 0;
        for (size_t i = 1; i < o[0].size(); ++i) maxStep = std::max (maxStep, std::abs (o[0][i] - o[0][i - 1]));
        std::printf ("     largest step: %.4f\n", maxStep);
        check (maxStep < 0.06f, "Square edges don't click");
    }
    {
        trem::Params p; p.depth = 1.0; p.wave = 0.0; p.stereo = trem::pan;
        auto o = run (p, fs, 2.0, 2);
        double maxErr = 0;
        for (size_t i = skip; i < o[0].size(); ++i) maxErr = std::max (maxErr, std::abs ((double) o[0][i] + o[1][i] - 1.0));
        check (maxErr < 0.02, "Pan: left and right in anti-phase (L + R constant)");

        p.stereo = trem::mono;
        auto m = run (p, fs, 1.0, 2);
        bool same = true;
        for (size_t i = 0; i < m[0].size(); ++i) same = same && m[0][i] == m[1][i];
        check (same, "Mono: both channels identical");
    }
    {
        trem::Params p; p.depth = 1.0; p.bypassed = true;
        auto o = run (p, fs, 0.5);
        check (minOf (o[0], 0) > 0.9999f, "Bypass: dry signal");
    }
    {
        trem::Params p; p.depth = 0.0; p.outputDb = -6.0;
        auto o = run (p, fs, 0.2);
        check (std::abs (o[0].back() - 0.5012f) < 0.001f, "Output gain -6 dB");
    }
    {
        check (std::abs (trem::syncedHz (trem::sync1_8, 120.0) - 4.0) < 1e-9, "Sync 1/8 at 120 BPM = 4 Hz");
        check (trem::syncedHz (trem::syncOff, 120.0) == 0.0, "Sync Off = use the Rate knob");
    }

    std::printf (failures == 0 ? "All tests passed\n" : "%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
