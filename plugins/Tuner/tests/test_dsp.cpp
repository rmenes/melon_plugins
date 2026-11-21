// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -I../Source -I../../../shared test_dsp.cpp -o test_dsp
#include "TunerDsp.h"
#include "../../../shared/TestSignals.h"
#include <chrono>
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

// Runs the engine and returns the readings of the last `tailSeconds` (frequency per analysis).
static std::vector<double> track (std::vector<float> sig, double fs, tuner::Params p = {}, double tailSeconds = 0.3,
                                  std::vector<float>* out = nullptr)
{
    tuner::Engine e;
    e.prepare (fs, 1);
    e.setParams (p);
    e.snapToTargets();
    std::vector<double> readings;
    uint32_t last = 0;
    const size_t tailStart = sig.size() - (size_t) (tailSeconds * fs);
    for (size_t pos = 0; pos < sig.size(); pos += 256)
    {
        float* blk[1] = { sig.data() + pos };
        e.process (blk, 1, (int) std::min<size_t> (256, sig.size() - pos));
        const auto f = e.current().frames.load();
        if (f != last && pos >= tailStart)
            readings.push_back (e.current().frequency.load());
        last = f;
    }
    if (out) *out = sig;
    return readings;
}

static double worstCents (const std::vector<double>& r, double f)
{
    if (r.empty()) return 1e9;
    double w = 0.0;
    for (double x : r)
        w = std::max (w, x > 0.0 ? std::abs (tuner::centsBetween (x, f)) : 1e9);
    return w;
}

// Guitar/bass-like tone: harmonics with 1/k amplitude, given level of the fundamental
static std::vector<float> harmonic (double fs, double f, double seconds, double fundamental = 1.0, double amp = 0.25)
{
    std::vector<float> v ((size_t) (fs * seconds));
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = (double) i / fs;
        double s = fundamental * std::sin (2 * testsig::kPi * f * t);
        for (int k = 2; k <= 10 && k * f < 0.45 * fs; ++k)
            s += std::sin (2 * testsig::kPi * f * k * t + 0.3 * k) / k;
        v[i] = (float) (amp * s * std::exp (-0.4 * t));
    }
    return v;
}

