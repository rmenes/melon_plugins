// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "ShifterDsp.h"
#include "../../../shared/TestSignals.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

// Runs the engine in blocks. Stereo when `right` is given.
static std::vector<float> run (const shifter::Params& p, double fs, std::vector<float> sig, std::vector<float>* right = nullptr)
{
    shifter::Engine e;
    const int nch = right ? 2 : 1;
    e.prepare (fs, nch);
    e.setParams (p);
    e.snapToTargets();
    if (right)
        *right = sig;
    for (size_t pos = 0; pos < sig.size(); pos += 256)
    {
        float* blk[2] = { sig.data() + pos, right ? right->data() + pos : nullptr };
        e.process (blk, nch, (int) std::min<size_t> (256, sig.size() - pos));
    }
    return sig;
}

// Fraction of the signal's energy at `freq` (single-bin DFT over [from, to))
static double energyAt (const std::vector<float>& v, double fs, double freq, size_t from, size_t to)
{
    double s = 0.0, c = 0.0, e = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        const double ph = 2.0 * testsig::kPi * freq * (double) i / fs;
        s += v[i] * std::sin (ph);
        c += v[i] * std::cos (ph);
        e += (double) v[i] * v[i];
    }
    const double n = (double) (to - from);
    return e > 0.0 ? 2.0 * (s * s + c * c) / (n * e) : 0.0;
}

// Average purity over 100 ms segments (a splice shifter keeps the pitch exact but its phase
// can wander very slightly over seconds, which a single long DFT would count as impurity)
static double purityAt (const std::vector<float>& v, double fs, double freq, size_t from, size_t to)
{
    const size_t seg = (size_t) (0.1 * fs);
    double sum = 0.0;
    int    n   = 0;
    for (size_t i = from; i + seg <= to; i += seg, ++n)
        sum += energyAt (v, fs, freq, i, i + seg);
    return n > 0 ? sum / n : 0.0;
}

