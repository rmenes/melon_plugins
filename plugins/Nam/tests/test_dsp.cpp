// DSP test for Melon NAM (does not need JUCE or the neural engine):
//   g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp
#include "NamDsp.h"
#include "Resampler.h"
#include <cstdio>
#include <vector>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

// Steady-state level (dB) of a sine at `freq` through the tone stack
static double toneGainDb (double bass, double mid, double treble, double freq, double fs = 48000.0)
{
    mnam::ToneStack ts;
    ts.prepare (fs);
    ts.set (bass, mid, treble);
    const int n = (int) fs;
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = (float) (0.1 * std::sin (melon::kTwoPi * freq * i / fs));
    ts.process (x.data(), n);
    // RMS over the second half (after the filters settle) against the input's RMS
    double sum = 0.0;
    for (int i = n / 2; i < n; ++i)
        sum += (double) x[(size_t) i] * x[(size_t) i];
    const double rms = std::sqrt (sum / (n - n / 2));
    return melon::gainToDb (rms / (0.1 / std::sqrt (2.0)));
}

int main()
{
    std::printf ("Melon NAM DSP test\n");

    // --- tone stack ---------------------------------------------------------------------
    check (std::abs (toneGainDb (5, 5, 5, 100.0)) < 0.05 && std::abs (toneGainDb (5, 5, 5, 1000.0)) < 0.05
               && std::abs (toneGainDb (5, 5, 5, 8000.0)) < 0.05,
           "Tone stack at 5 / 5 / 5 is flat");
    check (std::abs (toneGainDb (10, 5, 5, 30.0) - 20.0) < 1.5, "Bass 10 boosts the lows by about 20 dB");
    check (std::abs (toneGainDb (0, 5, 5, 30.0) + 20.0) < 1.5, "Bass 0 cuts the lows by about 20 dB");
    check (std::abs (toneGainDb (5, 10, 5, 425.0) - 15.0) < 0.3, "Middle 10 boosts 425 Hz by 15 dB");
    check (std::abs (toneGainDb (5, 0, 5, 425.0) + 15.0) < 0.3, "Middle 0 cuts 425 Hz by 15 dB");
    check (std::abs (toneGainDb (5, 5, 10, 15000.0) - 10.0) < 1.0, "Treble 10 boosts the highs by about 10 dB");
    check (std::abs (toneGainDb (5, 5, 0, 15000.0) + 10.0) < 1.0, "Treble 0 cuts the highs by about 10 dB");
    check (std::abs (toneGainDb (10, 5, 5, 4000.0)) < 1.0, "Bass leaves the highs alone");

    // --- gate ---------------------------------------------------------------------------
    {
        const double fs = 48000.0;
        mnam::Gate g;
        g.prepare (fs);
        g.setThresholdDb (-60.0);
        std::vector<float> quiet (48000), loud (48000);
        for (size_t i = 0; i < quiet.size(); ++i)
        {
            quiet[i] = (float) (0.0003 * std::sin (melon::kTwoPi * 200.0 * (double) i / fs)); // about -70 dB
            loud[i]  = (float) (0.3 * std::sin (melon::kTwoPi * 200.0 * (double) i / fs));    // about -10 dB
        }
        g.process (quiet.data(), (int) quiet.size());
        double qPeak = 0.0;
        for (size_t i = 24000; i < quiet.size(); ++i)
            qPeak = std::max (qPeak, (double) std::abs (quiet[i]));
        check (! g.isOpen() && qPeak < 1e-6, "Gate stays closed below the threshold");

        g.process (loud.data(), (int) loud.size());
        double lPeak = 0.0;
        for (size_t i = 24000; i < loud.size(); ++i)
            lPeak = std::max (lPeak, (double) std::abs (loud[i]));
        check (g.isOpen() && std::abs (lPeak - 0.3) < 0.01, "Gate opens and passes the signal above the threshold");
    }

    // --- compensation delay -----------------------------------------------------------------
    {
        mnam::CompensationDelay d;
        d.prepare (8192);
        d.setDelay (37);
        std::vector<float> x (200, 0.0f);
        x[0] = 1.0f;
        d.process (x.data(), (int) x.size());
        check (x[37] == 1.0f && x[0] == 0.0f, "Bypass delay matches the model latency");
    }

    // --- resampler (44.1 kHz session, 48 kHz model) ---------------------------------------------
    {
        mnam::ResampledProcessor rp;
        rp.prepare (44100.0, 48000.0, 512, 128);
        const int lat = rp.latencySamples();
        std::vector<float> sig (44100);
        for (size_t i = 0; i < sig.size(); ++i)
            sig[i] = (float) (0.5 * std::sin (melon::kTwoPi * 440.0 * (double) i / 44100.0));
        auto ref = sig;
        for (size_t pos = 0; pos < sig.size(); pos += 512)
        {
            const int n = (int) std::min<size_t> (512, sig.size() - pos);
            rp.process (sig.data() + pos, n, [] (const float* in, float* out, int k) { std::copy (in, in + k, out); });
        }
        double err = 0.0;
        for (size_t i = 4000; i < sig.size(); ++i)
            err = std::max (err, (double) std::abs (sig[i] - ref[i - (size_t) lat]));
        check (lat > 0 && err < 0.01, "Resampling round trip keeps the signal (with the reported latency)");
    }

    std::printf (failures == 0 ? "All tests passed\n" : "%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
