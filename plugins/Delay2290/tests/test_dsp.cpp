// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "DelayDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

struct Stereo { std::vector<float> l, r; };

static Stereo run (const delay2290::Params& p, double fs, const std::vector<float>& inL, const std::vector<float>& inR)
{
    delay2290::Engine e;
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

// energy (as amplitude) in a window of ±3 ms around `centre` (filters smear an impulse a little)
static double around (const std::vector<float>& v, size_t centre, double fs)
{
    const size_t w = (size_t) (0.003 * fs);
    double e = 0.0;
    for (size_t i = centre - w; i < std::min (v.size(), centre + w); ++i) e += (double) v[i] * v[i];
    return std::sqrt (e);
}

static std::vector<float> impulse (double fs, double seconds)
{
    std::vector<float> v ((size_t) (fs * seconds), 0.0f);
    v[0] = 1.0f;
    return v;
}

static size_t peakIndex (const std::vector<float>& v, size_t from, size_t to)
{
    size_t best = from;
    for (size_t i = from; i < std::min (to, v.size()); ++i)
        if (std::abs (v[i]) > std::abs (v[best])) best = i;
    return best;
}

int main()
{
    check (std::abs (delay2290::syncedTimeMs (5, 120.0) - 500.0) < 1e-9, "Sync: 1/4 at 120 BPM = 500 ms");
    check (std::abs (delay2290::syncedTimeMs (9, 120.0) - 375.0) < 1e-9, "Sync: 1/8 dotted at 120 BPM = 375 ms");
    check (delay2290::duckDb (-60.0, -30.0, 12.0) > -0.01 && delay2290::duckDb (0.0, -30.0, 12.0) < -11.99, "Ducking curve");

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const auto imp = impulse (fs, 2.0);
        const auto zero = std::vector<float> (imp.size(), 0.0f);

        {
            delay2290::Params p; p.mix = 0.0;
            auto o = run (p, fs, imp, imp);
            check (testsig::maxDiff (o.l, imp) < 1e-6, "Mix 0 % passes the dry signal untouched");
        }
        {
            delay2290::Params p; p.bypassed = true; p.feedback = 1.0;
            auto o = run (p, fs, imp, imp);
            check (testsig::maxDiff (o.l, imp) < 1e-6, "Bypass passes the signal untouched");
        }
        // echo timing and feedback
        {
            delay2290::Params p; p.timeMs = 250.0; p.feedback = 0.5; p.mix = 1.0; p.mode = delay2290::Mode::mono;
            p.hiCutHz = 20000.0; p.loCutHz = 10.0;
            auto o = run (p, fs, imp, imp);
            const size_t t = (size_t) std::lround (0.25 * fs);
            const size_t e1 = peakIndex (o.l, t - 50, t + 50), e2 = peakIndex (o.l, 2 * t - 50, 2 * t + 50);
            check (std::abs ((double) e1 - (double) t) <= 2.0, "First echo arrives at the delay time");
            const double r = around (o.l, 2 * t, fs) / around (o.l, t, fs);
            (void) e2;
            std::printf ("     echo 2 / echo 1 = %.3f\n", r);
            check (std::abs (r - 0.5) < 0.06, "Each repeat is Feedback times the previous one");
        }
        // stereo spread
        {
            delay2290::Params p; p.timeMs = 200.0; p.spread = 1.0; p.feedback = 0.0; p.mix = 1.0;
            auto o = run (p, fs, imp, imp);
            const size_t eL = peakIndex (o.l, 1, imp.size()), eR = peakIndex (o.r, 1, imp.size());
            check (std::abs ((double) eR / (double) eL - 1.5) < 0.02, "Stereo: Spread 100 % makes the right delay 50 % longer");
        }
        // ping-pong alternates
        {
            delay2290::Params p; p.timeMs = 200.0; p.feedback = 0.6; p.mix = 1.0; p.mode = delay2290::Mode::pingPong;
            p.hiCutHz = 20000.0; p.loCutHz = 10.0;
            auto o = run (p, fs, imp, imp);
            const size_t t = (size_t) std::lround (0.2 * fs);
            const double first = around (o.l, t, fs);
            const bool alt = first > 0.5 && around (o.r, t, fs) < 0.01
                             && around (o.r, 2 * t, fs) > 0.5 * first && around (o.l, 2 * t, fs) < 0.01;
            check (alt, "Ping-pong: first repeat left, second right");
        }
        // 100 % feedback with a loud input stays bounded; 0 % gives a single echo
        {
            delay2290::Params p; p.timeMs = 30.0; p.feedback = 1.0; p.mix = 0.5;
            auto loud = testsig::sine (fs, 200.0, 0.9, 3.0);
            auto o = run (p, fs, loud, loud);
            check (testsig::allFinite (o.l) && testsig::peak (o.l) < 4.0, "100 % feedback stays bounded");
        }
        // ducking: repeats quieter while the input is loud
        {
            delay2290::Params p; p.timeMs = 100.0; p.feedback = 0.0; p.mix = 1.0; p.duckAmountDb = 20.0; p.duckThreshDb = -30.0; p.mode = delay2290::Mode::mono;
            auto sig = testsig::sine (fs, 300.0, 0.5, 1.0);
            auto o = run (p, fs, sig, sig);
            delay2290::Params q = p; q.duckAmountDb = 0.0;
            auto ref = run (q, fs, sig, sig);
            const double d = testsig::rmsDb (o.l, (size_t) (0.5 * fs), (size_t) (0.9 * fs)) - testsig::rmsDb (ref.l, (size_t) (0.5 * fs), (size_t) (0.9 * fs));
            std::printf ("     ducked by %.1f dB\n", d);
            check (std::abs (d + 20.0) < 1.0, "Dynamic delay ducks the repeats by Amount while playing");
        }
        // changing the time while playing glides without clicks
        {
            delay2290::Engine e; e.prepare (fs, 1);
            delay2290::Params p; p.mix = 1.0; p.feedback = 0.3;
            e.setParams (p); e.snapToTargets();
            auto sig = testsig::sine (fs, 220.0, 0.3, 2.0);
            double maxStep = 0.0; float prev = 0.0f;
            for (size_t pos = 0; pos < sig.size(); pos += 256)
            {
                if (pos == (size_t) fs) { p.timeMs = 150.0; e.setParams (p); }
                float* blk[1] = { sig.data() + pos };
                e.process (blk, 1, (int) std::min<size_t> (256, sig.size() - pos));
                for (size_t i = pos; i < std::min (sig.size(), pos + 256); ++i) { maxStep = std::max (maxStep, (double) std::abs (sig[i] - prev)); prev = sig[i]; }
            }
            check (maxStep < 0.2, "Changing Time while playing does not click");
        }
        (void) zero;
    }
    std::printf (failures == 0 ? "\nAll tests passed\n" : "\n%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
