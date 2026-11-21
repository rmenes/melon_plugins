// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp
#include "SimpleGateDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

static std::vector<float> run (const sgate::Params& p, double fs, std::vector<float> sig)
{
    sgate::Engine e;
    e.prepare (fs, 1);
    e.setParams (p);
    e.snapToTargets();
    for (size_t pos = 0; pos < sig.size(); pos += 256)
    {
        float* blk[1] = { sig.data() + pos };
        e.process (blk, 1, (int) std::min<size_t> (256, sig.size() - pos));
    }
    return sig;
}

// note (0.5 s) - hiss at about -60 dBFS (0.5 s) - note (0.5 s)
static std::vector<float> burst (double fs, double freq)
{
    auto loud = testsig::sine (fs, freq, 0.3, 0.5);
    std::vector<float> s;
    s.insert (s.end(), loud.begin(), loud.end());
    melon::Noise nz;
    for (int i = 0; i < (int) (0.5 * fs); ++i) s.push_back ((float) (0.001 * nz.next()));
    s.insert (s.end(), loud.begin(), loud.end());
    return s;
}

// A note decaying slowly from -20 dB to -80 dB over 3 s; counts open/close changes (chatter)
static int chatter (const sgate::Params& p, double fs, double freq)
{
    std::vector<float> s ((size_t) (3.0 * fs));
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double t = (double) i / fs;
        s[i] = (float) (melon::dbToGain (-20.0 - 20.0 * t) * std::sin (2 * melon::kPi * freq * t));
    }
    sgate::Engine e;
    e.prepare (fs, 1);
    e.setParams (p);
    e.snapToTargets();
    int changes = 0;
    bool was = false;
    for (size_t pos = 0; pos < s.size(); pos += 64)
    {
        float* blk[1] = { s.data() + pos };
        e.process (blk, 1, (int) std::min<size_t> (64, s.size() - pos));
        if (e.isOpen() != was) { ++changes; was = e.isOpen(); }
    }
    return changes;
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const size_t q0 = (size_t) (0.75 * fs), q1 = (size_t) (0.98 * fs);
        const size_t l0 = (size_t) (0.1 * fs), l1 = (size_t) (0.4 * fs);

        for (auto inst : { sgate::Instrument::guitar, sgate::Instrument::bass })
        {
            const bool   bass = inst == sgate::Instrument::bass;
            const double f    = bass ? 41.2 : 82.4; // low E
            std::printf (" %s\n", bass ? "Bass" : "Guitar");
            sgate::Params p;
            p.instrument = inst;
            const auto in  = burst (fs, f);
            const auto out = run (p, fs, in);
            check (testsig::rmsDb (out, q0, q1) < -140.0 || testsig::rmsDb (out, q0, q1) - testsig::rmsDb (in, q0, q1) < -80.0,
                   "Hiss between notes is silenced");
            check (std::abs (testsig::rmsDb (out, l0, l1) - testsig::rmsDb (in, l0, l1)) < 0.05, "Notes pass untouched (low E)");
            const size_t onset = (size_t) (1.0 * fs);
            double first = 0.0;
            for (size_t i = onset; i < onset + (size_t) (0.004 * fs); ++i) first = std::max (first, (double) std::abs (out[i]));
            check (first > 0.25, "Opens fast enough to keep the attack");
            check (testsig::allFinite (out), "Finite output");

            p.holdMs = 0.0; p.releaseMs = 20.0;
            const int ch = chatter (p, fs, f);
            std::printf ("     open/close changes on a decaying low E: %d\n", ch);
            check (ch <= 2, "No chatter on a decaying low note (even with Hold 0)");
            const int chB = chatter (p, fs, bass ? 30.87 : 61.74);
            std::printf ("     open/close changes on a decaying low B (5-string bass / 7-string guitar): %d\n", chB);
            check (chB <= 2, "No chatter on the lowest B string");
        }
        {
            sgate::Params p; p.bypassed = true;
            const auto in = burst (fs, 330.0);
            check (testsig::maxDiff (run (p, fs, in), in) < 1e-6, "Bypass passes the signal untouched");
        }
        {
            sgate::Params p; p.thresholdDb = -80.0;
            const auto in = burst (fs, 330.0);
            check (std::abs (testsig::rmsDb (run (p, fs, in), q0, q1) - testsig::rmsDb (in, q0, q1)) < 0.5,
                   "Threshold under the hiss leaves the gate open");
        }
        {
            // Hold keeps the gate open through a short gap
            auto note = testsig::sine (fs, 196.0, 0.3, 0.3);
            std::vector<float> s (note);
            s.resize (s.size() + (size_t) (0.05 * fs), 0.0f);
            const size_t gapEnd = s.size();
            s.insert (s.end(), note.begin(), note.end());
            sgate::Params p; p.holdMs = 120.0;
            auto out = run (p, fs, s);
            double peakAfter = 0.0;
            for (size_t i = gapEnd; i < gapEnd + 32; ++i) peakAfter = std::max (peakAfter, (double) std::abs (out[i]) / std::max (1e-9, (double) std::abs (s[i]) + 1e-9));
            check (std::abs (testsig::rmsDb (out, gapEnd, gapEnd + (size_t) (0.01 * fs)) - testsig::rmsDb (s, gapEnd, gapEnd + (size_t) (0.01 * fs))) < 0.05,
                   "Hold 120 ms bridges a 50 ms gap (no re-attack)");
        }
    }
    std::printf (failures == 0 ? "\nAll tests passed\n" : "\n%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