// Frequency from positive-going zero crossings (linear interpolation)
static double zeroCrossFreq (const std::vector<float>& v, double fs, size_t from, size_t to)
{
    double first = -1.0, last = -1.0;
    int    count = 0;
    for (size_t i = from + 1; i < to; ++i)
        if (v[i - 1] < 0.0f && v[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + v[i - 1] / (double) (v[i - 1] - v[i]);
            if (first < 0.0) first = t;
            last = t;
            ++count;
        }
    return count > 1 ? (count - 1) * fs / (last - first) : 0.0;
}

// Largest level change between 5 ms frames (amplitude wobble from the splices)
static double wobbleDb (const std::vector<float>& v, double fs, size_t from, size_t to)
{
    const size_t frame = (size_t) (0.005 * fs);
    double lo = 1e9, hi = -1e9;
    for (size_t i = from; i + frame <= to; i += frame)
    {
        const double db = testsig::rmsDb (v, i, i + frame);
        lo = std::min (lo, db);
        hi = std::max (hi, db);
    }
    return hi - lo;
}

int main()
{
    check (std::abs (shifter::ratioFor (12, 0) - 2.0) < 1e-12 && std::abs (shifter::ratioFor (-12, 0) - 0.5) < 1e-12, "Ratios");
    check (shifter::intervalName (7) == "Perfect 5th up" && shifter::intervalName (-12) == "Octave down"
               && shifter::intervalName (19) == "Octave + Perfect 5th up" && shifter::intervalName (0) == "Unison",
           "Interval names");

    const shifter::Mode modes[] = { shifter::Mode::tight, shifter::Mode::normal, shifter::Mode::smooth };
    const char* modeNames[]     = { "Tight", "Normal", "Smooth" };

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const double f0 = 220.0;
        const auto   in = testsig::sine (fs, f0, 0.3, 2.0);
        const size_t a0 = (size_t) (0.4 * fs), a1 = (size_t) (1.9 * fs);

        shifter::Params wet;
        wet.mix = 1.0;

        for (int mi = 0; mi < 3; ++mi)
        {
            std::printf ("  Mode %s\n", modeNames[mi]);
            for (double semis : { 12.0, 7.0, -12.0, -5.0, 24.0 })
            {
                shifter::Params p = wet;
                p.mode   = modes[mi];
                p.pitchA = semis;
                const auto out    = run (p, fs, in);
                const double tgt  = f0 * shifter::ratioFor (semis, 0.0);
                const double zf   = zeroCrossFreq (out, fs, a0, a1);
                const double frac = purityAt (out, fs, tgt, a0, a1);
                const double lvl  = testsig::rmsDb (out, a0, a1) - testsig::rmsDb (in, a0, a1);
                const double wob  = wobbleDb (out, fs, a0, a1);
                std::printf ("        %+3.0f st: %.2f Hz (target %.2f), %.1f %% energy at target, level %+.2f dB, wobble %.2f dB\n",
                             semis, zf, tgt, 100.0 * frac, lvl, wob);
                char msg[128];
                std::snprintf (msg, sizeof msg, "%+.0f st lands on the right pitch", semis);
                check (testsig::allFinite (out) && std::abs (zf / tgt - 1.0) < 0.001 && frac > 0.97, msg);
                std::snprintf (msg, sizeof msg, "%+.0f st keeps the level (no dips at the splices)", semis);
                check (std::abs (lvl) < 1.5 && wob < 3.0, msg);
            }
        }

        // Detune: +10 cents
        {
            shifter::Params p = wet;
            p.pitchA = 0.0; p.detuneA = 10.0;
            const auto out = run (p, fs, in);
            const double zf = zeroCrossFreq (out, fs, a0, a1);
            check (std::abs (zf / (f0 * shifter::ratioFor (0, 10)) - 1.0) < 0.001, "Detune moves the pitch by cents");
        }

        // Guitar chords stay clean and at level
        {
            const auto gtr = testsig::pluckedChords (fs, 3.0);
            shifter::Params p = wet;
            p.pitchA = 12.0; p.mode = shifter::Mode::smooth;
            const auto out = run (p, fs, gtr);
            check (testsig::allFinite (out) && std::abs (testsig::rmsDb (out) - testsig::rmsDb (gtr)) < 3.0, "Chords: level kept");
            check (testsig::brightness (out) > testsig::brightness (gtr) * 1.3, "Chords: octave up is brighter");
        }

        // Two voices, stereo width: A (+12) to the left, B (-12) to the right
        {
            shifter::Params p = wet;
            p.pitchA = 12.0; p.pitchB = -12.0; p.voiceB = true; p.width = 1.0;
            std::vector<float> r;
            const auto l = run (p, fs, in, &r);
            const double lHi = energyAt (l, fs, 440.0, a0, a1), lLo = energyAt (l, fs, 110.0, a0, a1);
            const double rHi = energyAt (r, fs, 440.0, a0, a1), rLo = energyAt (r, fs, 110.0, a0, a1);
            std::printf ("        width 100 %%: L has %.0f %% A / %.0f %% B, R has %.0f %% A / %.0f %% B\n", 100 * lHi, 100 * lLo, 100 * rHi, 100 * rLo);
            check (lHi > 0.9 && rLo > 0.9, "Width 100 %: voice A left, voice B right");

            p.width = 0.0;
            const auto m = run (p, fs, in, &r);
            const double mHi = energyAt (m, fs, 440.0, a0, a1), mLo = energyAt (m, fs, 110.0, a0, a1);
            check (mHi > 0.35 && mLo > 0.35 && testsig::maxDiff (m, r) < 1e-6, "Width 0 %: both voices centred");
        }

        // Mix 0 = dry, Bypass, Output
        {
            shifter::Params p;
            p.mix = 0.0;
            check (testsig::maxDiff (run (p, fs, in), in) < 1e-6, "Mix 0 = dry signal");
            shifter::Params byp = wet;
            byp.bypassed = true;
            check (testsig::maxDiff (run (byp, fs, in), in) < 1e-6, "Bypass is transparent");
            p.outputDb = -6.0;
            check (std::abs (testsig::rmsDb (run (p, fs, in)) - testsig::rmsDb (in) + 6.0) < 0.05, "Output gain");
        }

        // Delay: the wet signal starts later
        {
            shifter::Params p = wet;
            p.pitchA = 0.0; p.delayMs = 300.0;
            auto burst = testsig::sine (fs, f0, 0.3, 0.1);
            burst.resize ((size_t) (fs * 1.0), 0.0f);
            const auto out = run (p, fs, burst);
            const size_t w0 = (size_t) (0.05 * fs), w1 = (size_t) (0.25 * fs), w2 = (size_t) (0.33 * fs), w3 = (size_t) (0.4 * fs);
            check (testsig::rmsDb (out, w0, w1) < -80.0 && testsig::rmsDb (out, w2, w3) > -20.0, "Delay time");
        }

        // Feedback: echoes repeat and climb in pitch, but stay bounded
        {
            shifter::Params p = wet;
            p.pitchA = 7.0; p.delayMs = 200.0; p.feedback = 0.7; p.mix = 0.5;
            auto burst = testsig::sine (fs, f0, 0.3, 0.15);
            burst.resize ((size_t) (fs * 2.0), 0.0f);
            const auto out = run (p, fs, burst);
            const double tail = testsig::rmsDb (out, (size_t) (0.6 * fs), (size_t) (0.9 * fs));
            check (tail > -40.0, "Feedback repeats");
            check (energyAt (out, fs, f0 * shifter::ratioFor (7, 0), (size_t) (0.25 * fs), (size_t) (0.35 * fs)) > 0.7,
                   "First repeat is shifted");
            check (energyAt (out, fs, f0 * shifter::ratioFor (14, 0), (size_t) (0.5 * fs), (size_t) (0.6 * fs)) > 0.7,
                   "Second repeat is shifted twice (climbing echoes)");

            shifter::Params hot = p;
            hot.feedback = 1.0; hot.pitchA = 0.0; hot.delayMs = 20.0;
            const auto oHot = run (hot, fs, testsig::sine (fs, 100.0, 4.0, 3.0));
            check (testsig::allFinite (oHot) && testsig::peak (oHot) < 30.0, "Max feedback on hot input stays bounded");
        }

        // Tone darkens the wet signal
        {
            const auto gtr = testsig::pluckedChords (fs, 2.0);
            shifter::Params bright = wet, dark = wet;
            bright.pitchA = dark.pitchA = 12.0;
            dark.toneHz = 1500.0;
            check (testsig::brightness (run (dark, fs, gtr)) < testsig::brightness (run (bright, fs, gtr)) * 0.7, "Tone darkens");
        }

        // Sweeping the pitch (expression-pedal style) and switching voice B and Mode while playing
        {
            shifter::Engine e;
            e.prepare (fs, 2);
            shifter::Params p = wet;
            e.setParams (p);
            e.snapToTargets();
            auto l = testsig::pluckedChords (fs, 3.0), r = l;
            bool ok = true;
            for (size_t pos = 0, blk = 0; pos < l.size(); pos += 128, ++blk)
            {
                p.pitchA = 24.0 * std::sin (2.0 * testsig::kPi * (double) pos / (1.5 * fs));
                p.voiceB = (blk / 40) % 2 == 1;
                p.mode   = modes[(blk / 70) % 3];
                e.setParams (p);
                float* b[2] = { l.data() + pos, r.data() + pos };
                e.process (b, 2, (int) std::min<size_t> (128, l.size() - pos));
            }
            ok = testsig::allFinite (l) && testsig::allFinite (r) && testsig::peak (l) < 3.0 && testsig::peak (r) < 3.0;
            check (ok, "Pitch sweeps and live switching stay clean and bounded");
        }

        // Mix 100 % retuning ("virtual capo" / "drop tune"): no original pitch, not even right after
        // the start or after the pitch jumps from 0 / a downward shift to an upward one (a tap pinned
        // at the end of the buffer used to play the unshifted input for up to a whole grain)
        {
            const double f0 = 220.0;
            for (const auto m : modes)
            {
                double worst = 0.0;
                for (const auto [from, to] : { std::pair { 2, 2 }, { 0, 2 }, { -2, 2 }, { 0, -2 }, { 2, -2 }, { 5, 5 } })
                {
                    shifter::Engine e;
                    e.prepare (fs, 1);
                    shifter::Params p = wet;
                    p.pitchA = from; p.mode = m;
                    e.setParams (p);
                    e.snapToTargets();
                    auto out = testsig::sine (fs, f0, 0.5, 1.5);
                    const size_t sw = (size_t) (0.5 * fs) / 128 * 128, settle = (size_t) (0.06 * fs), seg = (size_t) (0.05 * fs);
                    for (size_t pos = 0; pos < out.size(); pos += 128)
                    {
                        if (pos == sw) { p.pitchA = to; e.setParams (p); }
                        float* b[1] = { out.data() + pos };
                        e.process (b, 1, (int) std::min<size_t> (128, out.size() - pos));
                    }
                    for (size_t a = settle; a + seg <= out.size(); a += seg / 2)
                    {
                        if ((from == 0 && a < sw + settle) || (a + seg > sw && a < sw + settle))
                            continue;
                        worst = std::max (worst, energyAt (out, fs, f0, a, a + seg));
                    }
                }
                check (worst < 0.4, "Mix 100 %: no original pitch after start or pitch jumps (retuning)");
            }
        }

        // Silence in -> silence out
        {
            shifter::Params p = wet;
            p.voiceB = true;
            const auto out = run (p, fs, testsig::silence (fs, 0.5));
            check (testsig::peak (out) == 0.0, "Silence stays silent");
        }
    }

    std::printf (failures == 0 ? "All tests passed\n" : "%d test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
