// GuitarSynthDsp.h - Melon Guitar Synth DSP core, in plain C++ (no JUCE) so it can be tested on its own.
//
// Modelled on the ten sliders of the classic Electro-Harmonix Micro Synthesizer guitar pedal:
// Trigger, four voice levels (Sub Octave, Guitar, Octave, Square Wave), Attack Delay, and a resonant
// 3-pole low-pass filter that sweeps from Start Freq to Stop Freq at Rate each time a note is triggered.
//
//   in ──┬──────────────────────────────────────────────────────────────── dry ──┐
//        ▼                                                          Mix (dry/wet) ─► out
//   in (mono) ─┬───────────────────────────────────────────── Guitar ──┐
//              │                                                       │
//              └─► tracking filter ─► comparator ─┬─► square ── Square ┤
//                  (follows the note)     │       └─► ÷2 flip-flop ─ Sub Octave
//                                         └─► full-wave rectifier ── Octave ┤
//                                                                      ▼
//       envelope ─► Trigger ─► new note? ──► Attack Delay fade-in ──► (×) ──► 3-pole resonant
//                                      └───► filter sweep Start → Stop ──────► low-pass ─► wet
//
// The synth voices are built from the note the guitar is playing, like the pedal: the input is
// low-passed to leave mostly the fundamental, a comparator with hysteresis turns it into a square
// wave, a flip-flop that toggles on every rising edge divides it by two (sub octave), and a full-wave
// rectifier doubles it (octave up). The square and sub-octave edges are band-limited (polyBLEP),
// so high notes don't alias. Every synth voice follows the playing dynamics.
//
// Trigger decides how loud a note has to be to restart the filter sweep (and the Attack Delay
// fade-in). Without a new trigger the filter rests at Stop Freq, as on the pedal.
//
// The pedal is mono, so the effect works on the mono sum of the input and sends the same signal
// to both outputs. The engine adds one sample of latency (needed by the band-limited edges).
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace gsynth
{
constexpr double kPi = 3.14159265358979323846;

// --- Tuning ------------------------------------------------------------------
constexpr double kMinFreqHz     = 50.0;     // Start / Stop Freq range
constexpr double kMaxFreqHz     = 10000.0;
constexpr double kMinSweepS     = 0.01;     // Rate 100 %: the sweep takes 10 ms
constexpr double kMaxSweepS     = 4.0;      // Rate 0 %: the sweep takes 4 s
constexpr double kMaxAttackS    = 2.0;      // Attack Delay range (0 = off)
constexpr double kMaxResonance  = 7.2;      // 3-pole ladder feedback at Resonance 100 % (oscillates at 8)
constexpr double kTriggerLoDb   = -18.0;    // Trigger 0 %: only hard picking triggers
constexpr double kTriggerHiDb   = -60.0;    // Trigger 100 %: the lightest touch triggers
constexpr double kGateDb        = -62.0;    // synth voices fade out below this input level
constexpr double kRetriggerS    = 0.05;     // minimum time between two triggers
constexpr double kMinTrackHz    = 40.0;     // pitch-tracking limits
constexpr double kMaxTrackHz    = 1600.0;
constexpr double kSmoothingMs   = 20.0;
constexpr double kTrackRatio    = 1.0;      // the 4-pole tracking low-pass sits on the fundamental
constexpr double kNoteDb        = -50.0;    // Attack Delay: a note louder than this restarts the fade-in
constexpr int    kMaxChannels   = 2;

// Relative voice levels at 100 %, so that all four voices sound about as loud as each other
constexpr double kGuitarGain = 1.0, kSubGain = 0.75, kOctaveGain = 1.4, kSquareGain = 0.55;

struct Params
{
    double trigger    = 0.5;    // 0..1 sensitivity
    double subOctave  = 0.4;    // 0..1 voice levels
    double guitar     = 0.3;
    double octave     = 0.0;
    double square     = 0.6;
    double attackS    = 0.0;    // Attack Delay: fade-in time of the voices after a trigger
    double resonance  = 0.6;    // 0..1
    double startHz    = 3000.0; // the sweep starts here when a note is triggered...
    double stopHz     = 300.0;  // ...and ends (and rests) here
    double rate       = 0.5;    // 0..1, higher = faster sweep
    double mix        = 1.0;    // 0..1, dry/wet: 0 = dry guitar only, 1 = synth only (like the pedal)
    double outputDb   = 0.0;
    bool   bypassed   = false;
};

// Sweep time (seconds) for a Rate value, and back. Rate 0 % = 4 s, 100 % = 10 ms, on a log scale.
inline double sweepSeconds (double rate)
{
    return kMaxSweepS * std::pow (kMinSweepS / kMaxSweepS, std::clamp (rate, 0.0, 1.0));
}
inline double rateForSweepSeconds (double s)
{
    return std::clamp (std::log (std::clamp (s, kMinSweepS, kMaxSweepS) / kMaxSweepS) / std::log (kMinSweepS / kMaxSweepS), 0.0, 1.0);
}

// Envelope level (linear) that fires the trigger
inline double triggerThreshold (double trigger)
{
    const double db = kTriggerLoDb + (kTriggerHiDb - kTriggerLoDb) * std::clamp (trigger, 0.0, 1.0);
    return std::pow (10.0, db / 20.0);
}

inline double feedbackFor (double resonance) { return kMaxResonance * std::clamp (resonance, 0.0, 1.0); }

// Make-up gain at the filter input. A ladder loses passband level as its feedback rises (1 / (1 + k));
// this gives most of it back, so Resonance mostly adds a peak (and a slight boost, as on the pedal).
inline double inputGainFor (double k) { return 1.0 + 0.65 * k; }

// Filter cutoff a time 't' (seconds) after a trigger. The sweep moves on a log-frequency scale with
// an RC-like curve (fast at first, settling at Stop), and is 95 % of the way there after the sweep time.
inline double cutoffAt (double t, double startHz, double stopHz, double sweepS)
{
    const double ls = std::log (startHz), le = std::log (stopHz);
    return std::exp (le + (ls - le) * std::exp (-3.0 * std::max (0.0, t) / std::max (1e-4, sweepS)));
}

// One-pole low-pass coefficient (bilinear / TPT form)
inline double onePoleCoeff (double hz, double fs)
{
    const double g = std::tan (kPi * std::clamp (hz, 1.0, 0.42 * fs) / fs);
    return g / (1.0 + g);
}

// Small-signal magnitude response (dB) of the swept filter (including its input gain) at one
// frequency, for the display. Ignores the soft clip, which only matters at high levels.
inline double responseDb (double freqHz, double fs, double cutoffHz, double resonance)
{
    const double w  = 2.0 * kPi * freqHz / fs;
    const auto   z1 = std::polar (1.0, -w);
    const double a  = onePoleCoeff (cutoffHz, fs);
    const auto   h1 = a * (1.0 + z1) / (1.0 - (1.0 - 2.0 * a) * z1);
    const auto   h3 = h1 * h1 * h1;
    const double k  = feedbackFor (resonance);
    const auto   h  = inputGainFor (k) * h3 / (1.0 + k * h3);
    return 20.0 * std::log10 (std::max (1e-6, std::abs (h)));
}

//==============================================================================
class Smoother
{
public:
    void   prepare (double fs, double ms) { coeff = std::exp (-1.0 / (0.001 * ms * fs)); }
    void   setTarget (double t) { target = t; }
    void   snap() { value = target; }
    double next() { return value = target + coeff * (value - target); }
    double current() const { return value; }

private:
    double coeff = 0.0, value = 0.0, target = 0.0;
};

// Peak envelope follower with separate attack and release times
class Envelope
{
public:
    void prepare (double fs, double attackMs, double releaseMs)
    {
        sr  = fs;
        att = 1.0 - std::exp (-1.0 / (0.001 * attackMs * fs));
        setRelease (releaseMs);
    }
    void setRelease (double releaseMs) { rel = 1.0 - std::exp (-1.0 / (0.001 * releaseMs * sr)); }
    void   reset() { value = 0.0; }
    double process (double x)
    {
        const double a = std::abs (x);
        value += (a > value ? att : rel) * (a - value);
        return value;
    }
    double current() const { return value; }

private:
    double sr = 48000.0, att = 0.0, rel = 0.0, value = 0.0;
};

// TPT one-pole low-pass (cutoff can change every sample)
struct OnePole
{
    double s = 0.0;
    double lowpass (double x, double a)
    {
        const double v = a * (x - s);
        const double y = v + s;
        s              = y + v;
        return y;
    }
};

//==============================================================================
// polyBLEP residuals for a step of height h that happened d samples (0..1) before the current
// sample: 'before' is added to the previous sample, 'after' to the current one.
inline double blepBefore (double h, double d) { return 0.5 * h * d * d; }
inline double blepAfter (double h, double d) { return -0.5 * h * (1.0 - d) * (1.0 - d); }

//==============================================================================
// Follows the note being played and builds the square, sub-octave and octave-up voices from it.
class Tracker
{
public:
    void prepare (double sampleRate)
    {
        fs     = sampleRate;
        hpA    = onePoleCoeff (60.0, fs);
        dcA    = onePoleCoeff (25.0, fs);
        trackEnv.prepare (fs, 0.5, 60.0); // level of the tracked fundamental (normalises the voices)
        hystEnv.prepare (fs, 0.2, 25.0);  // faster: follows the level as the tracking filter narrows
        cutoff.prepare (fs, 8.0);
        reset();
    }

    void reset()
    {
        hp = lp1 = lp2 = lp3 = lp4 = dc = subLp = OnePole {};
        trackEnv.reset();
        hystEnv.reset();
        hystEnv.setRelease (25.0);
        cutoff.setTarget (1000.0);
        cutoff.snap();
        state = -1.0; flip = 1.0; prev = 0.0;
        sqPrev = -1.0; subPrev = 1.0;
        sinceRise = 0.0; f0 = 0.0;
    }

    // A new note: open the tracking filter up again so it can catch a higher note quickly
    void newNote() { cutoff.setTarget (1000.0); }

    struct Out
    {
        double square, sub, octave; // all about ±1 for a steady note (scaled by the caller)
    };

    // Returns the voices for the PREVIOUS sample (one sample of latency for the band-limited edges)
    Out process (double x)
    {
        // remove rumble, then a steep low-pass on the fundamental, so the harmonics can't fool the comparator
        const double h  = x - hp.lowpass (x, hpA);
        const double a  = onePoleCoeff (cutoff.next(), fs);
        const double t  = lp4.lowpass (lp3.lowpass (lp2.lowpass (lp1.lowpass (h, a), a), a), a);
        const double ev = trackEnv.process (t);

        // comparator with hysteresis relative to the tracked level (rejects ripples from harmonics).
        // Its envelope releases over about two periods of the note, so it never lags behind the signal.
        const double hyst = std::max (0.25 * hystEnv.process (t), 1e-6);
        double sqNow  = state, subNow = flip;
        double sqCorr = 0.0, subCorr = 0.0; // polyBLEP corrections for the previous sample

        sinceRise += 1.0;
        if (state < 0.0 && t > hyst)
        {
            const double d = std::clamp ((t - hyst) / std::max (1e-12, t - prev), 0.0, 1.0); // how long ago it crossed
            state = 1.0;
            sqNow = 1.0 + blepAfter (2.0, d);
            sqCorr = blepBefore (2.0, d);

            // flip-flop: divides the frequency by two
            const double hFlip = -2.0 * flip;
            flip   = -flip;
            subNow = flip + blepAfter (hFlip, d);
            subCorr = blepBefore (hFlip, d);

            // period between rising edges → pitch estimate → tracking filter just above the fundamental
            const double period = sinceRise - d;
            sinceRise = d;
            const double hz = fs / period;
            if (hz >= kMinTrackHz && hz <= kMaxTrackHz)
            {
                f0 = f0 > 0.0 ? 0.6 * f0 + 0.4 * hz : hz;
                cutoff.setTarget (std::clamp (kTrackRatio * f0, 60.0, 2000.0));
                hystEnv.setRelease (std::clamp (2000.0 / f0, 2.0, 25.0));
            }
        }
        else if (state > 0.0 && t < -hyst)
        {
            const double d = std::clamp ((-hyst - t) / std::max (1e-12, prev - t), 0.0, 1.0);
            state = -1.0;
            sqNow = -1.0 + blepAfter (-2.0, d);
            sqCorr = blepBefore (-2.0, d);
        }
        prev = t;

        // Octave up: full-wave rectified fundamental (normalised), with the DC removed
        const double tn  = std::clamp (t / std::max (ev, 1e-6), -1.5, 1.5);
        const double rec = std::abs (tn);
        const double oct = rec - dc.lowpass (rec, dcA);

        // Sub octave: flip-flop square, rounded off a little so it sounds like a bass tone
        const double subA = onePoleCoeff (std::clamp (f0 > 0.0 ? 2.5 * f0 : 400.0, 100.0, 2500.0), fs);

        Out out;
        out.square = sqPrev + sqCorr;
        out.sub    = subLp.lowpass (subPrev + subCorr, subA);
        out.octave = octPrev;
        sqPrev  = sqNow;
        subPrev = subNow;
        octPrev = 2.0 * oct;
        return out;
    }

    double pitchHz() const { return f0; }

private:
    double   fs = 48000.0, hpA = 0.0, dcA = 0.0;
    OnePole  hp, lp1, lp2, lp3, lp4, dc, subLp;
    Envelope trackEnv, hystEnv;
    Smoother cutoff;
    double   state = -1.0, flip = 1.0, prev = 0.0;
    double   sqPrev = -1.0, subPrev = 1.0, octPrev = 0.0;
    double   sinceRise = 0.0, f0 = 0.0;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);

        tracker.prepare (fs);
        fastEnv.prepare (fs, 0.5, 40.0);   // trigger
        voiceEnv.prepare (fs, 2.0, 150.0); // dynamics of the synth voices (slow release: no ripple)
        voiceLpA = onePoleCoeff (40.0, fs);
        slowEnv.prepare (fs, 40.0, 300.0); // picks out new attacks during legato playing
        for (auto* s : allSmoothers())
            s->prepare (fs, kSmoothingMs);
        reset();
    }

    void reset()
    {
        tracker.reset();
        fastEnv.reset();
        slowEnv.reset();
        voiceEnv.reset();
        voiceLp = OnePole {};
        noteArmed = true;
        noteHoldoff = 0.0;
        for (auto& f : stages)
            f = OnePole {};
        for (auto& d : dryPrev)
            d = 0.0;
        voicePrev    = 0.0;
        sinceTrigger = 1e9; // filter resting at Stop Freq
        holdoff      = 0.0;
        armed        = true;
        fade         = 1.0;
        ducking      = false;
        gate         = 0.0;
        cutoffNow    = 0.0;
        triggerCount = 0;
    }

    void setParams (const Params& p)
    {
        threshold = triggerThreshold (p.trigger);
        noteLevel = std::pow (10.0, kNoteDb / 20.0);
        attackS   = std::clamp (p.attackS, 0.0, kMaxAttackS);
        sweepS    = sweepSeconds (p.rate);

        subGain.setTarget (kSubGain * std::clamp (p.subOctave, 0.0, 1.0));
        guitarGain.setTarget (kGuitarGain * std::clamp (p.guitar, 0.0, 1.0));
        octaveGain.setTarget (kOctaveGain * std::clamp (p.octave, 0.0, 1.0));
        squareGain.setTarget (kSquareGain * std::clamp (p.square, 0.0, 1.0));

        const double k = feedbackFor (p.resonance);
        feedback.setTarget (k);
        inGain.setTarget (inputGainFor (k));
        logStart.setTarget (std::log (std::clamp (p.startHz, kMinFreqHz, kMaxFreqHz)));
        logStop.setTarget (std::log (std::clamp (p.stopHz, kMinFreqHz, kMaxFreqHz)));

        mixAmount.setTarget (std::clamp (p.mix, 0.0, 1.0));
        outGain.setTarget (std::pow (10.0, p.outputDb / 20.0));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
    }

    void snapToTargets()
    {
        for (auto* s : allSmoothers())
            s->snap();
        cutoffNow = std::exp (logStop.current());
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int    nch       = std::min (numChannels, channels);
        const double gateCoeff = 1.0 - std::exp (-1.0 / (0.005 * fs));

        for (int n = 0; n < numSamples; ++n)
        {
            double x = 0.0;
            for (int ch = 0; ch < nch; ++ch)
                x += data[ch][n];
            x /= (double) std::max (1, nch);

            // --- note detection (Trigger) ----------------------------------------
            const double env  = fastEnv.process (x);
            const double slow = slowEnv.process (x);
            const bool   newPeak = env > 2.5 * slow; // a fresh attack over a ringing note
            holdoff -= 1.0;
            if (! armed && env < 0.5 * threshold)
                armed = true;
            if (env > threshold && (armed || newPeak) && holdoff <= 0.0)
                fireTrigger();

            // the Attack Delay has its own, fixed note detector (Trigger only affects the filter, as on the pedal)
            noteHoldoff -= 1.0;
            if (! noteArmed && env < 0.5 * noteLevel)
                noteArmed = true;
            if (env > noteLevel && (noteArmed || newPeak) && noteHoldoff <= 0.0)
            {
                noteArmed   = false;
                noteHoldoff = kRetriggerS * fs;
                if (attackS > 0.001 && fade > 0.0)
                    ducking = true; // legato: dip quickly, then fade in again
            }

            // --- voices (aligned with the band-limited synth voices: one sample late)
            const auto   v   = tracker.process (x);
            const double lvl = voiceLp.lowpass (voiceEnv.process (x), voiceLpA);
            gate += gateCoeff * (std::clamp ((20.0 * std::log10 (env + 1e-12) - kGateDb) / 8.0, 0.0, 1.0) - gate);

            const double synth = gate * lvl * (subGain.next() * v.sub + octaveGain.next() * v.octave + squareGain.next() * v.square);
            const double mix   = guitarGain.next() * voicePrev + synth;
            voicePrev          = x;

            // --- Attack Delay: fade the voices in after each trigger --------------
            if (attackS <= 0.001)
            {
                fade = 1.0;
                ducking = false;
            }
            else if (noteArmed)
            {
                fade = std::max (0.0, fade - 1.0 / (0.01 * fs)); // between notes: closed, ready to fade the next one in
            }
            else if (ducking)
            {
                fade -= 1.0 / (0.002 * fs); // quick 2 ms dip so the restart doesn't click
                if (fade <= 0.0) { fade = 0.0; ducking = false; }
            }
            else
            {
                fade = std::min (1.0, fade + 1.0 / (attackS * fs));
            }
            const double vca = fade * fade * (3.0 - 2.0 * fade); // smooth S-curve

            // --- swept 3-pole resonant low-pass ------------------------------------
            sinceTrigger += 1.0 / fs;
            const double ls = logStart.next(), le = logStop.next();
            cutoffNow = std::exp (le + (ls - le) * std::exp (-3.0 * sinceTrigger / sweepS));
            const double y = filter (mix * vca * inGain.next(), onePoleCoeff (cutoffNow, fs), feedback.next());

            const double wet = y;
            const double m   = mixAmount.next();
            const double og  = outGain.next();
            const double on  = active.next();
            for (int ch = 0; ch < nch; ++ch)
            {
                const double dry    = dryPrev[ch]; // keeps the dry path aligned with the one-sample latency
                dryPrev[ch]         = data[ch][n];
                const double effect = (dry + m * (wet - dry)) * og; // Mix, then Output
                data[ch][n]         = (float) (dry + on * (effect - dry)); // bypass crossfade
            }
        }
    }

    double currentCutoffHz() const { return cutoffNow; }
    double currentLevel() const { return fastEnv.current(); }
    double currentPitchHz() const { return tracker.pitchHz(); }
    int    triggers() const { return triggerCount; } // increments on every trigger (for the display LED)

