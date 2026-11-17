// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "ReverbDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

struct Stereo { std::vector<float> l, r; };

static Stereo run (const reverb::Params& p, double fs, const std::vector<float>& in)
{
    reverb::Engine e;
    e.prepare (fs, 2);
    e.setParams (p);
    e.snapToTargets();
    Stereo s { in, in };
    for (size_t pos = 0; pos < in.size(); pos += 512)
    {
        float* blk[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (blk, 2, (int) std::min<size_t> (512, in.size() - pos));
    }
    return s;
}

// 50 ms noise burst followed by silence
static std::vector<float> burst (double fs, double seconds)
{
    std::vector<float> v ((size_t) (fs * seconds), 0.0f);
    melon::Noise nz;
    for (size_t i = 0; i < (size_t) (0.05 * fs); ++i) v[i] = (float) (0.3 * nz.next());
    return v;
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const auto in = burst (fs, 6.0);

        {
            reverb::Params p; p.mix = 0.0;
            auto o = run (p, fs, in);
            check (testsig::maxDiff (o.l, in) < 1e-6, "Mix 0 % passes the dry signal untouched");
        }
        {
            reverb::Params p; p.bypassed = true;
            auto o = run (p, fs, in);
            check (testsig::maxDiff (o.l, in) < 1e-6, "Bypass passes the signal untouched");
        }

        bool finite = true, bounded = true, decays = true;
        double lo = 1e9, hi = -1e9;
        for (int m = 0; m < reverb::kNumMachines; ++m)
        {
            reverb::Params p; p.machine = m; p.mix = 1.0; p.decayS = 2.0;
            auto o = run (p, fs, in);
            const double tail = testsig::rmsDb (o.l, (size_t) (0.05 * fs), (size_t) (1.0 * fs));
            const double end  = testsig::rmsDb (o.l, (size_t) (5.5 * fs), (size_t) (6.0 * fs));
            finite = finite && testsig::allFinite (o.l) && testsig::allFinite (o.r);
            if (m != reverb::swell) // swell fades the (short) burst in, so it is quieter by design
            {
                lo = std::min (lo, tail);
                hi = std::max (hi, tail);
            }
            decays = decays && end < tail - 30.0;
            if ((int) fs == 48000)
                std::printf ("     %-12s first second %6.1f dB, at 5.5 s %6.1f dB\n", reverb::machines()[(size_t) m].name, tail, end);

            // extreme settings stay bounded
            reverb::Params x = p; x.decayS = 60.0; x.param1 = 1.0; x.param2 = 1.0; x.mod = 1.0; x.tone = 1.0;
            auto e = run (x, fs, testsig::scaled (in, 3.0f));
            bounded = bounded && testsig::allFinite (e.l) && testsig::peak (e.l) < 10.0;
        }
        check (finite, "Every machine gives finite output");
        check (decays, "Every machine's tail dies away (Decay 2 s)");
        check (bounded, "Max Decay / Param / Mod / Tone stays bounded");
        check (hi - lo < 12.0, "Machines are level-matched within 12 dB");

        // RT60 of the Hall: measure the slope of the energy decay
        for (double t60 : { 1.0, 4.0 })
        {
            reverb::Params p; p.machine = reverb::hall; p.mix = 1.0; p.decayS = t60; p.mod = 0.0; p.tone = 1.0; p.param2 = 1.0 / 3.0; // low decay x1
            auto o = run (p, fs, burst (fs, 2.0 + 1.2 * t60));
            const double a = testsig::rmsDb (o.l, (size_t) (0.3 * fs), (size_t) (0.4 * fs));
            const double b = testsig::rmsDb (o.l, (size_t) ((0.3 + 0.5 * t60) * fs), (size_t) ((0.4 + 0.5 * t60) * fs));
            const double measured = (0.5 * t60) * 60.0 / std::max (1e-6, a - b);
            std::printf ("     Hall Decay %.1f s -> measured RT60 %.2f s\n", t60, measured);
            check (measured > 0.7 * t60 && measured < 1.4 * t60, "Decay sets the RT60");
        }

        // Infinite keeps the tail going
        {
            reverb::Params p; p.machine = reverb::hall; p.mix = 1.0; p.decayS = 1.0; p.infinite = true;
            auto o = run (p, fs, burst (fs, 5.0));
            const double a = testsig::rmsDb (o.l, (size_t) (0.5 * fs), (size_t) (1.0 * fs));
            const double b = testsig::rmsDb (o.l, (size_t) (4.5 * fs), (size_t) (5.0 * fs));
            check (b > a - 6.0, "Infinite holds the tail");
        }
        // Stereo: the two outputs are decorrelated
        {
            reverb::Params p; p.mix = 1.0;
            auto o = run (p, fs, in);
            double sl = 0, sr = 0, slr = 0;
            for (size_t i = (size_t) (0.2 * fs); i < (size_t) (2.0 * fs); ++i) { sl += o.l[i] * o.l[i]; sr += o.r[i] * o.r[i]; slr += o.l[i] * o.r[i]; }
            check (std::abs (slr) / std::sqrt (sl * sr) < 0.5, "Left and right tails are decorrelated (wide)");
        }
    }
    std::printf (failures == 0 ? "\nAll tests passed\n" : "\n%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
