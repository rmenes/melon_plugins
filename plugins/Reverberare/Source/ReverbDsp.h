// ReverbDsp.h - Melon Reverberare DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// A multi-machine stereo reverb in the spirit of the Strymon BigSky: twelve "machines"
// (Room, Hall, Plate, Spring, Swell, Bloom, Cloud, Chorale, Shimmer, Magneto, Nonlinear,
// Reflections) sharing the same knobs: Decay, Pre-delay, Tone, Mod, Mix, plus two
// machine-specific parameters (Param 1 / Param 2) and Infinite (freeze).
//
//   in ─► pre-delay ─► [swell] ─► [tape echo] ─► diffusers ─► [bloom/cloud diffusers] ─┐
//        ┌─────────────────────────────────────────────────────────────────────────────┘
//        ├─► early reflections ─────────────────────────────────────────────────┐
//        └─► 8-line feedback delay network (Hadamard mix, modulated, damped) ───(+)─► [spring dispersion]
//              ▲                                                                  │   [vowel formants]
//              └──── [pitch shifter: shimmer] ◄───────────────────────────────────┘        │
//   in ──────────────────────────────────────────────────────────────── dry / wet Mix ◄────┘
//
// Decay sets the RT60 of the network directly (the time for the tail to fall by 60 dB);
// each line's feedback gain is computed from its length, so the decay is the same whatever
// the room size. "Low decay" multiplies the decay time of the bass (below ~250 Hz).
#pragma once

#include "MelonDsp.h"
#include <array>
#include <cstdio>
#include <string>

