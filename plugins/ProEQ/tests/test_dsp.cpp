// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp -pthread
#include "ProEqDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>
#include <random>

using namespace proeq;

static int failures = 0;
static void check (bool ok, const char* what, double value = 0.0)
{
    std::printf ("  [%s] %s (%.2f)\n", ok ? " ok " : "FAIL", what, value);
    failures += ok ? 0 : 1;
}

static Band band (Type t, double f, double g = 0.0, double q = 1.0, int slope = 1, Placement pl = Placement::stereo)
{
    Band b;
    b.used = true;
    b.type = t;
    b.freq = f;
    b.gainDb = g;
    b.q = q;
    b.slope = slope;
    b.placement = pl;
    return b;
}

struct Stereo
{
    std::vector<float> l, r;
};

static Stereo run (const Params& p, double fs, Stereo s)
{
    auto e = std::make_unique<Engine>();
    e->setSynchronousKernels (true);
    e->prepare (fs, 2);
    e->setParams (p);
    e->snapToTargets();
    for (size_t pos = 0; pos < s.l.size(); pos += 256)
    {
        float* blk[2] = { s.l.data() + pos, s.r.data() + pos };
        e->process (blk, 2, (int) std::min<size_t> (256, s.l.size() - pos));
    }
    return s;
}

// Gain (dB) of a sine through the EQ, measured after it settles
static double sineGainDb (const Params& p, double fs, double freq, int channel = 0, Stereo* custom = nullptr)
{
    const double secs = p.phase == Phase::linear ? 0.8 : 0.4;
    auto sig = testsig::sine (fs, freq, 0.25, secs);
    Stereo in { sig, sig };
    if (custom) in = *custom;
    const auto out  = run (p, fs, in);
    const auto& o   = channel == 0 ? out.l : out.r;
    const auto& ref = channel == 0 ? in.l : in.r;
    const size_t a = (size_t) (0.5 * secs * fs), b = o.size();
    return 20.0 * std::log10 (testsig::rms (o, a, b) / testsig::rms (ref, a, b));
}

static double designDb (const Band& b, double fs, double f)
{
    Cascade c;
    designBand (b, b.gainDb, fs, c);
    return c.magnitudeDb (f, fs);
}

static bool near (double v, double target, double tol) { return std::abs (v - target) <= tol; }

