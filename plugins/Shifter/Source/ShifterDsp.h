// ShifterDsp.h - Melon Shifter DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// A dual-voice pitch shifter in the spirit of the classic studio "harmonizer" racks
// (Eventide H910 / H949): each voice is a delay line read at a different speed, with two read
// taps that take turns and crossfade, plus a delay with feedback on the shifted signal.
//
//                 ┌──────────────────────────── dry ─────────────────────────────┐
//   in ──► (+) ──►┤                                                              Mix ─► Output ─► out
//           ▲     └─► buffer ─┬─► Voice A (pitch, detune) ─► pan ─┐              │
//           │                 └─► Voice B (pitch, detune) ─► pan ─┴─► Tone ─► Delay ─┘
//           └──────────────────────── Feedback ◄──────────────────────────────┘
//
// Each voice: the read position moves at (1 - ratio) samples per sample relative to the write
// position, so the output plays at `ratio` times the speed. The read delay sweeps across a
// window (20 / 40 / 70 ms by Mode); when one tap reaches the end of the window it jumps back,
// while the other tap is fully up. The two taps' sin² windows always add up to 1.
//
// De-glitching (like the H949's): when a tap jumps, its new position is chosen by searching a
// few milliseconds for the point that best matches the waveform the other tap is playing, so
// the crossfade happens between two signals in phase - no hollow, phasey dips on sustained notes.
// The search uses a mono sum, and both channels share the same tap positions, so the stereo image
// stays stable.
#pragma once

#include "MelonDsp.h"
#include <array>
#include <string>
#include <vector>

namespace shifter
{
enum class Mode { tight, normal, smooth };

constexpr int    kMaxChannels  = 2;
constexpr double kSmoothingMs  = 20.0;
constexpr double kGlideMs      = 25.0;   // pitch changes glide (no zipper noise when sweeping)
constexpr double kDelaySmoothMs = 120.0;
constexpr double kMaxDelayMs   = 1000.0;
constexpr double kMaxFeedback  = 0.9;    // Feedback 100 % = 0.9 loop gain
constexpr double kMinDelayMs   = 0.25;   // smallest read delay of a tap
constexpr double kMaxGrainMs   = 400.0;  // longest crossfade cycle (for small shifts and detune)
constexpr int    kMaxSemitones = 24;

struct ModeSettings
{
    double windowMs, searchMs, matchMs;
};

// Tight: shortest window, lowest latency, best for single-note lines (some grain on chords).
// Smooth: longest window, best for chords and pads, more latency.
inline ModeSettings settingsFor (Mode m)
{
    switch (m)
    {
        case Mode::tight:  return { 20.0, 4.0, 8.0 };
        case Mode::smooth: return { 70.0, 12.0, 16.0 };
        case Mode::normal: break;
    }
    return { 40.0, 8.0, 12.0 };
}

// Typical delay of the shifted signal behind the dry signal
inline double latencyMs (Mode m)
{
    const auto s = settingsFor (m);
    return kMinDelayMs + 0.5 * (s.windowMs + s.searchMs);
}

inline double ratioFor (double semitones, double cents) { return std::pow (2.0, (semitones + cents * 0.01) / 12.0); }

inline std::string intervalName (int semitones)
{
    static const char* names[] = { "Unison", "Minor 2nd", "Major 2nd", "Minor 3rd", "Major 3rd", "Perfect 4th",
                                   "Tritone", "Perfect 5th", "Minor 6th", "Major 6th", "Minor 7th", "Major 7th", "Octave" };
    const int a = std::abs (semitones);
    if (a == 0)
        return names[0];
    std::string s;
    if (a <= 12)
        s = names[a];
    else if (a == 24)
        s = "2 Octaves";
    else
        s = std::string ("Octave + ") + names[a - 12];
    return s + (semitones > 0 ? " up" : " down");
}

struct Params
{
    double pitchA   = 12.0;  // semitones
    double detuneA  = 0.0;   // cents
    double pitchB   = -12.0;
    double detuneB  = 0.0;
    bool   voiceB   = false;
    double mix      = 0.5;   // 0 = dry, 1 = wet
    double width    = 0.6;   // 0..1, spreads voice A left and voice B right
    double delayMs  = 0.0;
    double feedback = 0.0;   // 0..1 (x kMaxFeedback)
    double toneHz   = 20000.0;
    Mode   mode     = Mode::normal;
    double outputDb = 0.0;
    bool   bypassed = false;
};

//==============================================================================
// One pitch voice: positions and weights of its two read taps. The audio itself is read by the
// engine from each channel's buffer, so every channel shares the same taps.
class Voice
{
public:
    void prepare (double sampleRate, Mode m, int maxReadable)
    {
        fs       = sampleRate;
        maxDelay = (double) maxReadable;
        dMin     = std::max (3.0, kMinDelayMs * 0.001 * fs);
        step     = std::max (1, (int) (fs / 16000.0)); // search resolution (keeps the CPU cost flat at high rates)
        setMode (m);
        reset();
    }

