// DSP core test (does not need JUCE):  g++ -std=c++17 -O2 -I../Source test_dsp.cpp -o test_dsp
#include "GuitarSynthDsp.h"
#include <cstdio>
#include <functional>
#include <vector>

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

using Signal = std::function<float (int)>;

struct Run
{
    std::vector<float> l, r;
    std::vector<double> cutoff; // filter cutoff at the end of every 64-sample block
    int triggers = 0;
};

// Runs a mono test signal through the engine (copied to both channels), in 64-sample blocks
static Run run (const gsynth::Params& p, double fs, int numSamples, const Signal& signal)
{
    gsynth::Engine e;
    e.prepare (fs, 2);
    e.setParams (p);
    e.snapToTargets();

    Run out { std::vector<float> ((size_t) numSamples), std::vector<float> ((size_t) numSamples), {}, 0 };
    for (int i = 0; i < numSamples; ++i)
        out.l[(size_t) i] = out.r[(size_t) i] = signal (i);

    for (int pos = 0; pos < numSamples; pos += 64)
    {
        float* blk[2] = { out.l.data() + pos, out.r.data() + pos };
        e.process (blk, 2, std::min (64, numSamples - pos));
        out.cutoff.push_back (e.currentCutoffHz());
    }
    out.triggers = e.triggers();
    return out;
}

// Amplitude of one frequency in part of a signal (Goertzel, Hann window)
static double toneLevel (const std::vector<float>& x, size_t from, size_t to, double freq, double fs)
{
    const double w = 2.0 * gsynth::kPi * freq / fs;
    std::complex<double> acc = 0.0;
    double wsum = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * gsynth::kPi * (double) (i - from) / (double) (to - from));
        acc += win * (double) x[i] * std::polar (1.0, -w * (double) i);
        wsum += win;
    }
    return 2.0 * std::abs (acc) / wsum;
}

static double rms (const std::vector<float>& x, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (to - from));
}

