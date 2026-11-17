// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "GateDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

static std::vector<float> run (const gate::Params& p, double fs, std::vector<float> sig, int* latency = nullptr)
{
    gate::Engine e;
    e.prepare (fs, 1);
    e.setParams (p);
    e.snapToTargets();
    if (latency) *latency = e.latencySamples();
    for (size_t pos = 0; pos < sig.size(); pos += 256)
    {
        float* blk[1] = { sig.data() + pos };
        e.process (blk, 1, (int) std::min<size_t> (256, sig.size() - pos));
    }
    return sig;
}

// loud tone (0.5 s) - quiet hiss (0.5 s) - loud tone (0.5 s)
static std::vector<float> burst (double fs)
{
    auto loud = testsig::sine (fs, 330.0, 0.5, 0.5);
    std::vector<float> s;
    s.insert (s.end(), loud.begin(), loud.end());
    melon::Noise nz;
    for (int i = 0; i < (int) (0.5 * fs); ++i) s.push_back ((float) (0.001 * nz.next())); // about -60 dBFS
    s.insert (s.end(), loud.begin(), loud.end());
    return s;
}

int main()
{
    // static curve
    check (std::abs (gate::gainComputerDb (-20, -40, 100, 0, 60, false)) < 1e-9, "Above threshold: no attenuation");
    check (std::abs (gate::gainComputerDb (-60, -40, 100, 0, 60, false) + 60.0) < 1e-9, "Far below threshold: attenuation = Range");
    check (std::abs (gate::gainComputerDb (-50, -40, 2, 0, 60, false) + 10.0) < 1e-9, "Ratio 2 expander: 10 dB under -> 10 dB more");
    check (gate::gainComputerDb (-40, -40, 2, 6, 60, false) < 0.0 && gate::gainComputerDb (-40, -40, 2, 6, 60, false) > -3.0,
           "Knee softens the corner");
    check (std::abs (gate::gainComputerDb (-10, -40, 100, 0, 20, true) + 20.0) < 1e-9, "Ducking attenuates loud side-chain");

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const auto in   = burst (fs);
        const size_t q0 = (size_t) (0.7 * fs), q1 = (size_t) (0.95 * fs); // inside the quiet part, after release
        const size_t l0 = (size_t) (0.1 * fs), l1 = (size_t) (0.4 * fs);  // inside the first loud part

        {
            gate::Params p;
            auto out = run (p, fs, in);
            check (testsig::rmsDb (out, q0, q1) - testsig::rmsDb (in, q0, q1) < -55.0, "Gate closes on the quiet part (~ Range)");
            check (std::abs (testsig::rmsDb (out, l0, l1) - testsig::rmsDb (in, l0, l1)) < 0.1, "Loud part passes untouched");
            check (testsig::allFinite (out), "Finite output");
        }
        {
            gate::Params p; p.rangeDb = 12.0;
            auto out = run (p, fs, in);
            check (std::abs (testsig::rmsDb (out, q0, q1) - testsig::rmsDb (in, q0, q1) + 12.0) < 0.5, "Range 12 dB limits the attenuation");
        }
        {
            gate::Params p; p.bypassed = true;
            check (testsig::maxDiff (run (p, fs, in), in) < 1e-6, "Bypass (no lookahead) passes the signal untouched");
        }
        {
            gate::Params p; p.mix = 0.0;
            check (testsig::maxDiff (run (p, fs, in), in) < 1e-6, "Mix 0 % passes the dry signal");
        }
        {
            gate::Params p; p.lookaheadMs = 5.0;
            int lat = 0;
            auto out = run (p, fs, in, &lat);
            check (lat == (int) std::lround (0.005 * fs), "Lookahead reports its latency");
            // with lookahead, the first loud sample after the quiet part comes out at (almost) full level
            const size_t onset = (size_t) (1.0 * fs) + (size_t) lat;
            double ratio = 0.0;
            for (size_t i = onset; i < onset + (size_t) (0.002 * fs); ++i) ratio = std::max (ratio, (double) std::abs (out[i]));
            check (ratio > 0.3, "Lookahead opens the gate before the note starts");
        }
        {
            gate::Params p; p.mode = gate::Mode::ducking; p.thresholdDb = -30.0; p.rangeDb = 20.0; p.ratio = 100.0;
            auto out = run (p, fs, in);
            check (std::abs (testsig::rmsDb (out, l0, l1) - testsig::rmsDb (in, l0, l1) + 20.0) < 0.5, "Ducking turns loud parts down by Range");
            check (std::abs (testsig::rmsDb (out, q0, q1) - testsig::rmsDb (in, q0, q1)) < 0.5, "Ducking leaves quiet parts alone");
        }
        {
            gate::Params p; p.mode = gate::Mode::clean;
            auto out = run (p, fs, in);
            const size_t c0 = (size_t) (0.85 * fs); // Clean mode releases 1.5x slower
            check (testsig::rmsDb (out, c0, q1) - testsig::rmsDb (in, c0, q1) < -55.0, "Clean mode closes too");
        }
        {
            // a slowly decaying note around the threshold: hysteresis avoids chatter
            std::vector<float> s ((size_t) (2.0 * fs));
            for (size_t i = 0; i < s.size(); ++i)
            {
                const double t = (double) i / fs;
                const double lvl = -36.0 - 8.0 * t + 1.5 * std::sin (2 * melon::kPi * 7.0 * t); // wobbles around -40 dB
                s[i] = (float) (melon::dbToGain (lvl) * std::sin (2 * melon::kPi * 220.0 * t));
            }
            auto count = [&] (gate::Mode mode, double hyst) {
                gate::Params p; p.mode = mode; p.hysteresisDb = hyst; p.holdMs = 0.0; p.releaseMs = 20.0;
                auto out = run (p, fs, s);
                int changes = 0; bool o = true;
                for (size_t i = 0; i < out.size(); i += (size_t) (fs / 1000))
                {
                    const bool now = std::abs (out[i]) > 0.5 * std::abs (s[i]) || std::abs (s[i]) < 1e-4;
                    if (now != o) { ++changes; o = now; }
                }
                return changes;
            };
            const int none = count (gate::Mode::classic, 0.0), guitar = count (gate::Mode::guitar, 3.0);
            std::printf ("     open/close changes: no hysteresis %d, guitar mode %d\n", none, guitar);
            check (guitar < none, "Hysteresis (Guitar mode) reduces chatter");
        }
    }
    std::printf (failures == 0 ? "\nAll tests passed\n" : "\n%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