private:
    void fireTrigger()
    {
        armed        = false;
        holdoff      = kRetriggerS * fs;
        sinceTrigger = 0.0;
        ++triggerCount;
        tracker.newNote();
    }

    // Zero-delay-feedback 3-pole ladder (three TPT one-poles with global feedback). The loop is
    // solved linearly, then a soft clip on the stage input keeps high Resonance warm and bounded.
    double filter (double x, double g, double k)
    {
        const double g2 = g * g, g3 = g2 * g, c = 1.0 - g;
        const double s  = g2 * c * stages[0].s + g * c * stages[1].s + c * stages[2].s;
        const double y3 = (g3 * x + s) / (1.0 + k * g3);
        const double u  = 1.5 * std::tanh ((x - k * y3) / 1.5);
        return stages[2].lowpass (stages[1].lowpass (stages[0].lowpass (u, g), g), g);
    }

    std::vector<Smoother*> allSmoothers()
    {
        return { &subGain, &guitarGain, &octaveGain, &squareGain, &feedback, &inGain, &logStart, &logStop, &mixAmount, &outGain, &active };
    }

    double   fs = 48000.0;
    int      channels = 2;
    Tracker  tracker;
    Envelope fastEnv, slowEnv, voiceEnv;
    OnePole  voiceLp;
    double   voiceLpA = 0.0;
    OnePole  stages[3];
    double   dryPrev[kMaxChannels] {};
    double   voicePrev = 0.0;

    double noteLevel = 0.003, noteHoldoff = 0.0;
    bool   noteArmed = true;
    double threshold = 0.01, attackS = 0.0, sweepS = 0.3;
    double sinceTrigger = 1e9, holdoff = 0.0, fade = 1.0, gate = 0.0, cutoffNow = 0.0;
    bool   armed = true, ducking = false;
    int    triggerCount = 0;

    Smoother subGain, guitarGain, octaveGain, squareGain, feedback, inGain, logStart, logStop, mixAmount, outGain, active;
};

} // namespace gsynth