// Only one voice on, filter wide open and flat
static gsynth::Params voiceOnly (double gtr, double sub, double oct, double sq)
{
    gsynth::Params p;
    p.guitar = gtr; p.subOctave = sub; p.octave = oct; p.square = sq;
    p.startHz = p.stopHz = gsynth::kMaxFreqHz;
    p.resonance = 0.0;
    return p;
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);
        const int n  = (int) fs * 2;
        const auto N = (size_t) n;
        auto sine = [fs] (double f, double a) { return [=] (int i) { return (float) (a * std::sin (2.0 * gsynth::kPi * f * i / fs)); }; };
        // a plucked-string-like tone: strong 2nd and 3rd harmonics, decaying
        auto pluck = [fs] (double f) {
            return [=] (int i) {
                const double t = i / fs, w = 2.0 * gsynth::kPi * f * t;
                return (float) (0.4 * std::exp (-1.5 * t) * (std::sin (w) + 0.7 * std::sin (2 * w + 0.5) + 0.5 * std::sin (3 * w + 1.1) + 0.2 * std::sin (5 * w)));
            };
        };

        // 1. Bypass → output is the input, one sample late (the plugin reports one sample of latency)
        {
            auto p = voiceOnly (0, 1, 1, 1);
            p.bypassed = true;
            auto in  = sine (440, 0.5);
            auto out = run (p, fs, n, in);
            double worst = 0;
            for (int i = 1; i < n; ++i) worst = std::max (worst, (double) std::abs (out.l[(size_t) i] - in (i - 1)));
            check (worst < 1e-6, "Bypass passes the dry signal untouched (1-sample latency)");
        }

        // 1b. Mix: 0 % = dry only (even with every voice up), 50 % = half dry + half wet
        {
            gsynth::Params p;
            p.guitar = p.subOctave = p.octave = p.square = 1.0;
            p.resonance = 1.0;
            p.mix = 0.0;
            auto in  = sine (196, 0.4);
            auto out = run (p, fs, n, in);
            double worst = 0;
            for (int i = 1; i < n; ++i) worst = std::max (worst, (double) std::abs (out.l[(size_t) i] - in (i - 1)));
            check (worst < 1e-6, "Mix 0 % passes the dry signal untouched");

            auto q = voiceOnly (0, 0, 0, 0); // wet path silent
            q.mix  = 0.5;
            auto half = run (q, fs, n, sine (440, 0.05));
            const double db = 20 * std::log10 (toneLevel (half.l, N / 2, N, 440, fs) / 0.05);
            check (std::abs (db + 6.02) < 0.1, "Mix 50 %: dry at half level (-6 dB)");
        }

        // 2. Guitar voice alone, filter open, no resonance → unity gain
        {
            auto out = run (voiceOnly (1, 0, 0, 0), fs, n, sine (440, 0.05));
            const double db = 20 * std::log10 (toneLevel (out.l, N / 2, N, 440, fs) / 0.05);
            std::printf ("    guitar voice gain %+.2f dB\n", db);
            check (std::abs (db) < 0.5, "Guitar voice: unity gain through an open filter");
        }

        // 3-5. Synth voices on a 220 Hz note
        {
            auto out = run (voiceOnly (0, 0, 0, 1), fs, n, sine (220, 0.3));
            const double f1 = toneLevel (out.l, N / 2, N, 220, fs), f2 = toneLevel (out.l, N / 2, N, 440, fs),
                         f3 = toneLevel (out.l, N / 2, N, 660, fs);
            std::printf ("    square: 220 Hz %.3f, 440 Hz %.4f, 660 Hz %.3f\n", f1, f2, f3);
            check (f1 > 0.1 && f2 < 0.05 * f1 && std::abs (f3 / f1 - 1.0 / 3.0) < 0.06,
                   "Square voice: square wave at the played note (odd harmonics, 1/3 at the 3rd)");
        }
        {
            auto out = run (voiceOnly (0, 1, 0, 0), fs, n, sine (220, 0.3));
            const double sub = toneLevel (out.l, N / 2, N, 110, fs), f1 = toneLevel (out.l, N / 2, N, 220, fs);
            std::printf ("    sub octave: 110 Hz %.3f, 220 Hz %.4f\n", sub, f1);
            check (sub > 0.1 && f1 < 0.1 * sub, "Sub Octave voice: one octave below the played note");
        }
        {
            auto out = run (voiceOnly (0, 0, 1, 0), fs, n, sine (220, 0.3));
            const double f2 = toneLevel (out.l, N / 2, N, 440, fs), f1 = toneLevel (out.l, N / 2, N, 220, fs),
                         sub = toneLevel (out.l, N / 2, N, 110, fs);
            std::printf ("    octave up: 440 Hz %.3f, 220 Hz %.4f, 110 Hz %.4f\n", f2, f1, sub);
            check (f2 > 0.1 && f1 < 0.05 * f2 && sub < 0.05 * f2, "Octave voice: one octave above the played note");
        }

        // 6. Tracking a harmonic-rich, decaying low-E pluck
        {
            gsynth::Engine e;
            const double f0 = 82.41;
            auto in = pluck (f0);
            auto out = run (voiceOnly (0, 1, 0, 0), fs, n, in);
            const double sub = toneLevel (out.l, N / 4, N, f0 / 2, fs), fund = toneLevel (out.l, N / 4, N, f0, fs);
            std::printf ("    low E pluck: sub (41 Hz) %.3f, 82 Hz %.4f\n", sub, fund);
            check (sub > 4.0 * fund, "Tracks a plucked low E with strong harmonics (sub octave at 41 Hz)");

            auto out2 = run (voiceOnly (0, 0, 0, 1), fs, n, pluck (329.6));
            const double a1 = toneLevel (out2.l, N / 4, N / 2, 329.6, fs), a2 = toneLevel (out2.l, N / 4, N / 2, 659.2, fs);
            check (a1 > 0.05 && a2 < 0.15 * a1, "Tracks a plucked high E (square at 330 Hz, no octave jumps)");
        }

        // 6b. No dropped cycles while the tracker locks on at the start of a note (the square
        //     voice must keep switching every half period, from 15 ms after the pick)
        {
            bool ok = true;
            for (double f0 : { 82.41, 110.0, 196.0, 329.6, 659.3 })
            {
                auto out = run (voiceOnly (0, 0, 0, 1), fs, (int) (0.4 * fs), pluck (f0));
                size_t last = 0;
                double worst = 0;
                for (size_t i = (size_t) (0.015 * fs); i < out.l.size(); ++i)
                    if ((out.l[i - 1] < 0) != (out.l[i] < 0))
                    {
                        if (last) worst = std::max (worst, (double) (i - last) * f0 / fs);
                        last = i;
                    }
                ok &= worst < 0.75 && last > 0;
            }
            check (ok, "No dropped cycles while locking onto a new note (low E to high E)");
        }

        // 7. Filter sweep: rests at Stop, jumps to Start on a trigger, reaches Stop after the sweep time
        {
            gsynth::Params p = voiceOnly (1, 0, 0, 0);
            p.startHz = 4000; p.stopHz = 200; p.rate = gsynth::rateForSweepSeconds (0.3);
            const int start = (int) (0.5 * fs);
            auto out = run (p, fs, n, [=] (int i) { return i < start ? 0.0f : (float) (0.3 * std::sin (2 * gsynth::kPi * 196.0 * i / fs)); });
            auto cutoffAtTime = [&] (double t) { return out.cutoff[std::min (out.cutoff.size() - 1, (size_t) (t * fs / 64))]; };
            const double before = cutoffAtTime (0.4), atHit = cutoffAtTime (0.502), after = cutoffAtTime (0.5 + 0.3);
            std::printf ("    cutoff: before %.0f Hz, at the note %.0f Hz, after the sweep %.0f Hz (%d trigger)\n", before, atHit, after, out.triggers);
            check (std::abs (before - 200) < 1, "Filter rests at Stop Freq before any note");
            check (atHit > 3000, "A note restarts the sweep from Start Freq");
            check (std::abs (std::log (after / 200)) < std::log (4000.0 / 200) * 0.06, "Sweep reaches Stop Freq after the sweep time");
            check (out.triggers == 1, "A single held note triggers once");
        }

        // 8. Trigger sensitivity
        {
            gsynth::Params p = voiceOnly (1, 0, 0, 0);
            auto quiet = sine (196, 0.01); // -40 dBFS
            p.trigger = 0.2;               // threshold -26 dB
            const int a = run (p, fs, n / 2, quiet).triggers;
            p.trigger = 0.8;               // threshold -52 dB
            const int b = run (p, fs, n / 2, quiet).triggers;
            check (a == 0 && b == 1, "Trigger sets how loud a note must be to start the sweep");

            // repeated picking: four notes, each with silence in between → four triggers
            auto notes = [fs] (int i) {
                const double t = std::fmod (i / fs, 0.4);
                return (float) (t < 0.3 ? 0.3 * std::exp (-6 * t) * std::sin (2 * gsynth::kPi * 147 * i / fs) : 0.0);
            };
            p.trigger = 0.5;
            const int c = run (p, fs, (int) (1.6 * fs), notes).triggers;
            // legato: the second note is picked while the first still rings
            auto legato = [fs] (int i) {
                const double t = i / fs, t2 = t - 0.5;
                double y = 0.2 * std::exp (-2 * t) * std::sin (2 * gsynth::kPi * 147 * t);
                if (t2 >= 0) y += 0.5 * std::exp (-2 * t2) * std::sin (2 * gsynth::kPi * 196 * t2);
                return (float) y;
            };
            const int d = run (p, fs, n / 2, legato).triggers;
            std::printf ("    separate notes: %d triggers, legato pair: %d triggers\n", c, d);
            check (c == 4, "Every separately picked note triggers");
            check (d == 2, "A louder note picked over a ringing one retriggers");
        }

        // 9. Attack Delay: voices fade in after the trigger
        {
            gsynth::Params p = voiceOnly (1, 0, 0, 0);
            p.attackS = 0.5;
            const int start = (int) (0.2 * fs);
            auto out = run (p, fs, n, [=] (int i) { return i < start ? 0.0f : (float) (0.3 * std::sin (2 * gsynth::kPi * 220.0 * i / fs)); });
            const size_t s = (size_t) start, w = (size_t) (0.05 * fs);
            const double early = rms (out.l, s, s + w), late = rms (out.l, s + (size_t) (0.6 * fs), s + (size_t) (0.6 * fs) + w);
            std::printf ("    attack delay: first 50 ms %.4f, after 0.6 s %.4f\n", early, late);
            check (early < 0.05 * late && late > 0.15, "Attack Delay removes the attack and fades the note in");
        }

        // 10. Display maths matches the audio (static filter, small signal, guitar voice only)
        {
            gsynth::Params p = voiceOnly (1, 0, 0, 0);
            p.startHz = p.stopHz = 1000;
            p.resonance = 0.7;
            double worst = 0;
            for (double f : { 150.0, 700.0, 1500.0, 1732.0, 2500.0, 5000.0 })
            {
                auto out = run (p, fs, n, sine (f, 0.005));
                const double meas = 20 * std::log10 (toneLevel (out.l, N / 2, N, f, fs) / 0.005);
                const double calc = gsynth::responseDb (f, fs, 1000.0, 0.7);
                if (calc > -40) worst = std::max (worst, std::abs (meas - calc));
            }
            std::printf ("    display vs measured: worst %.2f dB\n", worst);
            check (worst < 0.5, "Response display matches the processed audio");
        }

        // 11. Extremes: everything at 100 %, max Resonance, fastest sweep, loud noise bursts
        {
            gsynth::Params p;
            p.guitar = p.subOctave = p.octave = p.square = 1.0;
            p.resonance = 1.0; p.rate = 1.0; p.trigger = 1.0;
            p.startHz = gsynth::kMaxFreqHz; p.stopHz = gsynth::kMinFreqHz;
            unsigned seed = 1;
            auto noise = [&seed, fs] (int i) {
                seed = seed * 1664525u + 1013904223u;
                const double t = std::fmod (i / fs, 0.25);
                return i < (int) (3 * fs) && t < 0.1 ? 0.9f * ((float) (seed >> 9) / 4194304.0f - 1.0f) : 0.0f;
            };
            auto out = run (p, fs, (int) fs * 5, noise);
            bool   finite = true;
            double peak = 0, tail = 0;
            for (size_t i = 0; i < out.l.size(); ++i)
            {
                finite &= std::isfinite (out.l[i]);
                peak = std::max (peak, (double) std::abs (out.l[i]));
                if (i > (size_t) fs * 4) tail = std::max (tail, (double) std::abs (out.l[i]));
            }
            std::printf ("    peak %.2f, level 1 s after the input stops %.1e\n", peak, tail);
            check (finite && peak < 4.0, "Extreme settings: output finite and bounded");
            check (tail < 1e-4, "Max Resonance: the filter rings down after the input stops");
        }

        // 12. Silence in → silence out (the gate keeps the synth voices quiet), L = R
        {
            gsynth::Params p;
            p.guitar = p.subOctave = p.octave = p.square = 1.0;
            unsigned seed = 7;
            auto hiss = [&seed] (int) { seed = seed * 1664525u + 1013904223u; return 2e-5f * ((float) (seed >> 9) / 4194304.0f - 1.0f); };
            auto out = run (p, fs, n, hiss);
            check (rms (out.l, N / 2, N) < 1e-4, "Background hiss (-94 dB) doesn't make the synth voices chatter");
            auto out2 = run (p, fs, n, pluck (110));
            double d = 0;
            for (size_t i = 0; i < N; ++i) d = std::max (d, (double) std::abs (out2.l[i] - out2.r[i]));
            check (d < 1e-7, "Left and right identical (mono pedal)");
        }

        // 13. Band-limited edges: a high square note has little aliasing. Probes the frequencies where
        //     the strongest harmonics above Nyquist would fold back to.
        {
            const double f = 1046.5; // C6
            auto out = run (voiceOnly (0, 0, 0, 1), fs, n, sine (f, 0.3));
            const double fund = toneLevel (out.l, N / 2, N, f, fs);
            double worst = 0;
            int probes = 0;
            for (int m = (int) (0.5 * fs / f) + 1; m * f < fs && probes < 6; ++m)
            {
                if (m % 2 == 0) continue;               // a square has odd harmonics only
                const double alias = fs - m * f;        // where harmonic m folds back to
                const double gap   = std::fmod (alias, f);
                if (std::min (gap, f - gap) < 40.0) continue; // too close to a real harmonic to measure
                worst = std::max (worst, toneLevel (out.l, N / 2, N, alias, fs) * m); // relative to that harmonic's own 1/m level
                ++probes;
            }
            const double db = 20 * std::log10 (worst / fund);
            std::printf ("    folded harmonics: %.1f dB below their naive level (%d probes)\n", -db, probes);
            check (probes > 0 && db < -20.0, "Square voice is band-limited (folded harmonics at least 20 dB down)");
        }
    }

    // 14. Control mappings
    {
        std::printf ("Mappings\n");
        check (std::abs (gsynth::sweepSeconds (0.0) - 4.0) < 1e-9 && std::abs (gsynth::sweepSeconds (1.0) - 0.01) < 1e-9,
               "Rate 0 % = 4 s sweep, 100 % = 10 ms");
        check (std::abs (gsynth::rateForSweepSeconds (gsynth::sweepSeconds (0.37)) - 0.37) < 1e-9, "Rate <-> sweep time round trip");
        check (std::abs (gsynth::cutoffAt (0, 3000, 300, 0.2) - 3000) < 1e-6 && std::abs (gsynth::cutoffAt (1e3, 3000, 300, 0.2) - 300) < 1e-6,
               "Sweep curve starts at Start and ends at Stop");
        check (std::abs (gsynth::cutoffAt (0.5, 800, 800, 0.2) - 800) < 1e-6, "Start = Stop: fixed filter, no sweep");
    }

    std::printf ("\n%s\n", failures == 0 ? "All tests passed." : "SOME TESTS FAILED.");
    return failures == 0 ? 0 : 1;
}
