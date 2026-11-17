// DSP core test (does not need JUCE):  g++ -std=c++17 -O2 -I../Source test_dsp.cpp -o test_dsp
#include "FlangerDsp.h"
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

static double gFreq = 440.0, gAmp = 0.1;
static float impulse (int i, double) { return i == 0 ? 1.0f : 0.0f; }
static float sine (int i, double fs) { return (float) (gAmp * std::sin (2.0 * flanger::kPi * gFreq * i / fs)); }
static float noiseBurst (int i, double fs)
{
    static unsigned s = 1;
    s = s * 1664525u + 1013904223u;
    return i < (int) fs ? 0.5f * ((float) (s >> 9) / 4194304.0f - 1.0f) : 0.0f; // 1 s of noise, then silence
}

// Runs a mono test signal through the engine (copied to both channels)
static Run run (const flanger::Params& p, double fs, int numSamples, float (*signal) (int, double))
{
    flanger::Engine e;
    e.prepare (fs, 2);
    e.setParams (p);
    e.snapToTargets();

    Run out { std::vector<float> ((size_t) numSamples), std::vector<float> ((size_t) numSamples) };
    for (int i = 0; i < numSamples; ++i)
        out.l[(size_t) i] = out.r[(size_t) i] = signal (i, fs);

    for (int pos = 0; pos < numSamples; pos += 512)
    {
        float* blk[2] = { out.l.data() + pos, out.r.data() + pos };
        e.process (blk, 2, std::min (512, numSamples - pos));
    }
    return out;
}

static double maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double m = 0;
    for (size_t i = 0; i < a.size(); ++i)
        m = std::max (m, (double) std::abs (a[i] - b[i]));
    return m;
}

