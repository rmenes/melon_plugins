// DSP core test (does not need JUCE):  g++ -std=c++17 -O2 -I../Source test_dsp.cpp -o test_dsp
#include "ChorusDsp.h"
#include <cstdio>
#include <vector>

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

struct Run
{
    std::vector<float> l, r;
};

// Runs a mono test signal through the engine (copied to both channels, like a guitar on a stereo track)
static Run run (const chorus::Params& p, double fs, int numSamples, float (*signal) (int, double))
{
    chorus::Engine e;
    e.prepare (fs, 2);
    e.setParams (p);
    e.snapToTargets();

    Run out { std::vector<float> ((size_t) numSamples), std::vector<float> ((size_t) numSamples) };
    for (int i = 0; i < numSamples; ++i)
        out.l[(size_t) i] = out.r[(size_t) i] = signal (i, fs);

    float* ch[2] = { out.l.data(), out.r.data() };
    for (int pos = 0; pos < numSamples; pos += 512)
    {
        float* blk[2] = { ch[0] + pos, ch[1] + pos };
        e.process (blk, 2, std::min (512, numSamples - pos));
    }
    return out;
}

static float impulse (int i, double) { return i == 0 ? 1.0f : 0.0f; }
static float sine440 (int i, double fs) { return 0.5f * (float) std::sin (2.0 * chorus::kPi * 440.0 * i / fs); }

static double maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double m = 0;
    for (size_t i = 0; i < a.size(); ++i)
        m = std::max (m, (double) std::abs (a[i] - b[i]));
    return m;
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const int n = (int) fs * 2;

        // 1. Mix 0 → output is exactly the input
        {
            chorus::Params p;
            p.mix = 0.0;
            auto out = run (p, fs, n, sine440);
            std::vector<float> dry ((size_t) n);
            for (int i = 0; i < n; ++i) dry[(size_t) i] = sine440 (i, fs);
            check (maxDiff (out.l, dry) < 1e-6, "Mix 0 % passes the dry signal untouched");
        }

        // 2. Bypass → output is exactly the input
        {
            chorus::Params p;
            p.bypassed = true;
            p.depth    = 1.0;
            auto out = run (p, fs, n, sine440);
            std::vector<float> dry ((size_t) n);
            for (int i = 0; i < n; ++i) dry[(size_t) i] = sine440 (i, fs);
            check (maxDiff (out.l, dry) < 1e-6, "Bypass passes the dry signal untouched");
        }

        // 3. Depth 0, Mix 100, Tone flat → a single echo at the base delay, unity gain
        {
            chorus::Params p;
            p.depth = 0.0;
            p.mix   = 1.0;
            auto out = run (p, fs, n, impulse);
            size_t peakAt = 0;
            double sum    = 0;
            for (size_t i = 0; i < out.l.size(); ++i)
            {
                sum += out.l[i];
                if (std::abs (out.l[i]) > std::abs (out.l[peakAt])) peakAt = i;
            }
            const double ms = 1000.0 * (double) peakAt / fs;
            check (std::abs (ms - chorus::kBaseDelayMs) < 0.05, "Depth 0: echo sits at the 7 ms base delay");
            check (std::abs (sum - 1.0) < 1e-3, "Tone flat: the chorus signal has unity gain");
        }

        // 4. Width 0 → left and right identical; width 100 → they differ
        {
            chorus::Params p;
            p.width = 0.0;
            p.depth = 1.0;
            auto mono = run (p, fs, n, sine440);
            check (maxDiff (mono.l, mono.r) < 1e-6, "Width 0 %: left and right are identical (mono chorus)");

            p.width = 1.0;
            auto wide = run (p, fs, n, sine440);
            check (maxDiff (wide.l, wide.r) > 0.05, "Width 100 %: left and right differ (stereo chorus)");
        }

        // 5. Extremes stay finite and bounded
        {
            chorus::Params p;
            p.rateHz = chorus::kMaxRateHz;
            p.depth  = 1.0;
            p.width  = 1.0;
            p.mix    = 0.5;
            p.tone   = 1.0;
            auto out = run (p, fs, n, sine440);
            bool   finite = true;
            double peak   = 0;
            for (float v : out.l) { finite &= std::isfinite (v); peak = std::max (peak, (double) std::abs (v)); }
            check (finite && peak < 2.0, "Max rate/depth/bright: output finite and bounded");
        }
    }

    // 6. Tone: dark cuts the highs, bright boosts them (measured on the chorus signal only)
    {
        auto gainAt = [] (double tone, double freq) {
            chorus::Params p;
            p.depth = 0.0;
            p.mix   = 1.0;
            p.tone  = tone;
            const double fs = 48000;
            static double f;
            f = freq;
            auto out = run (p, fs, 48000, [] (int i, double rate) { return (float) std::sin (2.0 * chorus::kPi * f * i / rate); });
            double peak = 0;
            for (size_t i = 24000; i < out.l.size(); ++i) peak = std::max (peak, (double) std::abs (out.l[i]));
            return 20.0 * std::log10 (peak);
        };
        std::printf ("Tone (chorus signal, 48 kHz)\n");
        const double dark100 = gainAt (-1.0, 100.0), dark5k = gainAt (-1.0, 5000.0);
        const double brt100  = gainAt (1.0, 100.0), brt5k = gainAt (1.0, 5000.0);
        std::printf ("  Dark:   100 Hz %+5.1f dB   5 kHz %+5.1f dB\n", dark100, dark5k);
        std::printf ("  Bright: 100 Hz %+5.1f dB   5 kHz %+5.1f dB\n", brt100, brt5k);
        check (dark5k < -7.0 && std::abs (dark100) < 0.5, "Dark: highs cut, lows unchanged");
        check (brt5k > 7.0 && std::abs (brt100) < 0.5, "Bright: highs boosted, lows unchanged");
    }

    std::printf ("\n%s\n", failures == 0 ? "All tests passed." : "SOME TESTS FAILED.");
    return failures == 0 ? 0 : 1;
}