int main()
{
    // Note maths and tunings
    check (std::abs (tuner::freqFromMidi (69, 440.0) - 440.0) < 1e-9, "A4 = 440 Hz");
    check (tuner::noteName (40) == "E" && tuner::octaveOf (40) == 2, "MIDI 40 = E2");
    check (tuner::noteName (39, true) == "Eb" && tuner::noteName (39) == "D#", "Sharps and flats");
    {
        auto t = tuner::findTarget (82.0, 440.0, 1); // Guitar Standard, slightly flat low E
        check (t.string == 0 && t.midi == 40 && t.cents < 0.0 && t.cents > -15.0, "Guitar Standard: 82 Hz -> low E, flat");
        t = tuner::findTarget (73.42, 440.0, 1);     // D2 in standard tuning -> still low E string
        check (t.string == 0 && t.cents < -150.0, "Guitar Standard: D2 is shown as low E tuned down 2 semitones");
        t = tuner::findTarget (73.42, 440.0, 4);     // Drop D
        check (t.string == 0 && std::abs (t.cents) < 1.0, "Drop D: D2 is in tune on the low string");
        t = tuner::findTarget (30.87, 440.0, 11);    // Bass 5-string low B
        check (t.string == 0 && t.midi == 23 && std::abs (t.cents) < 1.0, "Bass 5-String: low B0");
        t = tuner::findTarget (442.0, 442.0, 0);
        check (t.midi == 69 && std::abs (t.cents) < 1e-6, "Chromatic with A4 = 442 Hz");
    }
    {
        tuner::Tracker tr;
        tr.push (110.0);
        check (tr.push (220.0) == 110.0, "Tracker ignores a single octave jump");
        check (tr.push (220.5) > 219.0, "Tracker follows a new note after two frames");
        for (int i = 0; i < tuner::kHoldFrames; ++i) tr.push (0.0);
        check (tr.push (0.0) == 0.0, "Tracker releases the note after the hold time");
    }

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("Sample rate %.0f Hz\n", fs);

        // Pure tones across the whole range: bass low B to 24th fret high E
        double worstSine = 0.0;
        for (double f : { 30.87, 41.2, 55.0, 82.41, 110.0, 196.0, 329.63, 659.26, 987.77, 1318.51 })
            worstSine = std::max (worstSine, worstCents (track (testsig::sine (fs, f, 0.2, 0.8), fs), f));
        std::printf ("        sines 31 Hz - 1.3 kHz: worst error %.3f cents\n", worstSine);
        check (worstSine < 0.5, "Sines: error under 0.5 cent from 31 Hz to 1.3 kHz");

        // Harmonic-rich decaying notes, slightly detuned
        double worstHarm = 0.0;
        for (double f : { 30.87 * 1.004, 41.2, 82.41 * 0.993, 146.83, 246.94 * 1.01, 329.63 })
            worstHarm = std::max (worstHarm, worstCents (track (harmonic (fs, f, 1.0), fs), f));
        std::printf ("        harmonic notes: worst error %.3f cents\n", worstHarm);
        check (worstHarm < 1.0, "Harmonic-rich notes: error under 1 cent");

        // Bass note with a weak fundamental: no octave error
        {
            auto r = track (harmonic (fs, 41.2, 1.0, 0.25), fs);
            std::printf ("        weak-fundamental E1: worst error %.3f cents\n", worstCents (r, 41.2));
            check (worstCents (r, 41.2) < 2.0, "Weak fundamental (bass E1): no octave error");
        }

        // Silence, signal below the threshold, and noise give no note
        {
            auto r = track (testsig::silence (fs, 0.6), fs);
            check (! r.empty() && r.back() == 0.0, "Silence: no note");
            tuner::Params p;
            p.thresholdDb = -40.0;
            r = track (testsig::sine (fs, 110.0, 0.002, 0.6), fs, p); // about -57 dB
            check (! r.empty() && r.back() == 0.0, "Below the threshold: no note");
            std::vector<float> noise ((size_t) (fs * 0.8));
            melon::Noise n;
            for (auto& x : noise) x = (float) (0.2 * n.next());
            r = track (noise, fs);
            int notes = 0;
            for (double x : r) notes += x > 0.0;
            std::printf ("        white noise: %d of %zu frames gave a note\n", notes, r.size());
            check (notes <= (int) r.size() / 4, "White noise: mostly no note");
        }

        // Audio path: unity, mute, output gain
        {
            auto in = testsig::pluckedChords (fs, 0.5);
            std::vector<float> out;
            track (in, fs, {}, 0.3, &out);
            check (testsig::maxDiff (in, out) < 1e-6, "Default: audio passes through unchanged");
            tuner::Params p;
            p.mute = true;
            track (in, fs, p, 0.3, &out);
            check (testsig::peak (out) < 1e-6, "Mute: silent output");
            p.bypassed = true;
            track (in, fs, p, 0.3, &out);
            check (testsig::maxDiff (in, out) < 1e-6, "Bypass overrides Mute");
            p = {};
            p.outputDb = -6.0;
            track (in, fs, p, 0.3, &out);
            check (std::abs (testsig::rmsDb (out) - testsig::rmsDb (in) + 6.0) < 0.1, "Output gain -6 dB");
        }

        // CPU: worst case is the lowest notes (longest lag search)
        {
            auto sig = testsig::sine (fs, 41.2, 0.2, 10.0);
            const auto t0 = std::chrono::steady_clock::now();
            track (sig, fs);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("        CPU: %.2f %% of one core\n", 100.0 * secs / 10.0);
            check (secs < 0.5, "CPU under 5 % of one core");
        }
    }

    std::printf (failures == 0 ? "\nAll tests passed\n" : "\n%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