int main()
{
    std::printf ("Filter shapes (48 kHz)\n");
    {
        const double fs = 48000.0;
        check (near (designDb (band (Type::bell, 1000, 6, 1), fs, 1000), 6.0, 0.01), "Bell +6 dB at its frequency", designDb (band (Type::bell, 1000, 6, 1), fs, 1000));
        check (near (designDb (band (Type::bell, 1000, 6, 1), fs, 100), 0.0, 0.3), "Bell: far away is flat", designDb (band (Type::bell, 1000, 6, 1), fs, 100));
        check (near (designDb (band (Type::lowShelf, 200, -8, 0.7071), fs, 20), -8.0, 0.2), "Low Shelf -8 dB below", designDb (band (Type::lowShelf, 200, -8, 0.7071), fs, 20));
        check (near (designDb (band (Type::lowShelf, 200, -8, 0.7071, 0), fs, 200), -4.0, 0.05), "Low Shelf 6 dB/oct: half the gain at f", designDb (band (Type::lowShelf, 200, -8, 0.7071, 0), fs, 200));
        check (near (designDb (band (Type::highShelf, 5000, 6, 0.7071), fs, 18000), 6.0, 0.3), "High Shelf +6 dB above", designDb (band (Type::highShelf, 5000, 6, 0.7071), fs, 18000));
        check (near (designDb (band (Type::highShelf, 5000, 6, 0.7071, 0), fs, 20), 0.0, 0.05), "High Shelf 6 dB/oct flat below", designDb (band (Type::highShelf, 5000, 6, 0.7071, 0), fs, 20));
        const auto tilt = band (Type::tiltShelf, 1000, 10, 0.7071);
        check (near (designDb (tilt, fs, 20), -5.0, 0.3) && near (designDb (tilt, fs, 20000), 5.0, 0.6) && near (designDb (tilt, fs, 1000), 0.0, 0.05),
               "Tilt Shelf: -5 dB lows, +5 dB highs, 0 dB at f", designDb (tilt, fs, 20000) - designDb (tilt, fs, 20));
        check (designDb (band (Type::notch, 1000, 0, 4), fs, 1000) < -60.0, "Notch: deep at f", designDb (band (Type::notch, 1000, 0, 4), fs, 1000));
        check (near (designDb (band (Type::bandPass, 1000, 0, 1, 0), fs, 1000), 0.0, 0.01), "Band Pass 6 dB/oct: 0 dB at f", designDb (band (Type::bandPass, 1000, 0, 1, 0), fs, 1000));
        check (near (designDb (band (Type::bandPass, 1000, 0, 2, 4), fs, 1000), 0.0, 0.01), "Band Pass 30 dB/oct: 0 dB at f", designDb (band (Type::bandPass, 1000, 0, 2, 4), fs, 1000));
        check (designDb (band (Type::bandPass, 1000, 0, 2, 4), fs, 250) < -45.0, "Band Pass 30 dB/oct: steep skirts", designDb (band (Type::bandPass, 1000, 0, 2, 4), fs, 250));

        for (int s = 0; s < kNumSlopes; ++s)
        {
            const auto lc = band (Type::lowCut, 1000, 0, 0.7071, s);
            const double at   = designDb (lc, fs, 1000);
            const double oct2 = designDb (lc, fs, 250) - designDb (lc, fs, 125); // per octave, far below
            char msg[96];
            std::snprintf (msg, sizeof (msg), "Low Cut %d dB/oct: -3 dB at f, slope %.1f dB/oct", kSlopes[s], oct2);
            check (near (at, -3.01, 0.1) && near (oct2, kSlopes[s], 0.6), msg, at);
        }
        const auto hc = band (Type::highCut, 2000, 0, 0.7071, 3);
        check (near (designDb (hc, fs, 2000), -3.01, 0.1) && designDb (hc, fs, 8000) < -45.0, "High Cut 24 dB/oct", designDb (hc, fs, 8000));
        check (designDb (band (Type::lowCut, 1000, 0, 4.0, 1), fs, 1000) > 10.0, "Cut Q above 0.71 adds a resonant peak", designDb (band (Type::lowCut, 1000, 0, 4.0, 1), fs, 1000));
    }

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);

        {   // all bands off = untouched
            Params p;
            const auto sig = testsig::pluckedChords (fs, 1.0);
            const auto out = run (p, fs, { sig, sig });
            check (testsig::maxDiff (out.l, sig) < 1.0e-6, "No bands: output = input", testsig::maxDiff (out.l, sig));
        }
        {   // a bell, measured with sines
            Params p;
            p.bands[0] = band (Type::bell, 1000, 6, 1);
            check (near (sineGainDb (p, fs, 1000), 6.0, 0.15), "Bell +6 dB @ 1 kHz (audio)", sineGainDb (p, fs, 1000));
            check (near (sineGainDb (p, fs, 100), 0.0, 0.3), "Bell: 100 Hz untouched", sineGainDb (p, fs, 100));
            p.bands[0].on = false;
            check (near (sineGainDb (p, fs, 1000), 0.0, 0.01), "Band switched off = flat", sineGainDb (p, fs, 1000));
        }
        {   // gain scale
            Params p;
            p.bands[0] = band (Type::bell, 1000, 6, 1);
            p.gainScale = 0.5;
            check (near (sineGainDb (p, fs, 1000), 3.0, 0.15), "Gain Scale 50 % halves the gain", sineGainDb (p, fs, 1000));
            p.gainScale = -1.0;
            check (near (sineGainDb (p, fs, 1000), -6.0, 0.15), "Gain Scale -100 % flips it", sineGainDb (p, fs, 1000));
        }
        {   // stereo placement
            Params p;
            p.bands[0] = band (Type::bell, 1000, 6, 1, 1, Placement::left);
            check (near (sineGainDb (p, fs, 1000, 0), 6.0, 0.15) && near (sineGainDb (p, fs, 1000, 1), 0.0, 0.01), "Left band only touches the left channel", sineGainDb (p, fs, 1000, 1));
            p.bands[0].placement = Placement::side;
            check (near (sineGainDb (p, fs, 1000, 0), 0.0, 0.01), "Side band ignores a mono (L = R) signal", sineGainDb (p, fs, 1000, 0));
            p.bands[0].placement = Placement::mid;
            check (near (sineGainDb (p, fs, 1000, 0), 6.0, 0.15), "Mid band acts on a mono (L = R) signal", sineGainDb (p, fs, 1000, 0));
            auto sig = testsig::sine (fs, 1000, 0.25, 0.4);
            Stereo anti { sig, testsig::scaled (sig, -1.0f) }; // pure Side
            p.bands[0].placement = Placement::side;
            check (near (sineGainDb (p, fs, 1000, 0, &anti), 6.0, 0.15), "Side band acts on an L = -R signal", sineGainDb (p, fs, 1000, 0, &anti));
        }
        {   // dynamic EQ
            Params p;
            p.bands[0] = band (Type::bell, 2000, 0, 1);
            p.bands[0].dynRangeDb     = -9.0;
            p.bands[0].dynThresholdDb = -40.0;
            check (near (sineGainDb (p, fs, 2000), -9.0, 0.3), "Dynamic: loud signal gets the full range (-9 dB)", sineGainDb (p, fs, 2000));
            auto quiet = testsig::sine (fs, 2000, 0.002, 0.4); // about -57 dBFS, under the threshold
            Stereo q { quiet, quiet };
            check (near (sineGainDb (p, fs, 2000, 0, &q), 0.0, 0.1), "Dynamic: quiet signal is untouched", sineGainDb (p, fs, 2000, 0, &q));
            p.bands[0].dynRangeDb = 6.0;
            check (near (sineGainDb (p, fs, 2000), 6.0, 0.3), "Dynamic: positive range boosts when loud", sineGainDb (p, fs, 2000));
        }
        {   // linear phase
            Params p;
            p.phase = Phase::linear;
            p.bands[0] = band (Type::bell, 1000, 6, 1);
            p.bands[1] = band (Type::lowCut, 80, 0, 0.7071, 3);
            const double g = sineGainDb (p, fs, 1000);
            check (near (g, 6.0, 0.3), "Linear Phase: same gain as Zero Latency (+6 dB)", g);
            check (sineGainDb (p, fs, 30) < -25.0, "Linear Phase: Low Cut 24 dB/oct removes 30 Hz", sineGainDb (p, fs, 30));

            // impulse response: symmetric around the latency (linear phase)
            auto e = std::make_unique<Engine>();
            e->setSynchronousKernels (true);
            e->prepare (fs, 1);
            e->setParams (p);
            e->snapToTargets();
            std::vector<float> imp ((size_t) 20000, 0.0f);
            float* blk[1] = { imp.data() };
            e->process (blk, 1, 64); // first block switches the mode and designs the kernel
            const int L = e->latencySamples();
            std::fill (imp.begin(), imp.end(), 0.0f);
            imp[0] = 1.0f;
            for (size_t pos = 0; pos < imp.size(); pos += 256)
            {
                float* b2[1] = { imp.data() + pos };
                e->process (b2, 1, (int) std::min<size_t> (256, imp.size() - pos));
            }
            size_t peakAt = 0;
            for (size_t i = 0; i < imp.size(); ++i)
                if (std::abs (imp[i]) > std::abs (imp[peakAt])) peakAt = i;
            double asym = 0.0;
            for (int k = 1; k < L / 2; ++k)
                asym = std::max (asym, (double) std::abs (imp[(size_t) (L + k)] - imp[(size_t) (L - k)]));
            check (L == kernelSize (1, fs), "Linear Phase Medium: latency = kernel size", L);
            check ((int) peakAt == L && asym < 1.0e-4, "Linear Phase: symmetric impulse centred on the latency", asym);

            p.bands[1].placement = Placement::side; // M/S path (cross kernels)
            p.bands[0].placement = Placement::mid;
            check (near (sineGainDb (p, fs, 1000), 6.0, 0.3), "Linear Phase: Mid band on mono signal", sineGainDb (p, fs, 1000));

            p.bands[0].dynRangeDb = -6.0;
            p.bands[0].dynThresholdDb = -40.0;
            p.bands[0].gainDb = 0.0;
            check (near (sineGainDb (p, fs, 1000), -6.0, 0.4), "Linear Phase: dynamic band still works", sineGainDb (p, fs, 1000));
        }
        {   // auto gain
            Params p;
            p.bands[0] = band (Type::highShelf, 1000, 9, 0.7071);
            p.autoGain = true;
            const double g = sineGainDb (p, fs, 100);
            check (g < -2.0 && g > -9.0, "Auto Gain lowers the output for a big boost", g);
        }
        {   // solo
            Params p;
            p.bands[3] = band (Type::bell, 500, 3, 2);
            p.solo = 3;
            check (sineGainDb (p, fs, 4000) < -15.0 && near (sineGainDb (p, fs, 500), 3.0, 0.3), "Solo keeps only the band's region", sineGainDb (p, fs, 4000));
        }
        {   // bypass
            Params p;
            p.bands[0] = band (Type::bell, 1000, 12, 1);
            p.bypassed = true;
            check (near (sineGainDb (p, fs, 1000), 0.0, 0.01), "Bypass", sineGainDb (p, fs, 1000));
        }
        {   // stress: random settings changing every block, both modes
            std::mt19937 rng (7);
            std::uniform_real_distribution<double> u (0.0, 1.0);
            for (int mode = 0; mode < 2; ++mode)
            {
                auto e = std::make_unique<Engine>();
                e->setSynchronousKernels (true);
                e->prepare (fs, 2);
                auto sig = testsig::pluckedChords (fs, 1.5);
                auto sig2 = sig;
                bool finite = true;
                double peak = 0.0;
                for (size_t pos = 0; pos < sig.size(); pos += 512)
                {
                    Params p;
                    p.phase = mode ? Phase::linear : Phase::zeroLatency;
                    for (int b = 0; b < kNumBands; ++b)
                    {
                        auto& x = p.bands[(size_t) b];
                        x.used = u (rng) < 0.5;
                        x.type = (Type) (int) (u (rng) * kNumTypes);
                        x.freq = 20.0 * std::pow (1000.0, u (rng));
                        x.gainDb = -12.0 + 24.0 * u (rng);
                        x.q = 0.3 * std::pow (20.0, u (rng));
                        x.slope = (int) (u (rng) * kNumSlopes);
                        x.placement = (Placement) (int) (u (rng) * 5);
                        x.dynRangeDb = u (rng) < 0.3 ? -12.0 + 24.0 * u (rng) : 0.0;
                        x.dynThresholdDb = -60.0 * u (rng);
                    }
                    e->setParams (p);
                    const int n = (int) std::min<size_t> (512, sig.size() - pos);
                    float* blk[2] = { sig.data() + pos, sig2.data() + pos };
                    e->process (blk, 2, n);
                }
                finite = testsig::allFinite (sig) && testsig::allFinite (sig2);
                peak = std::max (testsig::peak (sig), testsig::peak (sig2));
                check (finite && peak < 40.0, mode ? "Random automation (Linear Phase): stable" : "Random automation (Zero Latency): stable", peak);
            }
        }
    }

    {   // background kernel designer (as used in the plugin)
        std::printf ("Background designer\n");
        Params p;
        p.phase = Phase::linear;
        p.resolution = 0;
        p.bands[0] = band (Type::bell, 1000, 6, 1);
        auto e = std::make_unique<Engine>();
        e->prepare (48000.0, 1);
        e->setParams (p);
        e->snapToTargets();
        auto sig = testsig::sine (48000.0, 1000.0, 0.25, 0.1);
        std::vector<float> out;
        for (int rep = 0; rep < 12; ++rep)
        {
            auto blk = sig;
            float* d[1] = { blk.data() };
            for (size_t pos = 0; pos < blk.size(); pos += 256)
            {
                float* b2[1] = { blk.data() + pos };
                e->process (b2, 1, (int) std::min<size_t> (256, blk.size() - pos));
            }
            (void) d;
            out.insert (out.end(), blk.begin(), blk.end());
            std::this_thread::sleep_for (std::chrono::milliseconds (20));
        }
        const double g = 20.0 * std::log10 (testsig::rms (out, out.size() - 4800, out.size()) / testsig::rms (sig));
        check (near (g, 6.0, 0.3), "Kernel designed on the worker thread reaches the audio", g);
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