    void reset()
    {
        phase = 0.0;
        d[0] = d[1] = dMin;
        fresh = true;
    }

    void setMode (Mode m)
    {
        const auto s = settingsFor (m);
        window = s.windowMs * 0.001 * fs;
        search = (int) (s.searchMs * 0.001 * fs);
        match  = (int) (s.matchMs * 0.001 * fs);
        gMax   = std::max (8.0 * window, kMaxGrainMs * 0.001 * fs);

        // Hann taper for the waveform match: without it, a match length that is not a whole
        // number of periods biases the choice by a few samples.
        taper.clear();
        for (int j = 0; j < match; j += step)
        {
            const double t = std::sin (melon::kPi * (j + 0.5) / match);
            taper.push_back (t * t);
        }
    }

    // Advances one sample. `mono` is the buffer used for the splice search.
    void tick (double ratio, const melon::DelayLine& mono, bool searchEnabled)
    {
        const double slope = 1.0 - ratio;
        const double a     = std::abs (slope);
        const double grain = std::min (gMax, window / std::max (a, 1e-9));
        lastGrain = grain;

        if (fresh)
        {
            // start with tap 1 half-way through its life, so the first grain is already shifted;
            // pitch up: tap 0 starts at the far end, so it has room to travel towards dMin
            fresh = false;
            const double span = std::min (window, a * grain);
            d[0]  = dMin + (slope < 0.0 ? span : 0.0);
            d[1]  = dMin + 0.5 * span;
        }

        // A tap pinned at the end of the buffer it is moving towards stops moving, and a tap
        // that stops moving plays the input at its ORIGINAL pitch (e.g. right after the pitch
        // goes from 0 or below to an upward shift). Speed up the crossfade until it is respawned,
        // so this lasts one short window instead of up to a whole grain (~330 ms at +2 st).
        bool pinned = false;
        for (auto& v : d)
        {
            const double next = v + slope;
            pinned = pinned || (slope < 0.0 && next <= dMin) || (slope > 0.0 && next >= maxDelay);
            v = std::clamp (next, dMin, maxDelay);
        }

        const double prev = phase;
        phase += 1.0 / (pinned ? std::min (grain, window) : grain);

        if (phase >= 1.0)
        {
            phase -= 1.0;
            respawn (0, slope, grain, mono, searchEnabled);
        }
        if (prev < 0.5 && phase >= 0.5)
            respawn (1, slope, grain, mono, searchEnabled);

        const double s = std::sin (melon::kPi * phase);
        w[0] = s * s;     // tap 0 at `phase`
        w[1] = 1.0 - w[0]; // tap 1 at `phase + 0.5`: sin² + cos² = 1
    }

    double read (const melon::DelayLine& buf) const { return w[0] * buf.read (d[0]) + w[1] * buf.read (d[1]); }