namespace reverb
{
using melon::kPi;
using melon::kTwoPi;

enum Machine { room, hall, plate, spring, swell, bloom, cloud, chorale, shimmer, magneto, nonlinear, reflections, kNumMachines };

struct MachineInfo
{
    const char* name;
    const char* param1;
    const char* param2;
    const char* description;
};

inline const std::array<MachineInfo, kNumMachines>& machines()
{
    static const std::array<MachineInfo, kNumMachines> m { {
        { "Room",        "Size",      "Early/Late", "Small to medium rooms. Param 2 balances early reflections and tail" },
        { "Hall",        "Size",      "Low Decay",  "Large concert halls with a slow, smooth build-up" },
        { "Plate",       "Size",      "Low Decay",  "Bright, dense studio plate with no early reflections" },
        { "Spring",      "Dwell",     "Drip",       "Amp-style spring tank. Dwell drives the springs; Drip sets the boing" },
        { "Swell",       "Rise",      "Low Decay",  "Notes fade in (volume swell) into a lush reverb" },
        { "Bloom",       "Length",    "Feedback",   "The reverb grows after the note: a slow, blooming build-up" },
        { "Cloud",       "Diffusion", "Low Decay",  "Huge, smeared ambient wash with no clear attack" },
        { "Chorale",     "Vowel",     "Formant",    "Reverb tail shaped by vocal formants (a choir-like tail)" },
        { "Shimmer",     "Interval",  "Shimmer",    "Pitch-shifted voices regenerate inside the tail" },
        { "Magneto",     "Echo Time", "Repeats",    "Multi-head tape echo feeding a reverb" },
        { "Nonlinear",   "Shape",     "Diffusion",  "Gated, reverse and swooshing effects. Decay sets the length" },
        { "Reflections", "Room Size", "Diffusion",  "Early reflections only: the sound of the room's walls" },
    } };
    return m;
}

constexpr int    kLines        = 8;
constexpr double kMaxPreDelayMs = 500.0;
constexpr double kSmoothingMs  = 30.0;
constexpr double kMachineFadeMs = 30.0;

struct Params
{
    int    machine    = hall;
    double decayS     = 3.0;
    double preDelayMs = 20.0;
    double tone       = 0.5; // 0 dark .. 1 bright
    double mod        = 0.3; // 0..1
    double param1     = 0.5;
    double param2     = 0.5;
    double mix        = 0.35;
    double outputDb   = 0.0;
    bool   infinite   = false;
    bool   bypassed   = false;
};

// Shimmer intervals for Param 1 (in semitones)
inline const std::array<std::pair<double, const char*>, 5>& shimmerIntervals()
{
    static const std::array<std::pair<double, const char*>, 5> v { { { -12.0, "Octave down" }, { 7.0, "Fifth up" }, { 12.0, "Octave up" },
                                                                     { 19.0, "Octave + fifth" }, { 24.0, "Two octaves" } } };
    return v;
}
inline int shimmerIndex (double p1) { return std::clamp ((int) (p1 * 5.0), 0, 4); }

// Text for Param 1 / Param 2 of a machine (for the editor)
inline std::string paramText (int machine, int which, double v)
{
    char buf[64];
    if (machine == shimmer && which == 1)
        return shimmerIntervals()[(size_t) shimmerIndex (v)].second;
    if (machine == magneto && which == 1)
    {
        std::snprintf (buf, sizeof buf, "%.0f ms", 60.0 + v * 640.0);
        return buf;
    }
    if (machine == swell && which == 1)
    {
        std::snprintf (buf, sizeof buf, "%.2f s", 0.05 + v * v * 2.95);
        return buf;
    }
    if (machine == chorale && which == 1)
    {
        const char* vowels[] = { "oo", "oh", "ah", "eh", "ee" };
        return vowels[std::clamp ((int) std::lround (v * 4.0), 0, 4)];
    }
    if ((machine == hall || machine == plate || machine == swell || machine == cloud) && which == 2)
    {
        std::snprintf (buf, sizeof buf, "x%.2f", 0.5 + 1.5 * v);
        return buf;
    }
    std::snprintf (buf, sizeof buf, "%.0f %%", v * 100.0);
    return buf;
}

//==============================================================================
// Schroeder allpass on a delay line (optionally modulated)
class Allpass
{
public:
    void   prepare (int maxSamples) { line.prepare (maxSamples + 16); }
    void   reset() { line.reset(); }
    double process (double x, double delay, double g)
    {
        const double d = line.read (delay);
        const double w = x + g * d;
        line.push ((float) w);
        return d - g * w;
    }

private:
    melon::DelayLine line;
};

// First-order allpass (dispersion for the spring)
struct Allpass1
{
    double z = 0.0;
    double process (double x, double a)
    {
        const double y = a * x + z;
        z = x - a * y;
        return y;
    }
};

// Two-tap crossfading delay pitch shifter
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        fs     = sampleRate;
        window = 0.06 * fs;
        line.prepare ((int) (window * 2.0) + 8);
    }
    void reset() { line.reset(); phase = 0.0; }
    double process (double x, double semitones)
    {
        line.push ((float) x);
        const double ratio = std::pow (2.0, semitones / 12.0);
        phase += (1.0 - ratio) / window; // delay grows or shrinks to move the pitch
        phase -= std::floor (phase);
        const double p2 = std::fmod (phase + 0.5, 1.0);
        const double a  = line.read (2.0 + phase * window), b = line.read (2.0 + p2 * window);
        const double wa = std::sin (kPi * phase), wb = std::sin (kPi * p2); // sin windows: wa² + wb² = 1
        return a * wa + b * wb;
    }

