// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "CompDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

static std::vector<float> run (const comp::Params& p, double fs, std::vector<float> sig, double* grDb = nullptr)
{
    comp::Engine e;
    e.prepare (fs, 1);
    e.setParams (p);
    e.snapToTargets();
    for (size_t pos = 0; pos < sig.size(); pos += 256)
    {
        float* blk[1] = { sig.data() + pos };
        e.process (blk, 1, (int) std::min<size_t> (256, sig.size() - pos));
    }
    if (grDb) *grDb = e.currentGainReductionDb();
    return sig;
}

// loud tone (1 s) then the same tone 30 dB quieter (1 s)
static std::vector<float> step (double fs)
{
    auto loud  = testsig::sine (fs, 220.0, 0.3, 1.0);
    auto quiet = testsig::sine (fs, 220.0, 0.3 * 0.0316, 1.0);
    loud.insert (loud.end(), quiet.begin(), quiet.end());
    return loud;
}

int main()
{
    check (std::abs (comp::levelGain (0.5) - 1.0) < 1e-12, "Level at noon = unity");
    check (comp::cellGain (0.0) == 1.0 && comp::cellGain (comp::kThreshold) == 0.5, "Cell gain curve");

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const auto in = step (fs);
        const size_t l0 = (size_t) (0.5 * fs), l1 = (size_t) (1.0 * fs), q0 = (size_t) (1.6 * fs), q1 = (size_t) (2.0 * fs);
        const double inStep = testsig::rmsDb (in, l0, l1) - testsig::rmsDb (in, q0, q1);

        comp::Params p;
        p.blend = 1.0;
        auto out = run (p, fs, in);
        check (testsig::allFinite (out), "Output is finite");
        const double outStep = testsig::rmsDb (out, l0, l1) - testsig::rmsDb (out, q0, q1);
        std::printf ("        30 dB input step -> %.1f dB output step\n", outStep);
        check (outStep < inStep * 0.6, "Compresses: 30 dB input step shrinks a lot");
        check (std::abs (testsig::rmsDb (out, l0, l1) - testsig::rmsDb (in, l0, l1)) < 6.0,
               "Default settings stay near unity for a loud guitar-level note");

        // more Sustain = smaller step, and quiet notes get louder
        comp::Params lo = p, hi = p;
        lo.sustain = 0.2; hi.sustain = 0.9;
        auto oLo = run (lo, fs, in), oHi = run (hi, fs, in);
        const double stepLo = testsig::rmsDb (oLo, l0, l1) - testsig::rmsDb (oLo, q0, q1);
        const double stepHi = testsig::rmsDb (oHi, l0, l1) - testsig::rmsDb (oHi, q0, q1);
        check (stepHi < stepLo - 3.0, "More Sustain squeezes harder");
        check (testsig::rmsDb (oHi, q0, q1) > testsig::rmsDb (oLo, q0, q1) + 6.0, "More Sustain lifts quiet notes");

        // Blend 0 = dry (in phase, unity at Level noon)
        comp::Params dry = p;
        dry.level = 0.5;
        dry.blend = 0.0;
        check (testsig::maxDiff (run (dry, fs, in), in) < 1e-5, "Blend 0 = dry signal");

        // Blend keeps the peaks: the note attack stands out more from the sustained tail at Blend 50 %
        const auto gtr = testsig::pluckedChords (fs, 3.0);
        auto attackToTail = [fs] (const std::vector<float>& v) {
            return testsig::rmsDb (v, 0, (size_t) (0.06 * fs)) - testsig::rmsDb (v, (size_t) (0.7 * fs), (size_t) (0.95 * fs));
        };
        comp::Params half = p;
        half.blend = 0.5;
        const double atFull = attackToTail (run (p, fs, gtr)), atHalf = attackToTail (run (half, fs, gtr));
        std::printf ("        attack-to-tail: Blend 100 %% %.1f dB, Blend 50 %% %.1f dB, dry %.1f dB\n", atFull, atHalf, attackToTail (gtr));
        check (atHalf > atFull + 1.0 && atFull < attackToTail (gtr) - 3.0, "Blend brings back the peaks");

        // Humbucker switch releases faster: gain recovers sooner after the loud part
        const size_t r0 = (size_t) (1.0 * fs), r1 = (size_t) (1.15 * fs);
        comp::Params sc = p, hb = p;
        hb.pickup = comp::Pickup::humbucker;
        const auto oSc = run (sc, fs, in), oHb = run (hb, fs, in);
        check (testsig::rmsDb (oHb, r0, r1) > testsig::rmsDb (oSc, r0, r1) + 1.0, "Humbucker setting releases faster");

        // Tone
        comp::Params dark = p, bright = p;
        dark.tone = 0.0; bright.tone = 1.0;
        check (testsig::brightness (run (bright, fs, gtr)) > testsig::brightness (run (dark, fs, gtr)) * 1.5, "Tone brightens");

        // Bypass and output
        comp::Params byp = p;
        byp.bypassed = true;
        check (testsig::maxDiff (run (byp, fs, in), in) < 1e-6, "Bypass is transparent");
        comp::Params o6 = dry;
        o6.outputDb = -6.0;
        check (std::abs (testsig::rmsDb (run (o6, fs, in)) - testsig::rmsDb (in) + 6.0) < 0.05, "Output gain");

        // gain reduction meter
        double gr = 0.0;
        run (p, fs, testsig::sine (fs, 220.0, 0.5, 0.5), &gr);
        check (gr < -6.0, "Gain reduction reported on loud input");

        // stress: very hot input stays finite and bounded
        comp::Params hot = p;
        hot.sustain = 1.0; hot.level = 1.0;
        auto oHot = run (hot, fs, testsig::sine (fs, 100.0, 4.0, 0.5));
        check (testsig::allFinite (oHot) && testsig::peak (oHot) < 20.0, "Hot input stays bounded");
    }

    std::printf (failures == 0 ? "All tests passed\n" : "%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