    double tapDelay (int i) const { return d[i]; }
    double tapWeight (int i) const { return w[i]; }
    double grainSamples() const { return lastGrain; }

private:
    void respawn (int i, double slope, double grain, const melon::DelayLine& mono, bool searchEnabled)
    {
        // Pitch up: the delay shrinks during the grain, so start at the far end of the window.
        const double base = dMin + (slope < 0.0 ? std::min (window, -slope * grain) : 0.0);
        double best = base;

        if (searchEnabled && search > 0)
        {
            const double other = d[1 - i];
            const int    o     = (int) (other + 0.5);
            double bestScore   = -1e300;
            for (int k = 0; k <= search; k += step)
            {
                const int c = (int) (base + 0.5) + k;
                if (c + match >= (int) maxDelay)
                    break;
                const double score = matchScore (mono, o, c);
                if (score > bestScore)
                {
                    bestScore = score;
                    best      = base + k;
                }
            }
            // refine the coarse result to the nearest sample
            if (step > 1)
            {
                const double coarse = best;
                const int    centre = (int) (coarse + 0.5);
                for (int c = centre - step + 1; c < centre + step; ++c)
                {
                    if (c == centre || c < (int) dMin || c + match >= (int) maxDelay)
                        continue;
                    const double score = matchScore (mono, o, c);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best      = coarse + (c - centre);
                    }
                }
            }
            // sub-sample precision: parabolic peak fit, plus the other tap's fractional part
            const int b = (int) (best + 0.5);
            if (b - 1 >= (int) dMin && b + 1 + match < (int) maxDelay)
            {
                const double sm = matchScore (mono, o, b - 1), s0 = matchScore (mono, o, b), sp = matchScore (mono, o, b + 1);
                const double den = sm - 2.0 * s0 + sp;
                const double frac = den < -1e-12 ? std::clamp (0.5 * (sm - sp) / den, -0.5, 0.5) : 0.0;
                best = b + frac + (other - o);
            }
        }
        d[i] = std::clamp (best, dMin, maxDelay);
    }

    // How well the waveform at delay c matches the one at delay o (normalised, tapered)
    double matchScore (const melon::DelayLine& mono, int o, int c) const
    {
        double corr = 0.0, energy = 0.0;
        int    j = 0;
        for (int m = 0; m < match; m += step, ++j)
        {
            const double t = taper[(size_t) j];
            const double y = mono.readInt (c + m);
            corr   += t * mono.readInt (o + m) * y;
            energy += t * y * y;
        }
        return energy > 1e-12 ? corr / std::sqrt (energy) : 0.0;
    }