private:
    melon::DelayLine line;
    double fs = 48000.0, window = 2880.0, phase = 0.0;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int /*numChannels*/)
    {
        fs = sampleRate;
        const double scale = fs / 1000.0;
        for (int c = 0; c < 2; ++c)
        {
            pre[c].prepare ((int) (kMaxPreDelayMs * scale) + 8);
            for (auto& a : diff[c]) a.prepare ((int) (20.0 * scale));
            for (auto& a : longDiff[c]) a.prepare ((int) (180.0 * scale));
            er[c].prepare ((int) (260.0 * scale));
            echo[c].prepare ((int) (2.2 * fs));
            taps[c].prepare ((int) (1.7 * fs));
            shifter[c].prepare (fs);
        }
        for (auto& l : lines) l.prepare ((int) (190.0 * scale));
        for (auto* s : smoothers()) s->prepare (fs, kSmoothingMs);
        fadeStep = 1.0 / std::max (1.0, kMachineFadeMs * 0.001 * fs);
        applyMachine (params.machine);
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            pre[c].reset(); er[c].reset(); echo[c].reset(); taps[c].reset(); shifter[c].reset();
            for (auto& a : diff[c]) a.reset();
            for (auto& a : longDiff[c]) a.reset();
            for (auto& a : disp[c]) a.z = 0.0;
            for (auto& f : formant[c]) f.reset();
            springHp[c].reset(); springLp[c].reset(); echoLp[c].reset(); wetHp[c].reset();
            bloomFb[c] = 0.0; shimmerFb[c] = 0.0; echoFb[c] = 0.0;
        }
        for (auto& l : lines) l.reset();
        for (auto& d : damp) d.reset();
        for (auto& d : lowSplit) d.reset();
        swellGain = 0.0; swellQuiet = 0; inEnv = 0.0;
    }

    void setParams (const Params& p)
    {
        params = p;
        params.machine = std::clamp (p.machine, 0, kNumMachines - 1);
        decay.setTarget (std::clamp (p.decayS, 0.1, 60.0));
        preDelay.setTarget (std::clamp (p.preDelayMs, 0.0, kMaxPreDelayMs) * 0.001 * fs);
        tone.setTarget (std::clamp (p.tone, 0.0, 1.0));
        mod.setTarget (std::clamp (p.mod, 0.0, 1.0));
        p1.setTarget (std::clamp (p.param1, 0.0, 1.0));
        p2.setTarget (std::clamp (p.param2, 0.0, 1.0));
        double d, w;
        melon::equalPowerMix (p.mix, d, w);
        dryGain.setTarget (d);
        wetGain.setTarget (w);
        outGain.setTarget (melon::dbToGain (p.outputDb));
        freeze.setTarget (p.infinite ? 1.0 : 0.0);
        active.setTarget (p.bypassed ? 0.0 : 1.0);
    }

    void snapToTargets()
    {
        for (auto* s : smoothers()) s->snap();
        if (params.machine != current)
            applyMachine (params.machine);
        fade = 1.0;
        updateBlockValues();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int nch = std::clamp (numChannels, 1, 2);

        for (int n = 0; n < numSamples; ++n)
        {
            if (params.machine != current)
            {
                fade -= fadeStep;
                if (fade <= 0.0)
                {
                    fade = 0.0;
                    applyMachine (params.machine);
                    reset();
                }
            }
            else if (fade < 1.0)
                fade = std::min (1.0, fade + fadeStep);

            for (auto* s : smoothers()) s->next();
            if ((n & 31) == 0)
                updateBlockValues();

            const double inL = data[0][n];
            const double inR = nch > 1 ? data[1][n] : inL;
            double wetL = 0.0, wetR = 0.0;
            tick (inL, inR, wetL, wetR);

            const double dg = dryGain.value(), wg = wetGain.value() * fade, og = outGain.value(), on = active.value();
            const double outL = (dg * inL + wg * wetL) * og;
            const double outR = (dg * inR + wg * wetR) * og;
            data[0][n] = (float) (inL + on * (outL - inL));
            if (nch > 1)
                data[1][n] = (float) (inR + on * (outR - inR));
        }
    }