// Steady-state gain (dB) of a sine through a static flanger (Width 0)
static double measuredGainDb (flanger::Params p, double fs, double freq)
{
    gFreq = freq;
    gAmp  = 0.01; // small, so the soft clip stays linear
    auto out = run (p, fs, (int) fs, sine);
    double peak = 0;
    for (size_t i = out.l.size() / 2; i < out.l.size(); ++i)
        peak = std::max (peak, (double) std::abs (out.l[i]));
    return 20.0 * std::log10 (peak / gAmp);
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const int n = (int) fs * 2;
        gFreq = 440.0;
        gAmp  = 0.5;
        std::vector<float> dry ((size_t) n);
        for (int i = 0; i < n; ++i) dry[(size_t) i] = sine (i, fs);

        // 1. Mix 0 → output is exactly the input, even with Regen up
        {
            flanger::Params p;
            p.mix   = 0.0;
            p.regen = 1.0;
            auto out = run (p, fs, n, sine);
            check (maxDiff (out.l, dry) < 1e-6, "Mix 0 % passes the dry signal untouched");
        }

        // 2. Bypass → output is exactly the input
        {
            flanger::Params p;
            p.bypassed = true;
            p.regen    = 1.0;
            auto out = run (p, fs, n, sine);
            check (maxDiff (out.l, dry) < 1e-6, "Bypass passes the dry signal untouched");
        }

        // 3. Width 0, Regen 0, Mix 100 → a single echo at the Manual delay, unity gain
        {
            flanger::Params p;
            p.width    = 0.0;
            p.regen    = 0.0;
            p.mix      = 1.0;
            p.manualMs = 3.0;
            auto out = run (p, fs, n, impulse);
            size_t peakAt = 0;
            double sum    = 0;
            for (size_t i = 0; i < out.l.size(); ++i)
            {
                sum += out.l[i];
                if (std::abs (out.l[i]) > std::abs (out.l[peakAt])) peakAt = i;
            }
            const double ms = 1000.0 * (double) peakAt / fs;
            check (std::abs (ms - 3.0) < 0.05, "Width 0: echo sits at the Manual delay (3 ms)");
            check (std::abs (sum - 1.0) < 1e-3, "Regen 0: the delayed signal has unity gain");
        }

        // 4. Comb filter: Mix 50 %, Regen 0 → deep notch at 1/(2D), +6 dB peak at 1/D
        {
            flanger::Params p;
            p.width    = 0.0;
            p.regen    = 0.0;
            p.mix      = 0.5;
            p.manualMs = 2.0;
            const double notch = measuredGainDb (p, fs, flanger::firstNotchHz (2.0)); // 250 Hz
            const double peak  = measuredGainDb (p, fs, 500.0);
            std::printf ("    notch at 250 Hz %+6.1f dB, peak at 500 Hz %+5.1f dB\n", notch, peak);
            check (notch < -30.0, "Mix 50 %: deep notch at 1/(2 x delay)");
            check (std::abs (peak - 6.0) < 0.3, "Mix 50 %: +6 dB peak at 1/delay");
        }

        // 5. Regen makes the peaks resonate: the peak (500 Hz) stands out more from the
        //    point halfway to the next notch (625 Hz)
        {
            flanger::Params p;
            p.width    = 0.0;
            p.mix      = 0.5;
            p.manualMs = 2.0;
            p.regen    = 0.0;
            const double sharp0 = measuredGainDb (p, fs, 500.0) - measuredGainDb (p, fs, 625.0);
            p.regen = 0.8;
            const double sharp8 = measuredGainDb (p, fs, 500.0) - measuredGainDb (p, fs, 625.0);
            std::printf ("    peak sharpness at Regen 0: %.1f dB, at Regen 80: %.1f dB\n", sharp0, sharp8);
            check (sharp8 > sharp0 + 6.0, "Regen turns the peaks into sharp resonances");
        }

        // 6. Display maths matches the audio (static settings, small signal)
        {
            flanger::Params p;
            p.width    = 0.0;
            p.mix      = 0.5;
            p.manualMs = 1.3;
            p.regen    = 0.7;
            double worst = 0;
            for (double f : { 120.0, 333.0, 1000.0, 2700.0, 6000.0 })
            {
                const double meas = measuredGainDb (p, fs, f);
                const double calc = flanger::responseDb (f, fs, p, 1.3);
                if (calc > -25.0) worst = std::max (worst, std::abs (meas - calc));
            }
            std::printf ("    display vs measured: worst %.2f dB\n", worst);
            check (worst < 0.5, "Response display matches the processed audio");
        }

        // 7. Left and right identical (one LFO, like the mono pedal)
        {
            flanger::Params p;
            p.width = 1.0;
            p.regen = 0.7;
            gAmp    = 0.5;
            auto out = run (p, fs, n, sine);
            check (maxDiff (out.l, out.r) < 1e-6, "Left and right sweep together");
        }

        // 8. Extremes: max Regen, max Speed, noise in → finite, bounded, and dies away after the input stops
        {
            flanger::Params p;
            p.regen    = 1.0;
            p.width    = 1.0;
            p.speedHz  = flanger::kMaxSpeedHz;
            p.manualMs = flanger::kMinManualMs;
            p.mix      = 0.5;
            auto out = run (p, fs, (int) fs * 6, noiseBurst);
            bool   finite = true;
            double peak = 0, tail = 0;
            for (size_t i = 0; i < out.l.size(); ++i)
            {
                finite &= std::isfinite (out.l[i]);
                peak = std::max (peak, (double) std::abs (out.l[i]));
                if (i > (size_t) fs * 5) tail = std::max (tail, (double) std::abs (out.l[i]));
            }
            std::printf ("    peak %.2f, level 4 s after the input stops %.1e\n", peak, tail);
            check (finite && peak < 3.0, "Max Regen/Speed: output finite and bounded");
            check (tail < 1e-4, "Max Regen: the ringing dies away");
        }
    }

    // 9. Sweep range follows Manual and Width
    {
        std::printf ("Sweep\n");
        check (std::abs (flanger::delayMs (-1.0, 2.0, 1.0) - 2.0 / std::exp2 (1.5)) < 1e-9
                   && std::abs (flanger::delayMs (1.0, 2.0, 1.0) - 2.0 * std::exp2 (1.5)) < 1e-9,
               "Width 100 %: sweep spans 1.5 octaves either side of Manual");
        check (flanger::delayMs (1.0, 2.0, 0.0) == 2.0 && flanger::delayMs (-1.0, 2.0, 0.0) == 2.0,
               "Width 0 %: delay stays at Manual (static flange)");
        check (flanger::delayMs (-1.0, 0.25, 1.0) >= flanger::kMinDelayMs && flanger::delayMs (1.0, 10.0, 1.0) <= flanger::kMaxDelayMs,
               "Sweep stays inside the delay limits");
        check (flanger::triangle (0.0) == 1.0 && flanger::triangle (0.5) == -1.0 && flanger::triangle (0.25) == 0.0,
               "Triangle LFO shape");
    }

    std::printf ("\n%s\n", failures == 0 ? "All tests passed." : "SOME TESTS FAILED.");
    return failures == 0 ? 0 : 1;
}
