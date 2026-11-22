// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "MultiModDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

struct Stereo { std::vector<float> l, r; };

static Stereo run (const mmod::Params& p, double fs, const std::vector<float>& inL, const std::vector<float>& inR,
                   mmod::Engine* engine = nullptr)
{
    mmod::Engine local;
    auto& e = engine != nullptr ? *engine : local;
    if (engine == nullptr)
    {
        e.prepare (fs, 2);
        e.setParams (p);
        e.snapToTargets();
    }
    else
    {
        e.setParams (p);
    }
    Stereo s { inL, inR };
    for (size_t pos = 0; pos < inL.size(); pos += 256)
    {
        float* blk[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (blk, 2, (int) std::min<size_t> (256, inL.size() - pos));
    }
    return s;
}

static mmod::Params base (int machine)
{
    mmod::Params p;
    p.machine = machine; p.speedHz = 2.0; p.depth = 0.8; p.mix = 1.0; p.width = 0.5;
    return p;
}

// Magnitude of `freq` in `v` (Goertzel over the given range)
static double tone (const std::vector<float>& v, double fs, double freq, size_t from, size_t to)
{
    const double w = 2.0 * mmod::kPi * freq / fs, c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = from; i < to; ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (to - from);
}

// RMS of each window of `ms`; returns { min, max } over the signal after `skip` seconds
static std::pair<double, double> windowRange (const std::vector<float>& v, double fs, double ms, double skip)
{
    const size_t w = (size_t) (ms * 0.001 * fs);
    double lo = 1e9, hi = 0.0;
    for (size_t i = (size_t) (skip * fs); i + w <= v.size(); i += w)
    {
        const double r = testsig::rms (v, i, i + w);
        lo = std::min (lo, r); hi = std::max (hi, r);
    }
    return { lo, hi };
}

int main()
{
    check (std::abs (mmod::syncedHz (7, 120.0) - 2.0) < 1e-9, "Sync: 1/4 at 120 BPM = 2 Hz");
    check (std::abs (mmod::syncedHz (3, 120.0) - 0.5) < 1e-9, "Sync: 1/1 at 120 BPM = 0.5 Hz");
    check (mmod::stepIndex (mmod::stepPercent (4, 6) * 0.01, 6) == 4, "Stepped parameters: preset percentages select the right step");
    check (mmod::paramText (mmod::phaser, 1, mmod::stepPercent (3, 6) * 0.01) == "8", "Param text for stepped values");
    check (mmod::paramText (mmod::flanger, 1, 0.5) == "+0 %", "Flanger Regen is centred at 50 %");
    {
        bool ok = true;
        for (int w = 0; w < mmod::kNumWaves; ++w)
            for (double pos = 0.0; pos < 48.0; pos += 0.013)
                ok = ok && std::abs (mmod::waveValue (w, pos)) <= 1.0001;
        check (ok, "Every LFO wave stays within -1..+1");
    }

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const auto guitar = testsig::pluckedChords (fs, 3.0);
        const double inDb = testsig::rmsDb (guitar);

        {
            auto p = base (mmod::chorus); p.mix = 0.0;
            auto o = run (p, fs, guitar, guitar);
            check (testsig::maxDiff (o.l, guitar) < 1e-6, "Mix 0 % passes the dry signal untouched");
        }
        {
            auto p = base (mmod::flanger); p.bypassed = true; p.param1 = 1.0;
            auto o = run (p, fs, guitar, guitar);
            check (testsig::maxDiff (o.l, guitar) < 1e-6, "Bypass passes the signal untouched");
        }

        // every machine: finite, not silent, sensible level, and actually does something
        for (int m = 0; m < mmod::kNumMachines; ++m)
        {
            for (double v : { 0.0, 0.5, 1.0 })
            {
                auto p = base (m); p.param1 = v; p.param2 = v; p.depth = 1.0;
                if (m == mmod::autoswell) { p.param1 = std::min (v, 0.5); p.param2 = 0.5; } // 3 s swells on 1 s chords would be mostly quiet
                auto o = run (p, fs, guitar, guitar);
                const double outDb = 0.5 * (testsig::rmsDb (o.l) + testsig::rmsDb (o.r));
                char what[160];
                std::snprintf (what, sizeof what, "%-12s P1/P2 %3.0f %%: finite, level %+5.1f dB vs input, changes the sound",
                               mmod::machines()[(size_t) m].name, v * 100.0, outDb - inDb);
                check (testsig::allFinite (o.l) && testsig::allFinite (o.r) && outDb - inDb > -20.0 && outDb - inDb < 9.0
                           && testsig::maxDiff (o.l, guitar) > 1e-3 && testsig::peak (o.l) < 4.0,
                       what);
            }
        }

        {
            auto p = base (mmod::chorus); p.mix = 0.5;
            auto o = run (p, fs, guitar, guitar);
            check (std::abs (testsig::rmsDb (o.l) - inDb) < 3.0, "Chorus at Mix 50 % keeps about the same level");
            check (testsig::maxDiff (o.l, o.r) > 1e-3, "Width 50 %: left and right differ (stereo chorus)");
            p.width = 0.0;
            o = run (p, fs, guitar, guitar);
            check (testsig::maxDiff (o.l, o.r) < 1e-6, "Width 0 %: left and right are identical");
        }
        {
            auto p = base (mmod::vintageTrem); p.depth = 1.0; p.speedHz = 4.0; p.param1 = 0.0; p.param2 = 1.0;
            const auto s = testsig::sine (fs, 440.0, 0.3, 2.0);
            auto o = run (p, fs, s, s);
            const auto r = windowRange (o.l, fs, 10.0, 0.5);
            check (r.second / std::max (1e-9, r.first) > 30.0, "Vintage Trem at full depth: deep volume modulation");
        }
        {
            auto p = base (mmod::patternTrem); p.depth = 1.0; p.speedHz = 8.0; p.param1 = 0.0; p.param2 = 0.0; // Pulse
            const auto s = testsig::sine (fs, 440.0, 0.3, 2.0);
            auto o = run (p, fs, s, s);
            // step n covers [n/8, (n+1)/8) s; check the middle of an on step and an off step
            const double on  = testsig::rms (o.l, (size_t) (fs * (8.0 / 8 + 0.03)), (size_t) (fs * (9.0 / 8 - 0.03)));
            const double off = testsig::rms (o.l, (size_t) (fs * (9.0 / 8 + 0.03)), (size_t) (fs * (10.0 / 8 - 0.03)));
            check (on > 0.15 && off < 0.01, "Pattern Trem: on steps play, off steps are silent");
        }
        {
            auto p = base (mmod::quadrature); p.speedHz = 10.0; p.depth = 0.0;
            p.param1 = mmod::stepPercent (mmod::shiftUp, mmod::kNumQuadModes) * 0.01;
            p.param2 = mmod::stepPercent (1, 3) * 0.01; // x10: shift by 100 Hz
            const auto s = testsig::sine (fs, 1000.0, 0.3, 1.0);
            auto o = run (p, fs, s, s);
            const size_t a = (size_t) (0.2 * fs), b = (size_t) (0.9 * fs);
            const double up = tone (o.l, fs, 1100.0, a, b), orig = tone (o.l, fs, 1000.0, a, b), down = tone (o.l, fs, 900.0, a, b);
            check (up > 20.0 * orig && up > 20.0 * down, "Quadrature Shift Up moves 1000 Hz to 1100 Hz");
        }
        {
            auto p = base (mmod::phaser); p.param1 = 1.0; p.param2 = 1.0; p.depth = 1.0;
            std::vector<float> imp ((size_t) (fs * 2.0), 0.0f); imp[0] = 1.0f;
            auto o = run (p, fs, imp, imp);
            check (testsig::allFinite (o.l) && testsig::rms (o.l, (size_t) (1.5 * fs), imp.size()) < 1e-4,
                   "Phaser with 16 stages and maximum Regen stays stable");
        }
        {
            auto p = base (mmod::flanger); p.param1 = 1.0; p.param2 = 1.0;
            std::vector<float> imp ((size_t) (fs * 3.0), 0.0f); imp[0] = 1.0f;
            auto o = run (p, fs, imp, imp);
            check (testsig::allFinite (o.l) && testsig::peak (o.l) < 2.0, "Flanger with maximum Regen stays stable");
        }
        {
            // Autoswell: the attack of each note is soft, the end of the note at full level
            auto p = base (mmod::autoswell); p.depth = 1.0; p.param1 = 0.4; p.param2 = 0.5; p.mix = 1.0;
            std::vector<float> note ((size_t) (fs * 2.0), 0.0f);
            for (size_t i = (size_t) (0.5 * fs); i < note.size(); ++i)
                note[i] = (float) (0.3 * std::sin (2.0 * mmod::kPi * 220.0 * (double) i / fs));
            auto o = run (p, fs, note, note);
            const double early = testsig::rms (o.l, (size_t) (0.5 * fs), (size_t) (0.52 * fs));
            const double late  = testsig::rms (o.l, (size_t) (1.6 * fs), (size_t) (1.8 * fs));
            check (early < 0.2 * late && late > 0.18, "Autoswell fades each new note in");
        }
        {
            // switching machines while playing: no jumps, no NaN
            mmod::Engine e;
            e.prepare (fs, 2);
            auto p = base (mmod::flanger);
            e.setParams (p);
            e.snapToTargets();
            bool ok = true;
            for (int m = 0; m < mmod::kNumMachines; ++m)
            {
                p.machine = m;
                auto o = run (p, fs, std::vector<float> (guitar.begin(), guitar.begin() + (long) (fs * 0.25)),
                              std::vector<float> (guitar.begin(), guitar.begin() + (long) (fs * 0.25)), &e);
                ok = ok && testsig::allFinite (o.l) && testsig::peak (o.l) < 4.0;
            }
            check (ok, "Switching through every machine while playing stays clean");
        }
        {
            // host sync: the LFO follows the host position
            mmod::Engine e;
            e.prepare (fs, 2);
            e.setHostPosition (50.25);
            check (std::abs (e.getPosition() - 2.25) < 1e-9, "Host position locks (and wraps) the LFO");
        }
    }

    std::printf (failures == 0 ? "\nAll tests passed\n" : "\n%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