private:
    //--------------------------------------------------------------------------
    // Values that change slowly: recomputed every 32 samples from the smoothed parameters
    void updateBlockValues()
    {
        const auto m  = (Machine) current;
        const double a = p1.value(), b = p2.value();

        // network size
        double size = 1.0;
        switch (m)
        {
            case room:        size = 0.25 + 0.35 * a; break;
            case hall:        size = 0.7 + 0.8 * a; break;
            case plate:       size = 0.45 + 0.5 * a; break;
            case spring:      size = 0.35; break;
            case swell:       size = 0.9; break;
            case bloom:       size = 0.9; break;
            case cloud:       size = 1.4; break;
            case chorale:     size = 0.9; break;
            case shimmer:     size = 1.0; break;
            case magneto:     size = 0.6; break;
            case nonlinear:   size = 0.5; break;
            case reflections: size = 0.5; break;
            case kNumMachines: break;
        }

        // low decay multiplier
        double lowMult = 1.0;
        if (m == hall || m == plate || m == swell || m == cloud)
            lowMult = 0.5 + 1.5 * b;
        else if (m == spring)
            lowMult = 0.6;

        const double t60   = decay.value();
        const double frz   = freeze.value();
        const double dampHz = 1500.0 * std::pow (12.0, tone.value()); // 1.5 kHz .. 18 kHz
        for (int i = 0; i < kLines; ++i)
        {
            lineDelay[i]   = kLineMs[i] * size * 0.001 * fs;
            const double sec = lineDelay[i] / fs;
            const double gHi = std::pow (10.0, -3.0 * sec / t60);
            const double gLo = std::pow (10.0, -3.0 * sec / (t60 * lowMult));
            lineGainHi[i] = gHi + frz * (1.0 - gHi);
            lineGainLo[i] = gLo + frz * (1.0 - gLo);
            damp[i].setCutoff (fs, dampHz);
            lowSplit[i].setCutoff (fs, 250.0);
        }
        dampMix = 1.0 - frz;

        // modulation depth (samples at 48 kHz, scaled)
        double md = mod.value() * 10.0;
        if (m == chorale || m == shimmer || m == cloud) md *= 1.5;
        if (m == spring || m == reflections || m == nonlinear) md *= 0.3;
        modDepth = md * fs / 48000.0;

        diffCoeff  = (m == plate || m == cloud) ? 0.75 : (m == reflections || m == nonlinear) ? 0.4 + 0.35 * b : 0.62;
        erLevel    = m == room ? 1.2 * (1.0 - b) : m == reflections ? 1.0 : m == hall ? 0.25 : 0.0;
        lateLevel  = m == room ? 0.35 + 0.65 * b : (m == reflections || m == nonlinear) ? 0.0 : 1.0;
        erScale    = m == reflections ? 0.3 + 2.2 * a : m == room ? 0.4 + 0.6 * a : 1.0;

        longDiffStages = m == cloud ? 6 : m == bloom ? 3 : 0;
        longDiffCoeff  = m == cloud ? 0.5 + 0.3 * a : 0.7;
        longDiffScale  = m == bloom ? 0.3 + 1.2 * a : 1.0;
        bloomFeedback  = m == bloom ? 0.85 * b : 0.0;

        springDrive   = 1.0 + 7.0 * a;
        springDisp    = 0.35 + 0.5 * b;

        swellRise     = (0.05 + a * a * 2.95) * fs;

        shimmerSemis  = shimmerIntervals()[(size_t) shimmerIndex (a)].first;
        shimmerAmount = m == shimmer ? 0.6 * b : 0.0;

        echoTime     = (60.0 + 640.0 * a) * 0.001 * fs;
        echoRepeats  = 0.85 * b;

        nlWindow     = std::clamp (t60 * 0.25, 0.08, 1.5) * fs;
        nlShape      = a;

        if (m == chorale)
            setFormants (a);
        formantMix = m == chorale ? b : 0.0;

        // wet level per machine (keeps the machines roughly level-matched)
        static constexpr double kWet[kNumMachines] = { 0.70, 1.06, 1.10, 0.38, 1.06, 1.28, 1.94, 1.27, 1.26, 0.50, 0.37, 0.47 };
        wetTrim = kWet[current];
    }

    void setFormants (double vowel)
    {
        // formant frequencies (F1, F2, F3) of five vowels, interpolated
        static constexpr double f[5][3] = { { 300, 870, 2240 }, { 400, 800, 2600 }, { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 } };
        const double pos = std::clamp (vowel, 0.0, 1.0) * 4.0;
        const int    i0 = std::min (3, (int) pos);
        const double t  = pos - i0;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 3; ++k)
                formant[c][k].set (melon::Biquad::Type::bandpass, fs, f[i0][k] + t * (f[i0 + 1][k] - f[i0][k]), 6.0);
    }

    void applyMachine (int idx)
    {
        current = std::clamp (idx, 0, kNumMachines - 1);
        for (int c = 0; c < 2; ++c)
        {
            springHp[c].set (melon::Biquad::Type::highpass, fs, 180.0, 0.707);
            springLp[c].set (melon::Biquad::Type::lowpass, fs, 4500.0, 0.9);
            echoLp[c].setCutoff (fs, 3500.0);
            wetHp[c].setCutoff (fs, 60.0);
        }
        updateBlockValues();
    }

    //--------------------------------------------------------------------------
    void tick (double inL, double inR, double& outL, double& outR)
    {
        const auto m  = (Machine) current;
        const double frz = freeze.value();
        double x[2] = { inL, inR };

        // input level (for Swell)
        const double lvl = std::max (std::abs (inL), std::abs (inR));
        inEnv = std::max (lvl, inEnv * 0.9995);

        for (int c = 0; c < 2; ++c)
        {
            // pre-delay
            pre[c].push ((float) x[c]);
            x[c] = pre[c].read (std::max (1.0, preDelay.value()));
        }

        // Swell: fade the input in after each new note (re-arms after ~60 ms of near silence)
        if (m == swell)
        {
            if (inEnv < 0.003) { if (++swellQuiet > (int) (0.06 * fs)) swellGain = 0.0; }
            else { swellQuiet = 0; swellGain = std::min (1.0, swellGain + 1.0 / std::max (1.0, swellRise)); }
            const double gsw = swellGain * swellGain;
            x[0] *= gsw; x[1] *= gsw;
        }

        // Magneto: three-head tape echo in front of the reverb
        double echoOut[2] = { 0.0, 0.0 };
        if (m == magneto)
        {
            wow += 0.6 / fs; if (wow >= 1.0) wow -= 1.0;
            const double flutter = 1.0 + 0.002 * std::sin (kTwoPi * wow) * (0.3 + mod.value());
            for (int c = 0; c < 2; ++c)
            {
                echo[c].push ((float) (x[c] + echoRepeats * echoFb[c]));
                const double t = echoTime * flutter;
                const double h1 = echo[c].read (t * 0.5), h2 = echo[c].read (t * 0.75), h3 = echo[c].read (t);
                echoFb[c] = std::tanh (echoLp[c].lowpass (h3));
                echoOut[c] = 0.5 * (h1 + h2) + h3;
                x[c] = x[c] + 0.6 * echoOut[c];
            }
        }

        // Spring: drive into the tank
        if (m == spring)
            for (int c = 0; c < 2; ++c)
                x[c] = std::tanh (springDrive * x[c]) / std::sqrt (springDrive);

        // input diffusion
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 4; ++i)
                x[c] = diff[c][i].process (x[c], kDiffMs[c][i] * 0.001 * fs, diffCoeff);

        // long diffusers (Bloom: with feedback; Cloud: six stages)
        if (longDiffStages > 0)
            for (int c = 0; c < 2; ++c)
            {
                double v = x[c] + bloomFeedback * bloomFb[c];
                for (int i = 0; i < longDiffStages; ++i)
                    v = longDiff[c][i].process (v, kLongMs[c][i] * longDiffScale * 0.001 * fs, longDiffCoeff);
                bloomFb[c] = std::tanh (v);
                x[c] = m == bloom ? v : 0.5 * (x[c] + v);
            }

        // freeze: stop feeding new sound in
        const double feed = 1.0 - frz;
        x[0] *= feed; x[1] *= feed;

        // early reflections
        double erOut[2] = { 0.0, 0.0 };
        if (erLevel > 0.0)
            for (int c = 0; c < 2; ++c)
            {
                er[c].push ((float) x[c]);
                for (int k = 0; k < 12; ++k)
                    erOut[c] += kErGain[k] * er[c].read (std::max (1.0, kErMs[c][k] * erScale * 0.001 * fs));
                erOut[c] *= erLevel * 0.45;
            }

        // nonlinear: a window of taps shaped by Param 1 (reverse -> gate -> decay)
        double nlOut[2] = { 0.0, 0.0 };
        if (m == nonlinear)
            for (int c = 0; c < 2; ++c)
            {
                taps[c].push ((float) x[c]);
                for (int k = 0; k < 32; ++k)
                {
                    const double pos = (k + 0.5 + 0.37 * ((k * 7 + c * 3) % 5 - 2) / 2.0) / 32.0; // jittered 0..1
                    const double env = nlShape < 0.5 ? (1.0 - 2.0 * nlShape) * pos + 2.0 * nlShape * 1.0
                                                     : (2.0 - 2.0 * nlShape) * 1.0 + (2.0 * nlShape - 1.0) * (1.0 - pos);
                    nlOut[c] += env * ((k & 1) ? -1.0 : 1.0) * taps[c].read (std::max (1.0, pos * nlWindow));
                }
                nlOut[c] *= 0.22;
            }

        // --- feedback delay network ---
        double late[2] = { 0.0, 0.0 };
        if (lateLevel > 0.0)
        {
            double y[kLines];
            for (int i = 0; i < kLines; ++i)
            {
                lfoPhase[i] += kLfoHz[i] / fs;
                if (lfoPhase[i] >= 1.0) lfoPhase[i] -= 1.0;
                const double d = lineDelay[i] + modDepth * (1.0 + std::sin (kTwoPi * lfoPhase[i]));
                const double raw = lines[i].read (std::max (2.0, d));
                // two-band decay (lows decay with Low Decay) + high damping (Tone)
                const double lo  = lowSplit[i].lowpass (raw);
                const double fbk = lineGainLo[i] * lo + lineGainHi[i] * (raw - lo);
                const double dmp = damp[i].lowpass (fbk);
                y[i] = dampMix * dmp + (1.0 - dampMix) * raw;
            }

            late[0] = 0.35 * (y[0] - y[1] + y[2] - y[3] + y[4] - y[5] + y[6] - y[7]);
            late[1] = 0.35 * (y[0] + y[1] - y[2] - y[3] + y[4] + y[5] - y[6] - y[7]);

            // shimmer: pitch-shifted tail fed back in
            double shim[2] = { 0.0, 0.0 };
            if (shimmerAmount > 0.0)
                for (int c = 0; c < 2; ++c)
                {
                    shimmerFb[c] = shifter[c].process (late[c], shimmerSemis);
                    shim[c] = shimmerAmount * (1.0 - frz) * std::tanh (shimmerFb[c]);
                }

            hadamard (y);
            const double inA = x[0] + shim[0], inB = x[1] + shim[1];
            for (int i = 0; i < kLines; ++i)
            {
                const double inj = (i & 1) ? inB : inA;
                double v = y[i] + 0.5 * inj * ((i & 2) ? -1.0 : 1.0);
                if (std::abs (v) > 1.0) // safety: keeps extreme settings (long decay + shimmer) bounded
                    v = std::copysign (1.0 + std::tanh (std::abs (v) - 1.0), v);
                lines[i].push ((float) v);
            }
        }

        for (int c = 0; c < 2; ++c)
        {
            double w = lateLevel * late[c] + erOut[c] + nlOut[c];
            if (m == magneto)
                w += 0.5 * echoOut[c];

            if (m == spring)
            {
                for (auto& a : disp[c])
                    w = a.process (w, springDisp);
                w = springLp[c].process (springHp[c].process (w)) * 1.5;
            }
            if (formantMix > 0.0)
            {
                double f = 0.0;
                for (auto& b : formant[c]) f += b.process (w);
                w = (1.0 - formantMix) * w + formantMix * 2.5 * f;
            }
            w = wetHp[c].highpass (w);
            (c == 0 ? outL : outR) = w * wetTrim;
        }
    }

    static void hadamard (double* v)
    {
        for (int len = 1; len < kLines; len <<= 1)
            for (int i = 0; i < kLines; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const double a = v[j], b = v[j + len];
                    v[j] = a + b;
                    v[j + len] = a - b;
                }
        const double norm = 1.0 / std::sqrt ((double) kLines);
        for (int i = 0; i < kLines; ++i) v[i] *= norm;
    }

    std::vector<melon::Smoother*> smoothers()
    {
        return { &decay, &preDelay, &tone, &mod, &p1, &p2, &dryGain, &wetGain, &outGain, &freeze, &active };
    }

    // network line lengths (ms at size 1): mutually prime-ish
    static constexpr double kLineMs[kLines] = { 29.7, 37.1, 41.1, 43.7, 53.0, 59.9, 67.1, 73.9 };
    static constexpr double kLfoHz[kLines]  = { 0.11, 0.17, 0.23, 0.29, 0.37, 0.43, 0.53, 0.61 };
    static constexpr double kDiffMs[2][4]   = { { 4.77, 3.59, 12.73, 9.31 }, { 4.93, 3.71, 12.41, 9.67 } };
    static constexpr double kLongMs[2][6]   = { { 43.1, 67.3, 97.7, 31.9, 111.3, 79.1 }, { 47.9, 61.1, 101.9, 29.3, 117.7, 83.3 } };
    static constexpr double kErMs[2][12]    = { { 7.1, 11.3, 17.9, 23.5, 31.1, 37.9, 46.3, 55.1, 67.7, 79.3, 93.1, 109.7 },
                                                { 8.3, 13.7, 15.9, 26.1, 29.3, 41.3, 49.9, 59.3, 63.1, 83.9, 97.3, 113.9 } };
    static constexpr double kErGain[12]     = { 0.95, -0.85, 0.8, 0.7, -0.65, 0.6, -0.5, 0.45, 0.4, -0.3, 0.25, 0.2 };

    double fs = 48000.0, fade = 1.0, fadeStep = 0.001;
    int    current = hall;
    Params params;

    melon::DelayLine pre[2], er[2], echo[2], taps[2];
    Allpass  diff[2][4], longDiff[2][6];
    Allpass1 disp[2][16];
    melon::Biquad formant[2][3], springHp[2], springLp[2];
    melon::OnePole echoLp[2], wetHp[2];
    PitchShifter shifter[2];

    melon::DelayLine lines[kLines];
    melon::OnePole   damp[kLines], lowSplit[kLines];
    double lineDelay[kLines] {}, lineGainHi[kLines] {}, lineGainLo[kLines] {}, lfoPhase[kLines] {};

    double dampMix = 1.0, modDepth = 0.0, diffCoeff = 0.6, erLevel = 0.0, lateLevel = 1.0, erScale = 1.0;
    int    longDiffStages = 0;
    double longDiffCoeff = 0.7, longDiffScale = 1.0, bloomFeedback = 0.0, bloomFb[2] {};
    double springDrive = 1.0, springDisp = 0.5;
    double swellRise = 1.0, swellGain = 0.0, inEnv = 0.0;
    int    swellQuiet = 0;
    double shimmerSemis = 12.0, shimmerAmount = 0.0, shimmerFb[2] {};
    double echoTime = 1000.0, echoRepeats = 0.0, echoFb[2] {}, wow = 0.0;
    double nlWindow = 1000.0, nlShape = 0.5, formantMix = 0.0, wetTrim = 1.0;

    melon::Smoother decay, preDelay, tone, mod, p1, p2, dryGain, wetGain, outGain, freeze, active;
};

} // namespace reverb
