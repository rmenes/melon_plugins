// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "DirtDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

static std::vector<float> run (const dirt::Params& p, double fs, const std::vector<float>& in, std::vector<float>* right = nullptr)
{
    dirt::Engine e;
    e.prepare (fs, 2);
    e.setParams (p);
    e.snapToTargets();
    std::vector<float> l = in, r = in;
    for (size_t pos = 0; pos < l.size(); pos += 256)
    {
        float* blk[2] = { l.data() + pos, r.data() + pos };
        e.process (blk, 2, (int) std::min<size_t> (256, l.size() - pos));
    }
    if (right) *right = r;
    return l;
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const auto guitar = testsig::pluckedChords (fs, 3.0);
        const double inRms = testsig::rmsDb (guitar);

        {
            dirt::Params p; p.bypassed = true; p.drive = 1.0;
            check (testsig::maxDiff (run (p, fs, guitar), guitar) < 1e-6, "Bypass passes the signal untouched");
        }
        {
            dirt::Params p; p.mix = 0.0; p.drive = 1.0;
            check (testsig::maxDiff (run (p, fs, guitar), guitar) < 1e-6, "Mix 0 % passes the dry signal untouched");
        }
        {
            dirt::Params p; p.level = 0.0;
            check (testsig::rmsDb (run (p, fs, guitar)) < -100.0, "Level 0 = silence");
        }

        double lo = 1e9, hi = -1e9;
        bool   finite = true, bounded = true, stereo = true;
        for (int m = 0; m < dirt::kNumModels; ++m)
        {
            dirt::Params p; p.model = m;
            std::vector<float> r;
            auto out = run (p, fs, guitar, &r);
            const double db = testsig::rmsDb (out) - inRms;
            lo = std::min (lo, db); hi = std::max (hi, db);
            finite  = finite && testsig::allFinite (out);
            stereo  = stereo && testsig::maxDiff (out, r) < 1e-7;
            if ((int) fs == 48000)
                std::printf ("     %-15s  %+5.1f dB vs input (drive 50 %%)\n", dirt::models()[(size_t) m].name, db);

            p.drive = 1.0; p.tone = 1.0; p.level = 1.0;
            auto loud = run (p, fs, testsig::scaled (guitar, 4.0f));
            bounded = bounded && testsig::peak (loud) < 8.0 && testsig::allFinite (loud);
        }
        check (finite, "Every model gives finite output");
        check (bounded, "Max Drive, Tone, Level on a hot signal stays bounded");
        check (stereo, "Both channels get the same processing");
        check (hi - lo < 6.0, "Models are level-matched within 6 dB at default settings");

        // more Drive = more distortion (crest factor goes down)
        {
            dirt::Params p; p.model = 0; p.drive = 0.0;
            const double c0 = testsig::crestDb (run (p, fs, guitar));
            p.drive = 1.0;
            const double c1 = testsig::crestDb (run (p, fs, guitar));
            check (c1 < c0 - 1.0, "More Drive compresses the signal more");
        }
        // Tone: brighter at 100 % than at 0 %
        {
            dirt::Params p; p.model = 4; p.tone = 0.0;
            const double d0 = testsig::brightness (run (p, fs, guitar));
            p.tone = 1.0;
            const double d1 = testsig::brightness (run (p, fs, guitar));
            check (d1 > d0, "Tone up = brighter");
        }
        // switching models while playing does not click
        {
            dirt::Engine e; e.prepare (fs, 1);
            dirt::Params p; e.setParams (p); e.snapToTargets();
            auto sig = testsig::sine (fs, 220.0, 0.2, 1.0);
            double maxStep = 0.0; float prev = 0.0f;
            for (size_t pos = 0; pos < sig.size(); pos += 128)
            {
                p.model = (int) (pos / 4096) % dirt::kNumModels;
                e.setParams (p);
                float* blk[1] = { sig.data() + pos };
                e.process (blk, 1, (int) std::min<size_t> (128, sig.size() - pos));
                for (size_t i = pos; i < std::min (sig.size(), pos + 128); ++i)
                {
                    maxStep = std::max (maxStep, (double) std::abs (sig[i] - prev));
                    prev = sig[i];
                }
            }
            check (maxStep < 0.5, "Switching models while playing does not click");
        }
    }
    std::printf (failures == 0 ? "\nAll tests passed\n" : "\n%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
