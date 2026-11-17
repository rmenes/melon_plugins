// DSP test for Melon Rack (does not need JUCE or the neural engine):
//   g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp
#include "Resampler.h"
#include "RackDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

// Runs `sig` through a ResampledProcessor (host fs, inner rate innerFs) whose inner processor is
// a plain gain, with irregular block sizes. Returns the output and the reported latency.
static std::vector<float> roundTrip (std::vector<float> sig, double fs, double innerFs, int& latency, double& innerCount)
{
    rack::ResampledProcessor rp;
    const int maxBlock = 512;
    rp.prepare (fs, innerFs, maxBlock, 128);
    latency    = rp.latencySamples();
    innerCount = 0.0;
    const int sizes[] = { 512, 37, 256, 1, 128, 499, 64, 300 };
    size_t pos = 0;
    for (int b = 0; pos < sig.size(); ++b)
    {
        const int n = (int) std::min<size_t> ((size_t) sizes[b % 8], sig.size() - pos);
        rp.process (sig.data() + pos, n, [&] (const float* in, float* out, int k) {
            if (k > 128)
                check (false, "Inner processor never gets more than its chunk size");
            for (int i = 0; i < k; ++i)
                out[i] = 0.5f * in[i];
            innerCount += k;
        });
        pos += (size_t) n;
    }
    return sig;
}

int main()
{
    std::printf ("Resampling\n");
    for (auto rates : { std::pair<double, double> { 48000.0, 48000.0 }, { 44100.0, 48000.0 }, { 96000.0, 48000.0 },
                        { 88200.0, 48000.0 }, { 192000.0, 48000.0 }, { 48000.0, 44100.0 }, { 22050.0, 48000.0 } })
    {
        const double fs = rates.first, inner = rates.second;
        std::printf (" host %.0f Hz, model %.0f Hz\n", fs, inner);

        const double freq = std::min (1000.0, 0.2 * std::min (fs, inner));
        const auto   in   = testsig::sine (fs, freq, 0.5, 1.0);
        int    lat   = 0;
        double count = 0.0;
        const auto out = roundTrip (in, fs, inner, lat, count);

        // compare against the input delayed by the reported latency (0.5 = inner gain)
        double err = 0.0, ref = 0.0;
        for (size_t i = (size_t) (0.1 * fs); i < out.size(); ++i)
        {
            const double expect = 0.5 * in[i - (size_t) lat];
            err += (out[i] - expect) * (out[i] - expect);
            ref += expect * expect;
        }
        const double snrDb = 10.0 * std::log10 (ref / std::max (1e-30, err));
        std::printf ("    latency %d samples, round-trip SNR %.1f dB\n", lat, snrDb);
        check (snrDb > 60.0, "Output = input delayed by exactly the reported latency");
        check (std::abs (count - (double) in.size() * inner / fs) < 64.0, "Model sees the right number of samples");
        if (fs == inner)
            check (lat == 0, "No latency when the rates match");
        else
            check (lat > 0 && lat < 400, "Small, constant latency when resampling");

        // aliasing: a tone above the model's Nyquist must be filtered out when going down
        if (fs > inner)
        {
            const auto hi = testsig::sine (fs, 0.5 * inner * 1.08, 0.5, 0.5);
            const auto o  = roundTrip (hi, fs, inner, lat, count);
            const double lvl = testsig::rms (o, (size_t) (0.1 * fs));
            check (lvl < 0.5 * 0.5 * 0.7071 * 0.01, "Content above the model's Nyquist is removed (> 40 dB)");
        }
    }

    std::printf ("EQ\n");
    {
        rack::Eq6 eq;
        eq.prepare (48000.0);
        auto x = testsig::sine (48000.0, 440.0, 0.5, 0.5);
        auto y = x;
        eq.process (y.data(), (int) y.size());
        double d = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
            d = std::max (d, (double) std::abs (x[i] - y[i]));
        check (d < 1e-6, "All bands at 0 dB: signal unchanged");

        eq.setBand (2, 9.0);
        check (std::abs (eq.responseDb (500.0) - 9.0) < 0.3, "+9 dB at the 500 Hz band");
        eq.setBand (2, 0.0);
        eq.setBand (5, -12.0);
        check (std::abs (eq.responseDb (6000.0) + 12.0) < 0.3, "-12 dB at the 6 kHz band");
        check (std::abs (eq.responseDb (100.0)) < 0.5, "Far bands untouched");
    }

    std::printf ("Gate\n");
    {
        const double fs = 48000.0;
        rack::Gate gate;
        gate.prepare (fs);
        gate.setThresholdDb (-50.0);
        std::vector<float> s = testsig::sine (fs, 220.0, 0.3, 0.5);
        melon::Noise nz;
        for (int i = 0; i < (int) (0.5 * fs); ++i)
            s.push_back ((float) (0.0005 * nz.next())); // about -70 dBFS hiss
        const auto in = s;
        gate.process (s.data(), (int) s.size());
        const double loud = testsig::rms (s, (size_t) (0.1 * fs), (size_t) (0.45 * fs));
        const double tail = testsig::rms (s, (size_t) (0.8 * fs), s.size());
        check (std::abs (loud / testsig::rms (in, (size_t) (0.1 * fs), (size_t) (0.45 * fs)) - 1.0) < 0.01, "Playing: gate open, level unchanged");
        check (tail < 1e-5, "Hiss after the note: gate closed");
    }

    std::printf ("Compensation delay\n");
    {
        rack::CompensationDelay d;
        d.prepare (4096);
        d.setDelay (37);
        std::vector<float> x (200, 0.0f);
        x[0] = 1.0f;
        d.process (x.data(), (int) x.size());
        check (x[37] == 1.0f && x[0] == 0.0f, "Impulse comes out 37 samples later");
    }

    std::printf (failures == 0 ? "\nAll tests passed.\n" : "\n%d test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