    std::vector<double> taper;
    double fs = 48000.0, maxDelay = 1000.0, dMin = 12.0;
    double window = 1920.0, gMax = 19200.0, lastGrain = 1920.0;
    int    search = 384, match = 576, step = 3;
    double phase = 0.0;
    bool   fresh = true;
    double d[2] { 12.0, 12.0 };
    double w[2] { 0.0, 1.0 };
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);

        // big enough for the Smooth mode, so changing Mode never reallocates
        const auto big     = settingsFor (Mode::smooth);
        const int  shiftMax = (int) std::ceil ((big.windowMs + big.searchMs + big.matchMs + kMinDelayMs) * 0.001 * fs) + 64;
        mono.prepare (shiftMax);
        for (auto& c : ch)
        {
            c.buf.prepare (shiftMax);
            c.wet.prepare ((int) std::ceil (kMaxDelayMs * 0.001 * fs) + 8);
        }
        const int readable = shiftMax - 8; // taps + splice search stay inside the buffers
        voiceA.prepare (fs, params.mode, readable);
        voiceB.prepare (fs, params.mode, readable);
        mode = params.mode;

        for (auto* s : smoothers())
            s->prepare (fs, kSmoothingMs);
        semiA.prepare (fs, kGlideMs);
        semiB.prepare (fs, kGlideMs);
        delaySamples.prepare (fs, kDelaySmoothMs);
        lastTone = -1.0;
        updateTone (params.toneHz);
        reset();
    }

    void reset()
    {
        voiceA.reset();
        voiceB.reset();
        mono.reset();
        for (auto& c : ch)
        {
            c.buf.reset(); c.wet.reset(); c.tone.reset();
        }
    }

    void setParams (const Params& p)
    {
        params = p;
        if (p.mode != mode)
        {
            mode = p.mode;
            voiceA.setMode (mode);
            voiceB.setMode (mode);
        }
        semiA.setTarget (std::clamp (p.pitchA + p.detuneA * 0.01, -(double) kMaxSemitones - 0.5, (double) kMaxSemitones + 0.5));
        semiB.setTarget (std::clamp (p.pitchB + p.detuneB * 0.01, -(double) kMaxSemitones - 0.5, (double) kMaxSemitones + 0.5));
        bOn.setTarget (p.voiceB ? 1.0 : 0.0);
        double dg, wg;
        melon::equalPowerMix (p.mix, dg, wg);
        dryGain.setTarget (dg);
        wetGain.setTarget (wg);
        width.setTarget (std::clamp (p.width, 0.0, 1.0));
        feedback.setTarget (std::clamp (p.feedback, 0.0, 1.0) * kMaxFeedback);
        delaySamples.setTarget (std::clamp (p.delayMs, 0.0, kMaxDelayMs) * 0.001 * fs);
        outGain.setTarget (melon::dbToGain (p.outputDb));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
        updateTone (p.toneHz);
    }

    void snapToTargets()
    {
        for (auto* s : smoothers())
            s->snap();
        semiA.snap();
        semiB.snap();
        delaySamples.snap();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int nch = std::min (numChannels, channels);
        constexpr double quarterPi = 0.25 * melon::kPi;
        constexpr double kSqrt2    = 1.41421356237309505; // centre pan = unity

        for (int n = 0; n < numSamples; ++n)
        {
            const double rA = std::pow (2.0, semiA.next() / 12.0);
            const double rB = std::pow (2.0, semiB.next() / 12.0);
            const double b = bOn.next(), dg = dryGain.next(), wg = wetGain.next(), wd = width.next();
            const double fb = feedback.next(), dly = std::max (1.0, delaySamples.next());
            const double og = outGain.next(), on = active.next();

            // shifter input = input + feedback of the delayed wet signal
            double delayed[kMaxChannels] {}, monoIn = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                auto& st   = ch[(size_t) c];
                delayed[c] = st.wet.read (dly);
                const double in = data[c][n] + fb * std::tanh (delayed[c]);
                st.buf.push ((float) in);
                monoIn += in;
            }
            mono.push ((float) (monoIn / nch));

            voiceA.tick (rA, mono, true);
            voiceB.tick (rB, mono, params.voiceB || b > 1e-4);

            // voice A pans left and voice B right by Width (A stays centred when B is off)
            const double norm = 1.0 / std::sqrt (1.0 + b);
            const double panA = -wd * b, panB = wd;
            const double gA[2] = { std::cos ((panA + 1.0) * quarterPi) * kSqrt2, std::sin ((panA + 1.0) * quarterPi) * kSqrt2 };
            const double gB[2] = { std::cos ((panB + 1.0) * quarterPi) * kSqrt2, std::sin ((panB + 1.0) * quarterPi) * kSqrt2 };

            for (int c = 0; c < nch; ++c)
            {
                auto& st = ch[(size_t) c];
                const double ya = voiceA.read (st.buf);
                const double yb = b > 1e-4 ? voiceB.read (st.buf) : 0.0;
                const double wet = nch == 2 ? norm * (ya * gA[c] + b * yb * gB[c]) : norm * (ya + b * yb);
                st.wet.push ((float) st.tone.process (wet));

                const double x   = data[c][n];
                const double y   = (dg * x + wg * delayed[c]) * og;
                data[c][n] = (float) (x + on * (y - x));
            }
        }
    }

    const Voice& voice (int i) const { return i == 0 ? voiceA : voiceB; }

private:
    struct Channel
    {
        melon::DelayLine buf, wet;
        melon::Biquad    tone;
    };

    void updateTone (double hz)
    {
        if (std::abs (hz - lastTone) < 0.5 || fs <= 0.0)
            return;
        lastTone = hz;
        for (auto& c : ch)
        {
            if (hz >= 19999.0)
                c.tone.setIdentity();
            else
                c.tone.set (melon::Biquad::Type::lowpass, fs, hz, 0.707);
        }
    }

    std::array<melon::Smoother*, 7> smoothers() { return { &bOn, &dryGain, &wetGain, &width, &feedback, &outGain, &active }; }

    Params params;
    Mode   mode = Mode::normal;
    double fs = 48000.0, lastTone = -1.0;
    int    channels = 2;
    melon::DelayLine mono;
    std::array<Channel, kMaxChannels> ch;
    Voice voiceA, voiceB;
    melon::Smoother semiA, semiB, bOn, dryGain, wetGain, width, feedback, delaySamples, outGain, active;
};

} // namespace shifter
