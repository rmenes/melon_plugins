// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "MultiDelayDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

struct Stereo { std::vector<float> l, r; };

static Stereo run (const mdelay::Params& p, double fs, const std::vector<float>& inL, const std::vector<float>& inR)
{
    mdelay::Engine e;
    e.prepare (fs, 2);
    e.setParams (p);
    e.snapToTargets();
    Stereo s { inL, inR };
    for (size_t pos = 0; pos < inL.size(); pos += 256)
    {
        float* blk[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (blk, 2, (int) std::min<size_t> (256, inL.size() - pos));
    }
    return s;
}

static std::vector<float> impulse (double fs, double seconds)
{
    std::vector<float> v ((size_t) (fs * seconds), 0.0f);
    v[0] = 1.0f;
    return v;
}

// energy (as amplitude) in a window of +-4 ms around `centre`
static double around (const std::vector<float>& v, double centreMs, double fs)
{
    const size_t c = (size_t) (centreMs * 0.001 * fs), w = (size_t) (0.004 * fs);
    double e = 0.0;
    for (size_t i = c > w ? c - w : 0; i < std::min (v.size(), c + w); ++i) e += (double) v[i] * v[i];
    return std::sqrt (e);
}

static mdelay::Params base (int machine)
{
    mdelay::Params p;
    p.machine = machine; p.timeMs = 250.0; p.repeats = 0.5; p.mix = 1.0; p.filter = 1.0; p.depth = 0.0; p.grit = 0.0;
    if (machine == mdelay::digital) { p.param1 = 0.0; p.param2 = 0.0; } // no smear, low cut at 20 Hz
    return p;
}

int main()
{
    check (std::abs (mdelay::syncedTimeMs (5, 120.0) - 500.0) < 1e-9, "Sync: 1/4 at 120 BPM = 500 ms");
    check (std::abs (mdelay::syncedTimeMs (9, 120.0) - 375.0) < 1e-9, "Sync: 1/8 dotted at 120 BPM = 375 ms");
    check (mdelay::stepIndex (mdelay::stepPercent (3, 9) * 0.01, 9) == 3, "Stepped parameters: preset percentages select the right step");
    check (mdelay::paramText (mdelay::dual, 1, mdelay::stepPercent (7, 9) * 0.01) == "3/2", "Param text for stepped values");

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const auto imp = impulse (fs, 2.0);

        {
            auto p = base (mdelay::digital); p.mix = 0.0;
            auto o = run (p, fs, imp, imp);
            check (testsig::maxDiff (o.l, imp) < 1e-6, "Mix 0 % passes the dry signal untouched");
        }
        {
            auto p = base (mdelay::tape); p.bypassed = true; p.repeats = 1.0;
            auto o = run (p, fs, imp, imp);
            check (testsig::maxDiff (o.l, imp) < 1e-6, "Bypass passes the signal untouched");
        }
        {
            auto p = base (mdelay::digital);
            auto o = run (p, fs, imp, imp);
            const double e1 = around (o.l, 250, fs), e2 = around (o.l, 500, fs), gap = around (o.l, 375, fs);
            check (e1 > 0.8 && e1 < 1.1, "Digital: first echo at Time, full level");
            check (std::abs (e2 / e1 - 0.5) < 0.05, "Digital: second echo at Repeats (50 %) of the first");
            check (gap < 0.01, "Digital: silence between echoes");
        }
        {
            auto p = base (mdelay::dual); p.param1 = mdelay::stepPercent (2, 9) * 0.01; p.param2 = 0.0; // 1/2, parallel
            auto o = run (p, fs, imp, imp);
            check (around (o.l, 250, fs) > 0.5 && around (o.r, 125, fs) > 0.5 && around (o.l, 125, fs) < 0.01,
                   "Dual parallel: left at Time, right at Time x Ratio");
        }
        {
            auto p = base (mdelay::pattern); p.param1 = mdelay::stepPercent (4, 8) * 0.01; p.param2 = 0.0; // sixteenths
            auto o = run (p, fs, imp, imp);
            bool ok = true;
            for (double t : { 62.5, 125.0, 187.5, 250.0 }) ok = ok && around (o.l, t, fs) > 0.2;
            check (ok, "Pattern: sixteenths land at 1/4, 1/2, 3/4 and 1 x Time");
        }
        {
            auto p = base (mdelay::reverse); p.timeMs = 300.0; p.repeats = 0.0; p.param2 = 0.0;
            const auto sine = testsig::sine (fs, 440.0, 0.5, 0.3);
            std::vector<float> in (imp.size(), 0.0f);
            std::copy (sine.begin(), sine.end(), in.begin());
            auto o = run (p, fs, in, in);
            check (testsig::rms (o.l, (size_t) (0.35 * fs), (size_t) (0.8 * fs)) > 0.05, "Reverse: backwards repeats after the note");
        }
        {
            auto p = base (mdelay::ice); p.param1 = mdelay::stepPercent (4, 7) * 0.01; p.param2 = 1.0;
            const auto sine = testsig::sine (fs, 220.0, 0.5, 0.2);
            std::vector<float> in (imp.size(), 0.0f);
            std::copy (sine.begin(), sine.end(), in.begin());
            p.repeats = 0.0;
            auto o = run (p, fs, in, in);
            // zero crossings of the echo: an octave up doubles them
            std::vector<float> echo (o.l.begin() + (size_t) (0.30 * fs), o.l.begin() + (size_t) (0.40 * fs));
            int zc = 0;
            for (size_t i = 1; i < echo.size(); ++i) zc += (echo[i - 1] < 0) != (echo[i] < 0);
            check (zc > 70 && zc < 105, "Ice: octave up doubles the pitch of the repeats");
        }
        {
            auto p = base (mdelay::digital); p.repeats = 0.3;
            std::vector<float> in (imp.size() * 2, 0.0f);
            const auto chords = testsig::pluckedChords (fs, 0.5);
            std::copy (chords.begin(), chords.end(), in.begin());
            mdelay::Engine e; e.prepare (fs, 2); e.setParams (p); e.snapToTargets();
            std::vector<float> l = in, r = in;
            for (size_t pos = 0; pos < l.size(); pos += 256)
            {
                if (pos >= (size_t) (0.6 * fs)) { p.infinite = true; e.setParams (p); }
                float* blk[2] = { l.data() + pos, r.data() + pos };
                e.process (blk, 2, (int) std::min<size_t> (256, l.size() - pos));
            }
            const double a = testsig::rms (l, (size_t) (1.0 * fs), (size_t) (2.0 * fs));
            const double b = testsig::rms (l, (size_t) (3.0 * fs), (size_t) (4.0 * fs));
            check (a > 0.01 && std::abs (melon::gainToDb (b / a)) < 1.0, "Infinite holds the repeats at a steady level");
        }
        {
            auto p = base (mdelay::digital); p.trails = true;
            mdelay::Engine e; e.prepare (fs, 2); e.setParams (p); e.snapToTargets();
            std::vector<float> l = imp, r = imp;
            for (size_t pos = 0; pos < l.size(); pos += 256)
            {
                if (pos >= (size_t) (0.1 * fs)) { p.bypassed = true; e.setParams (p); }
                float* blk[2] = { l.data() + pos, r.data() + pos };
                e.process (blk, 2, (int) std::min<size_t> (256, l.size() - pos));
            }
            check (around (l, 250, fs) > 0.8, "Trails: the repeats keep ringing after bypass");
        }

        // every machine: stable with 100 % repeats, grit and modulation, on a loud guitar signal
        bool stable = true;
        double peak = 0.0;
        for (int m = 0; m < mdelay::kNumMachines; ++m)
        {
            for (double p1 : { 0.0, 1.0 })
            {
                auto p = base (m); p.repeats = 1.0; p.grit = 0.7; p.depth = 0.5; p.speed = 0.5; p.param1 = p1; p.param2 = 0.6;
                p.mix = 0.5; p.filter = 0.6;
                const auto in = testsig::scaled (testsig::pluckedChords (fs, 4.0), 1.0f);
                auto o = run (p, fs, in, in);
                stable = stable && testsig::allFinite (o.l) && testsig::allFinite (o.r);
                peak = std::max ({ peak, testsig::peak (o.l), testsig::peak (o.r) });
            }
        }
        check (stable && peak < 4.0, "All 12 machines stay finite and bounded at 100 % repeats");

        // machine switching while playing: no NaN and no big jump
        {
            mdelay::Engine e; e.prepare (fs, 2);
            auto p = base (mdelay::digital); p.mix = 0.5;
            e.setParams (p); e.snapToTargets();
            auto in = testsig::pluckedChords (fs, 3.0);
            std::vector<float> l = in, r = in;
            for (size_t pos = 0, k = 0; pos < l.size(); pos += 256, ++k)
            {
                if (k % 40 == 0) { p.machine = (int) (k / 40) % mdelay::kNumMachines; e.setParams (p); }
                float* blk[2] = { l.data() + pos, r.data() + pos };
                e.process (blk, 2, (int) std::min<size_t> (256, l.size() - pos));
            }
            check (testsig::allFinite (l) && testsig::peak (l) < 3.0, "Switching machines while playing is safe");
        }
    }

    std::printf (failures == 0 ? "\nAll tests passed\n" : "\n%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
