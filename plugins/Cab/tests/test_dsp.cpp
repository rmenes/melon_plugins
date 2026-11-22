// DSP test for Melon Cab (does not need JUCE; the convolution itself is JUCE's):
//   g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp
#include "CabDsp.h"
#include <cstdio>
#include <vector>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

static double gainDb (double lowCut, double highCut, double freq, double fs = 48000.0)
{
    cab::CutFilters f;
    f.prepare (fs);
    f.set (lowCut, highCut);
    const int n = (int) fs;
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = (float) (0.1 * std::sin (melon::kTwoPi * freq * i / fs));
    f.process (x.data(), n, 0);
    // RMS over the second half (after the filters settle) against the input's RMS
    double sum = 0.0;
    for (int i = n / 2; i < n; ++i)
        sum += (double) x[(size_t) i] * x[(size_t) i];
    const double rms = std::sqrt (sum / (n - n / 2));
    return melon::gainToDb (rms / (0.1 / std::sqrt (2.0)));
}

int main()
{
    std::printf ("Melon Cab DSP test\n");

    check (std::abs (gainDb (cab::kLowCutOff, cab::kHighCutOff, 40.0)) < 0.01
               && std::abs (gainDb (cab::kLowCutOff, cab::kHighCutOff, 15000.0)) < 0.01,
           "Filters at their end stops leave the signal untouched");
    check (std::abs (gainDb (100.0, cab::kHighCutOff, 100.0) + 3.0) < 0.3, "Low Cut is -3 dB at its frequency");
    check (gainDb (100.0, cab::kHighCutOff, 25.0) < -22.0, "Low Cut falls at 12 dB/oct below it");
    check (std::abs (gainDb (100.0, cab::kHighCutOff, 1000.0)) < 0.2, "Low Cut leaves the mids alone");
    check (std::abs (gainDb (cab::kLowCutOff, 5000.0, 5000.0) + 3.0) < 0.3, "High Cut is -3 dB at its frequency");
    check (gainDb (cab::kLowCutOff, 5000.0, 20000.0) < -22.0, "High Cut falls at 12 dB/oct above it");
    check (std::abs (gainDb (cab::kLowCutOff, 5000.0, 500.0)) < 0.2, "High Cut leaves the mids alone");

    // Both channels have their own filter state
    {
        cab::CutFilters f;
        f.prepare (48000.0);
        f.set (200.0, 4000.0);
        std::vector<float> a (64, 0.0f), b (64, 0.0f);
        a[0] = 1.0f;
        f.process (a.data(), 64, 0);
        f.process (b.data(), 64, 1);
        bool silent = true;
        for (float v : b)
            silent = silent && v == 0.0f;
        check (silent, "The right channel does not hear the left channel's filter state");
    }

    std::printf (failures == 0 ? "All tests passed\n" : "%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
