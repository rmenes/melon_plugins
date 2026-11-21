// MultiDelayDsp.h - Melon Multi Delay DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// A stereo multi-machine delay in the spirit of the Strymon TimeLine: twelve delay "machines"
// sharing the same knobs (Time, Repeats, Mix, Filter, Grit, Speed, Depth), plus two
// machine-specific parameters (Param 1 / Param 2), host sync, tap tempo, Infinite (freeze)
// and Trails (repeats keep ringing after bypass).
//
//   in ─► [swell] ─┬────────────────────────────────────────────────────── × dry ──────┐
//                  └─(+)─► delay line ─► read (time + mod) ─► loop colour ─┬─► output   │
//                    ▲                                     (machine, Filter, Grit)   colour ─ × wet ─(+)─► out
//                    └───────────── × Repeats ◄──────────────────────────────┘ (duck, trem, sweep)
//
// "Loop colour" sits inside the feedback loop, so every repeat is darker / dirtier / more
// degraded than the one before (tape, analog bucket-brigade, lo-fi). "Output colour" only
// shapes what you hear (ducking, tremolo, filter sweep). Dual, Pattern, Reverse and Ice have
// their own way of reading the delay lines.
//
// Infinite: the delay lines recirculate their content with a gain of exactly 1 and no new
// input, so the current repeats are held forever. New notes play dry over them.
#pragma once

#include "MelonDsp.h"
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace mdelay
{
using melon::kPi;
using melon::kTwoPi;

enum Machine { digital, tape, analog, dual, pattern, reverse, ice, duck, swell, trem, sweep, lofi, kNumMachines };

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
        { "Digital",  "Smear",       "Low Cut",    "Clean, full-range digital echo. Smear diffuses the repeats" },
        { "Tape",     "Tape Age",    "Flutter",    "Tape echo: warm, saturated repeats with wow and flutter" },
        { "Analog",   "Age",         "Hiss",       "Bucket-brigade echo: repeats get darker as the time gets longer" },
        { "Dual",     "Ratio",       "Routing",    "Two delays: parallel (L/R), series or cross-feedback" },
        { "Pattern",  "Pattern",     "Spread",     "Rhythmic multi-tap patterns spread across the stereo field" },
        { "Reverse",  "Smooth",      "Forward",    "Backwards repeats. Forward blends normal echoes back in" },
        { "Ice",      "Interval",    "Blend",      "Pitch-shifted slices that climb with every repeat" },
        { "Duck",     "Sensitivity", "Release",    "Repeats duck while you play and swell back when you stop" },
        { "Swell",    "Rise",        "Dry Swell",  "Every new note fades in (volume swell) into the echoes" },
        { "Trem",     "Trem Rate",   "Trem Depth", "Tremolo on the repeats" },
        { "Filter",   "Sweep Rate",  "Resonance",  "Resonant low-pass filter sweeping over the repeats" },
        { "Lo-Fi",    "Bits",        "Downsample", "Crushed, band-limited, old-sampler repeats" },
    } };
    return m;
}

constexpr double kMaxTimeMs     = 5000.0;
constexpr double kMaxModMs      = 6.0;
constexpr double kSmoothingMs   = 20.0;
constexpr double kMachineFadeMs = 8.0;
constexpr double kIceSliceMs    = 70.0;

struct Params
{
    int    machine  = digital;
    double timeMs   = 400.0;
    double repeats  = 0.35; // 0..1
    double mix      = 0.35; // 0..1 (50 % = dry and wet both at full level)
    double filter   = 0.8;  // 0 dark .. 1 full range
    double grit     = 0.0;  // 0..1
    double speed    = 0.3;  // 0..1 -> 0.05..10 Hz
    double depth    = 0.1;  // 0..1 -> 0..kMaxModMs
    double param1   = 0.5;
    double param2   = 0.5;
    double outputDb = 0.0;
    bool   infinite = false;
    bool   trails   = false;
    bool   bypassed = false;
};

//==============================================================================
// Mix: up to 50 % the dry stays at full level; above it the dry fades out.
inline void mixGains (double mix, double& dry, double& wet)
{
    mix = std::clamp (mix, 0.0, 1.0);
    dry = std::min (1.0, 2.0 * (1.0 - mix));
    wet = std::min (1.0, 2.0 * mix);
}

inline double speedHz (double speed) { return 0.05 * std::pow (200.0, std::clamp (speed, 0.0, 1.0)); }
inline double filterHz (double filter) { return 800.0 * std::pow (25.0, std::clamp (filter, 0.0, 1.0)); } // 800 Hz .. 20 kHz

// Note values for host sync: { name, length in quarter notes }
inline const std::array<std::pair<const char*, double>, 14>& syncValues()
{
    static const std::array<std::pair<const char*, double>, 14> v { {
        { "Off", 0.0 }, { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/2 D", 3.0 }, { "1/2 T", 4.0 / 3.0 },
        { "1/4", 1.0 }, { "1/4 D", 1.5 }, { "1/4 T", 2.0 / 3.0 }, { "1/8", 0.5 }, { "1/8 D", 0.75 },
        { "1/8 T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/16 D", 0.375 }, { "1/16 T", 1.0 / 6.0 },
    } };
    return v;
}

inline double syncedTimeMs (int syncIndex, double bpm)
{
    const auto& v = syncValues();
    if (syncIndex <= 0 || syncIndex >= (int) v.size() || bpm <= 0.0)
        return 0.0;
    return std::min (kMaxTimeMs, v[(size_t) syncIndex].second * 60000.0 / bpm);
}

//==============================================================================
// Stepped machine parameters
inline int stepIndex (double v, int count) { return std::clamp ((int) (std::clamp (v, 0.0, 1.0) * count), 0, count - 1); }
// Percentage that selects step `index` of `count` (used by the factory presets)
inline float stepPercent (int index, int count) { return (float) ((index + 0.5) * 100.0 / count); }

struct Ratio { const char* name; double value; };
inline const std::array<Ratio, 9>& dualRatios()
{
    static const std::array<Ratio, 9> r { {
        { "1/4", 0.25 }, { "1/3", 1.0 / 3.0 }, { "1/2", 0.5 }, { "2/3", 2.0 / 3.0 }, { "3/4", 0.75 },
        { "1:1", 1.0 }, { "4/3", 4.0 / 3.0 }, { "3/2", 1.5 }, { "2x", 2.0 },
    } };
    return r;
}

enum DualRouting { parallel, series, cross, kNumRoutings };
inline const char* routingName (int r)
{
    static const char* n[] = { "Parallel", "Series", "Cross" };
    return n[std::clamp (r, 0, kNumRoutings - 1)];
}

struct Tap { double pos, level, pan; }; // pos: fraction of Time; pan -1 (left) .. +1 (right)
struct PatternInfo { const char* name; int count; std::array<Tap, 5> taps; };
inline const std::array<PatternInfo, 8>& patterns()
{
    // The last tap of each pattern is at the full Time and feeds the repeats.
    static const std::array<PatternInfo, 8> p { {
        { "Eighths",     2, { { { 0.5, 0.7, -1 }, { 1.0, 1.0, 1 } } } },
        { "Gallop",      2, { { { 0.75, 0.8, -1 }, { 1.0, 1.0, 1 } } } },
        { "Swing",       2, { { { 2.0 / 3.0, 0.75, -1 }, { 1.0, 1.0, 1 } } } },
        { "Triplets",    3, { { { 1.0 / 3.0, 0.6, -1 }, { 2.0 / 3.0, 0.75, 1 }, { 1.0, 1.0, 0 } } } },
        { "Sixteenths",  4, { { { 0.25, 0.5, -1 }, { 0.5, 0.65, 1 }, { 0.75, 0.8, -1 }, { 1.0, 1.0, 1 } } } },
        { "Syncopated",  3, { { { 0.375, 0.65, -1 }, { 0.75, 0.8, 1 }, { 1.0, 1.0, 0 } } } },
        { "Cascade",     4, { { { 0.125, 0.35, -1 }, { 0.25, 0.5, 1 }, { 0.5, 0.7, -1 }, { 1.0, 1.0, 1 } } } },
        { "Quintuplets", 5, { { { 0.2, 0.4, -1 }, { 0.4, 0.55, 1 }, { 0.6, 0.7, -1 }, { 0.8, 0.85, 1 }, { 1.0, 1.0, 0 } } } },
    } };
    return p;
}

struct Interval { const char* name; double semitones; };
inline const std::array<Interval, 7>& iceIntervals()
{
    static const std::array<Interval, 7> i { {
        { "Octave down", -12 }, { "4th down", -5 }, { "4th up", 5 }, { "5th up", 7 },
        { "Octave up", 12 }, { "Octave + 5th", 19 }, { "2 octaves up", 24 },
    } };
    return i;
}

inline double tremRateHz (double v)   { return 0.5 * std::pow (24.0, std::clamp (v, 0.0, 1.0)); }   // 0.5..12 Hz
inline double sweepRateHz (double v)  { return 0.05 * std::pow (160.0, std::clamp (v, 0.0, 1.0)); } // 0.05..8 Hz
inline double swellRiseMs (double v)  { return 20.0 * std::pow (150.0, std::clamp (v, 0.0, 1.0)); } // 20 ms..3 s
inline double duckThreshDb (double v) { return -10.0 - 50.0 * std::clamp (v, 0.0, 1.0); }           // more sensitive = lower
inline double duckReleaseMs (double v){ return 50.0 * std::pow (40.0, std::clamp (v, 0.0, 1.0)); }  // 50 ms..2 s
inline int    lofiBits (double v)     { return 16 - (int) std::lround (12.0 * std::clamp (v, 0.0, 1.0)); } // 16..4 bits
inline int    lofiHold (double v)     { return 1 + (int) std::lround (23.0 * std::clamp (v, 0.0, 1.0)); }  // 1..24
inline double digitalLowCutHz (double v) { return 20.0 * std::pow (40.0, std::clamp (v, 0.0, 1.0)); }  // 20..800 Hz

constexpr double kDuckAmountDb = 24.0;

// Ducking gain (dB) for an input envelope level: a 6 dB soft knee around the threshold
inline double duckDb (double envDb, double threshDb, double amountDb)
{
    const double t = std::clamp ((envDb - threshDb) / 6.0 + 0.5, 0.0, 1.0);
    return -std::abs (amountDb) * t;
}

// Text shown for Param 1 / Param 2 (which = 1 or 2, v = 0..1)
inline std::string paramText (int machine, int which, double v)
{
    char buf[48];
    const int pct = (int) std::lround (v * 100.0);
    auto percent = [&] { std::snprintf (buf, sizeof buf, "%d %%", pct); return std::string (buf); };
    switch (machine)
    {
        case digital:
            if (which == 2) { std::snprintf (buf, sizeof buf, "%d Hz", (int) std::lround (digitalLowCutHz (v))); return buf; }
            return percent();
        case dual:
            return which == 1 ? dualRatios()[(size_t) stepIndex (v, 9)].name : routingName (stepIndex (v, kNumRoutings));
        case pattern:
            return which == 1 ? patterns()[(size_t) stepIndex (v, 8)].name : percent();
        case ice:
            return which == 1 ? iceIntervals()[(size_t) stepIndex (v, 7)].name : percent();
        case duck:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%d dB", (int) std::lround (duckThreshDb (v))); return buf; }
            std::snprintf (buf, sizeof buf, "%d ms", (int) std::lround (duckReleaseMs (v)));
            return buf;
        case swell:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%d ms", (int) std::lround (swellRiseMs (v))); return buf; }
            return percent();
        case trem:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%.1f Hz", tremRateHz (v)); return buf; }
            return percent();
        case sweep:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%.2f Hz", sweepRateHz (v)); return buf; }
            return percent();
        case lofi:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%d bits", lofiBits (v)); return buf; }
            std::snprintf (buf, sizeof buf, "1/%d", lofiHold (v));
            return buf;
        default:
            return percent();
    }
}

//==============================================================================
// Echo sketch for the editor's display: where the repeats land and how loud they are.
struct Echo
{
    double timeMs;
    double amp;
    int    side;     // -1 left, +1 right, 0 both
    bool   reversed;
};

inline std::vector<Echo> echoSketch (const Params& p, double spanMs)
{
    std::vector<Echo> e;
    const double T  = std::clamp (p.timeMs, 1.0, kMaxTimeMs);
    const double fb = p.infinite ? 1.0 : std::clamp (p.repeats, 0.0, 1.0);
    auto add = [&] (double t, double a, int side, bool rev = false) {
        if (t <= spanMs && a >= 0.01 && e.size() < 400) e.push_back ({ t, a, side, rev });
    };
    auto gainAt = [&] (int k) { return std::pow (fb, k); };

    switch (p.machine)
    {
        case dual:
        {
            const double T2 = std::min (2.0 * kMaxTimeMs, T * dualRatios()[(size_t) stepIndex (p.param1, 9)].value);
            const int route = stepIndex (p.param2, kNumRoutings);
            for (int k = 0; k < 64; ++k)
            {
                if (route == parallel)    { add ((k + 1) * T, gainAt (k), -1); add ((k + 1) * T2, gainAt (k), 1); }
                else if (route == series) { add ((k + 1) * T, gainAt (k), -1); add (T + (k + 1) * T2, gainAt (k), 1); }
                else
                {
                    // cross: left (T), then right (T2), then left (T) ...
                    const double t = (k / 2 + 1) * T + ((k + 1) / 2) * T2;
                    add (t, gainAt (k), (k & 1) ? 1 : -1);
                }
            }
            break;
        }
        case pattern:
        {
            const auto&  pat    = patterns()[(size_t) stepIndex (p.param1, 8)];
            const double spread = std::clamp (p.param2, 0.0, 1.0);
            for (int c = 0; c < 64; ++c)
                for (int i = 0; i < pat.count; ++i)
                {
                    const double pan = pat.taps[(size_t) i].pan * spread;
                    add ((c + pat.taps[(size_t) i].pos) * T, pat.taps[(size_t) i].level * gainAt (c), pan < -0.3 ? -1 : pan > 0.3 ? 1 : 0);
                }
            break;
        }
        default:
            for (int k = 0; k < 64; ++k)
                add ((k + 1) * T, gainAt (k), 0, p.machine == reverse);
            break;
    }
    return e;
}

//==============================================================================
// Schroeder allpass (diffuser for Digital's Smear)
class Allpass
{
public:
    void prepare (int len) { line.prepare (len + 2); length = len; }
    void reset() { line.reset(); }
    double process (double x, double g)
    {
        const double d = line.readInt (length - 1);
        const double v = x + g * d;
        line.push ((float) v);
        return d - g * v;
    }

private:
    melon::DelayLine line;
    int length = 1;
};

// TPT state-variable low-pass (Filter machine)
class Svf
{
public:
    void reset() { s1 = s2 = 0.0; }
    double lowpass (double x, double fs, double fc, double q)
    {
        const double g = std::tan (kPi * std::clamp (fc, 20.0, 0.45 * fs) / fs);
        const double k = 1.0 / std::max (0.5, q);
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - s2;
        const double v1 = a1 * s1 + a2 * v3;
        const double v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0 * v1 - s1;
        s2 = 2.0 * v2 - s2;
        return v2;
    }

private:
    double s1 = 0.0, s2 = 0.0;
};

// Two-grain pitch shifter (Ice): grains read a short buffer at `ratio` speed, Hann windows 50 % apart.
class GrainShifter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        buf.prepare ((int) (kIceSliceMs * 0.001 * fs * 4.0) + 16);
        reset();
    }
    void reset() { buf.reset(); phase = 0.0; }
    double process (double x, double ratio)
    {
        buf.push ((float) x);
        const double grain = kIceSliceMs * 0.001 * fs;
        phase += 1.0 / grain;
        if (phase >= 1.0) phase -= 1.0;
        const double span = grain * std::abs (ratio - 1.0);
        double y = 0.0;
        for (int g = 0; g < 2; ++g)
        {
            double ph = phase + 0.5 * g;
            if (ph >= 1.0) ph -= 1.0;
            // up: the read point catches up with the write point; down: it falls behind
            const double d = 2.0 + (ratio > 1.0 ? (1.0 - ph) : ph) * span;
            const double w = std::sin (kPi * ph);
            y += buf.read (d) * w * w;
        }
        return y;
    }

private:
    melon::DelayLine buf;
    double fs = 48000.0, phase = 0.0;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, 2);
        // Dual's second delay and Reverse's read-back both reach 2 x Time
        const int maxSamples = (int) ((2.0 * kMaxTimeMs + 4.0 * kMaxModMs + 50.0) * 0.001 * fs);
        for (auto& l : lines) l.prepare (maxSamples);
        static const int apMs[2][4] = { { 37, 113, 191, 277 }, { 41, 127, 179, 263 } }; // tenths of a ms
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 4; ++i)
                smear[(size_t) c][(size_t) i].prepare (std::max (2, (int) (apMs[c][i] * 0.0001 * fs)));
        for (auto& s : shifters) s.prepare (fs);

        timeS.prepare (fs, 60.0);
        ratioS.prepare (fs, 60.0);
        for (auto* s : smoothers()) s->prepare (fs, kSmoothingMs);
        machineGain.prepare (fs, kMachineFadeMs);
        freeze.prepare (fs, 10.0);
        machineGain.setTarget (1.0);
        machineGain.snap();
        active = params.machine;
        updateCoeffs();
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& c : smear) for (auto& a : c) a.reset();
        for (auto& s : shifters) s.reset();
        for (auto* f : allFilters()) f->reset();
        for (auto& d : dcBlock) d.reset();
        for (auto& s : svf) s.reset();
        lfo = tremPhase = sweepPhase = revPhase = 0.0;
        env = duckNow = 0.0;
        swellFast = swellSlow = 0.0; swellGain = 1.0; swellFalling = false;
        holdCount = 0; held = { 0.0, 0.0 };
        flutterNoise = 0.0; flutterPhase = 0.0;
    }

    void setParams (const Params& p)
    {
        const bool coeffsChanged = p.machine != params.machine || p.param1 != params.param1 || p.param2 != params.param2
                                   || p.filter != params.filter
                                   || std::abs (p.timeMs - params.timeMs) > 0.01 * params.timeMs;
        params = p;
        if (coeffsChanged) updateCoeffs();

        timeS.setTarget (std::clamp (p.timeMs, 1.0, kMaxTimeMs) * 0.001 * fs);
        ratioS.setTarget (dualRatios()[(size_t) stepIndex (p.param1, 9)].value);
        feedback.setTarget (std::clamp (p.repeats, 0.0, 1.0));
        double d, w;
        mixGains (p.mix, d, w);
        dryGain.setTarget (d);
        wetGain.setTarget (w);
        modDepth.setTarget (kMaxModMs * 0.001 * fs * std::clamp (p.depth, 0.0, 1.0));
        outGain.setTarget (melon::dbToGain (p.outputDb));
        bypassFade.setTarget (p.bypassed ? 0.0 : 1.0);
        freeze.setTarget (p.infinite ? 1.0 : 0.0);
    }

    void snapToTargets()
    {
        timeS.snap(); ratioS.snap(); freeze.snap();
        for (auto* s : smoothers()) s->snap();
        if (active != params.machine) { active = params.machine; updateCoeffs(); }
        machineGain.setTarget (1.0);
        machineGain.snap();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int    nch    = std::min (numChannels, channels);
        const double lfoInc = speedHz (params.speed) / fs;
        const double envRel = 1.0 - std::exp (-1.0 / (duckReleaseMs (params.param2) * 0.001 * fs));
        const double duckAtk = 1.0 - std::exp (-1.0 / (0.005 * fs));

        for (int n = 0; n < numSamples; ++n)
        {
            // machine change: fade the wet path out, switch, fade back in
            if (active != params.machine)
            {
                machineGain.setTarget (0.0);
                if (machineGain.value() < 1e-3)
                {
                    active = params.machine;
                    switchMachine();
                    machineGain.setTarget (1.0);
                }
            }
            const double mg = machineGain.next();

            const double T  = timeS.next();
            const double fb = feedback.next(), dg = dryGain.next(), wg = wetGain.next();
            const double md = modDepth.next(), og = outGain.next(), on = bypassFade.next(), fz = freeze.next();
            const double ratio = ratioS.next();

            // modulation of the delay time (Speed / Depth); always >= 0 so the delay never gets shorter than Time
            lfo += lfoInc;
            if (lfo >= 1.0) lfo -= 1.0;
            double shapeL, shapeR;
            if (active == analog)
            {
                shapeL = 2.0 * std::abs (2.0 * lfo - 1.0) - 1.0; // triangle, like an analog LFO
                double l2 = lfo + 0.25; if (l2 >= 1.0) l2 -= 1.0;
                shapeR = 2.0 * std::abs (2.0 * l2 - 1.0) - 1.0;
            }
            else
            {
                shapeL = std::sin (kTwoPi * lfo);
                shapeR = std::cos (kTwoPi * lfo);
            }
            double modL = md * (1.0 + shapeL), modR = md * (1.0 + shapeR);
            if (active == tape)
            {
                // flutter: fast, slightly irregular speed wobble of the tape transport
                flutterPhase += 6.3 / fs;
                if (flutterPhase >= 1.0) flutterPhase -= 1.0;
                flutterNoise += (noise.next() - flutterNoise) * (30.0 / fs);
                const double f = params.param2 * 0.0007 * fs * (1.0 + 0.7 * std::sin (kTwoPi * flutterPhase) + 1.5 * flutterNoise);
                modL += f;
                modR += f;
            }

            const double inL = data[0][n];
            const double inR = nch > 1 ? data[1][n] : inL;

            // Swell: every new note fades in
            double sw = 1.0, dryMul = 1.0;
            if (active == swell)
            {
                sw     = swellStep (std::max (std::abs (inL), std::abs (inR)));
                dryMul = 1.0 - std::clamp (params.param2, 0.0, 1.0) * (1.0 - sw);
            }

            // Trails: when bypassed, stop feeding the delay but let the repeats ring out
            const double send = params.trails ? on : 1.0;
            const double sL = inL * sw * send, sR = inR * sw * send;
            const double mono = 0.5 * (sL + sR);

            double wL = 0.0, wR = 0.0;      // what you hear
            double pushL = 0.0, pushR = 0.0; // what goes into the delay lines
            double T1 = T, T2 = T;

            switch (active)
            {
                case dual:
                {
                    T2 = std::min (T * ratio, 2.0 * kMaxTimeMs * 0.001 * fs);
                    const double a = loop (0, lines[0].read (T + modL));
                    const double b = loop (1, lines[1].read (T2 + modR));
                    switch (stepIndex (params.param2, kNumRoutings))
                    {
                        case parallel: pushL = sL + fb * a;   pushR = sR + fb * b; break;
                        case series:   pushL = mono + fb * a; pushR = a + fb * b;  break;
                        default:       pushL = mono + fb * b; pushR = fb * a;      break; // cross
                    }
                    wL = a; wR = b;
                    break;
                }
                case pattern:
                {
                    const auto&  pat    = patterns()[(size_t) stepIndex (params.param1, 8)];
                    const double spread = std::clamp (params.param2, 0.0, 1.0);
                    double main = 0.0;
                    for (int i = 0; i < pat.count; ++i)
                    {
                        const auto& tap = pat.taps[(size_t) i];
                        const double x  = i == pat.count - 1 ? (main = loop (0, lines[0].read (T + modL)))
                                                             : tone (0, lines[0].read (T * tap.pos + modL));
                        const double angle = (tap.pan * spread + 1.0) * 0.25 * kPi;
                        wL += x * tap.level * std::cos (angle) * 1.41421356; // equal-power pan, unity in the centre
                        wR += x * tap.level * std::sin (angle) * 1.41421356;
                    }
                    pushL = pushR = mono + fb * main; // both lines kept in step
                    break;
                }
                case reverse:
                {
                    // the delay line holds normal (forward) echoes; what you hear is it read backwards
                    const double fL = loop (0, lines[0].read (T + modL));
                    const double fR = loop (1, lines[1].read (T + modR));
                    pushL = sL + fb * fL;
                    pushR = sR + fb * fR;
                    revPhase += 1.0 / std::max (8.0, T);
                    if (revPhase >= 1.0) revPhase -= 1.0;
                    const double rL = reverseRead (lines[0], T), rR = reverseRead (lines[1], T);
                    const double fwd = std::clamp (params.param2, 0.0, 1.0);
                    wL = rL + fwd * (fL - rL);
                    wR = rR + fwd * (fR - rR);
                    break;
                }
                case ice:
                {
                    const double r     = std::pow (2.0, iceIntervals()[(size_t) stepIndex (params.param1, 7)].semitones / 12.0);
                    const double blend = std::clamp (params.param2, 0.0, 1.0);
                    const double yL = loop (0, lines[0].read (T + modL));
                    const double yR = loop (1, lines[1].read (T + modR));
                    const double iL = shifters[0].process (yL, r), iR = shifters[1].process (yR, r);
                    wL = yL + blend * (iL - yL);
                    wR = yR + blend * (iR - yR);
                    pushL = sL + fb * wL; // shifted repeats feed back: each repeat climbs further
                    pushR = sR + fb * wR;
                    break;
                }
                default:
                {
                    const double yL = loop (0, lines[0].read (T + modL));
                    const double yR = loop (1, lines[1].read (T + modR));
                    pushL = sL + fb * yL;
                    pushR = sR + fb * yR;
                    wL = yL; wR = yR;

                    if (active == analog && params.param2 > 0.0)
                    {
                        const double h = 0.004 * params.param2 * params.param2;
                        wL += h * noise.next();
                        wR += h * noise.next();
                    }
                    else if (active == duck)
                    {
                        const double lvl = std::max (std::abs (inL), std::abs (inR));
                        env = lvl > env ? lvl : env + envRel * (lvl - env);
                        const double target = duckDb (melon::gainToDb (env), duckThreshDb (params.param1), kDuckAmountDb);
                        duckNow += (target < duckNow ? duckAtk : envRel) * (target - duckNow);
                        const double g = melon::dbToGain (duckNow);
                        wL *= g; wR *= g;
                    }
                    else if (active == trem)
                    {
                        tremPhase += tremRateHz (params.param1) / fs;
                        if (tremPhase >= 1.0) tremPhase -= 1.0;
                        const double g = 1.0 - std::clamp (params.param2, 0.0, 1.0) * (0.5 + 0.5 * std::cos (kTwoPi * tremPhase));
                        wL *= g; wR *= g;
                    }
                    else if (active == sweep)
                    {
                        sweepPhase += sweepRateHz (params.param1) / fs;
                        if (sweepPhase >= 1.0) sweepPhase -= 1.0;
                        const double fc = 250.0 * std::pow (16.0, 0.5 + 0.5 * std::sin (kTwoPi * sweepPhase)); // 250 Hz..4 kHz
                        const double q  = 0.7 + 7.0 * std::clamp (params.param2, 0.0, 1.0);
                        const double comp = 1.0 / std::sqrt (q / 0.7); // keep the level steady as resonance rises
                        wL = svf[0].lowpass (wL, fs, fc, q) * comp;
                        wR = svf[1].lowpass (wR, fs, fc, q) * comp;
                    }
                    break;
                }
            }

            // Infinite: recirculate what is in the lines (gain exactly 1, no new input)
            if (fz > 0.0)
            {
                const double hL = lines[0].readInt ((int) std::lround (T1));
                const double hR = lines[1].readInt ((int) std::lround (active == dual ? T2 : T1));
                pushL += fz * (hL - pushL);
                pushR += fz * (hR - pushR);
            }
            lines[0].push ((float) limit (pushL));
            lines[1].push ((float) limit (pushR));

            const double wetOut = wg * mg * og;
            if (params.trails)
            {
                // the wet path ignores the bypass crossfade so the repeats ring out
                data[0][n] = (float) (inL + on * (dg * dryMul * og * inL - inL) + wetOut * wL);
                if (nch > 1)
                    data[1][n] = (float) (inR + on * (dg * dryMul * og * inR - inR) + wetOut * wR);
            }
            else
            {
                const double outL = dg * dryMul * og * inL + wetOut * wL;
                const double outR = dg * dryMul * og * inR + wetOut * wR;
                data[0][n] = (float) (inL + on * (outL - inL));
                if (nch > 1)
                    data[1][n] = (float) (inR + on * (outR - inR));
            }
        }
    }

    double currentTimeMs() const { return timeS.value() / fs * 1000.0; }
    int    activeMachine() const { return active; }

private:
    //--------------------------------------------------------------------------
    // Filter knob: low-pass from 800 Hz to 20 kHz (fully open at 100 %)
    double tone (int c, double x)
    {
        return toneOpen ? x : toneLp[(size_t) c].process (x);
    }

    // Everything that colours the repeats inside the feedback loop
    double loop (int c, double x)
    {
        const auto ci = (size_t) c;
        x = dcBlock[ci].highpass (x);
        switch (active)
        {
            case digital:
                if (smearAmt > 0.01)
                    for (auto& ap : smear[ci])
                        x = ap.process (x, 0.65 * smearAmt);
                x = lowCut[ci].process (x);
                break;
            case tape:
                x = headBump[ci].process (x);
                x = ageLp[ci].process (x);
                x = std::tanh (1.4 * x) / 1.4; // gentle tape compression
                break;
            case analog:
                x = ageLp[ci].process (ageLp2[ci].process (x));
                x = std::tanh (1.2 * x) / 1.2;
                break;
            case lofi:
            {
                x = lofiLp[ci].process (lofiHp[ci].process (x));
                const double q = std::ldexp (1.0, lofiBits (params.param1) - 1);
                x = std::round (x * q) / q;
                if (c == 0 && ++holdCount >= lofiHold (params.param2)) holdCount = 0;
                if (holdCount == 0) held[ci] = x;
                x = held[ci];
                break;
            }
            default:
                break;
        }
        x = tone (c, x);
        if (params.grit > 0.001)
        {
            const double g = 1.0 + 9.0 * std::pow (params.grit, 1.5);
            x = std::tanh (g * x) / g * (1.0 + 0.5 * params.grit); // unity for small signals, then clips
        }
        return x;
    }

    // Two backwards-reading grains, each one Time long, 50 % apart. The read point moves away from the
    // write point at twice the sample rate, so the audio plays backwards at its original pitch.
    double reverseRead (const melon::DelayLine& line, double T)
    {
        const double fade = 0.05 + 0.45 * std::clamp (params.param1, 0.0, 1.0); // Smooth: window shape
        double sum = 0.0, wsum = 0.0;
        for (int g = 0; g < 2; ++g)
        {
            double ph = revPhase + 0.5 * g;
            if (ph >= 1.0) ph -= 1.0;
            const double w = ph < fade ? 0.5 - 0.5 * std::cos (kPi * ph / fade)
                           : ph > 1.0 - fade ? 0.5 - 0.5 * std::cos (kPi * (1.0 - ph) / fade) : 1.0;
            sum  += w * line.read (1.0 + 2.0 * ph * T);
            wsum += w;
        }
        return wsum > 1e-6 ? sum / wsum : 0.0;
    }

    double swellStep (double lvl)
    {
        const double fastA = 1.0 - std::exp (-1.0 / (0.001 * fs)), fastR = 1.0 - std::exp (-1.0 / (0.03 * fs));
        const double slowC = 1.0 - std::exp (-1.0 / (0.15 * fs));
        swellFast += (lvl > swellFast ? fastA : fastR) * (lvl - swellFast);
        swellSlow += slowC * (swellFast - swellSlow);
        // a new pick attack (fast envelope jumps well above the slow one): drop the gain and rise again
        if (swellFast > 0.003 && swellFast > 2.5 * swellSlow + 1e-4 && swellGain > 0.3)
            swellFalling = true;
        if (swellFalling)
        {
            swellGain -= 1.0 / (0.004 * fs);
            if (swellGain <= 0.0) { swellGain = 0.0; swellFalling = false; }
        }
        else
            swellGain = std::min (1.0, swellGain + 1.0 / (swellRiseMs (params.param1) * 0.001 * fs));
        const double s = swellGain;
        return s * s * (3.0 - 2.0 * s); // smooth S-curve
    }

    void switchMachine()
    {
        timeS.prepare (fs, (active == tape || active == analog) ? 250.0 : 60.0); // tape and analog glide (pitch bend)
        for (auto& s : shifters) s.reset();
        for (auto& c : smear) for (auto& a : c) a.reset();
        for (auto* f : allFilters()) f->reset();
        for (auto& s : svf) s.reset();
        env = duckNow = 0.0;
        swellGain = 1.0; swellFalling = false;
        updateCoeffs();
    }

    void updateCoeffs()
    {
        using T = melon::Biquad::Type;
        const double fc = filterHz (params.filter);
        toneOpen = fc >= 19500.0;
        for (auto& f : toneLp) f.set (T::lowpass, fs, fc, 0.707);
        for (auto& d : dcBlock) d.setCutoff (fs, 25.0);

        const int m = params.machine;
        smearAmt = m == digital ? std::clamp (params.param1, 0.0, 1.0) : 0.0;
        for (auto& f : lowCut) f.set (T::highpass, fs, digitalLowCutHz (params.param2), 0.707);
        for (auto& f : headBump) f.set (T::peak, fs, 95.0, 0.9, 2.5);
        for (auto& f : lofiHp) f.set (T::highpass, fs, 350.0, 0.707);
        for (auto& f : lofiLp) f.set (T::lowpass, fs, 3400.0, 0.9);

        if (m == tape)
        {
            const double cut = 12000.0 * std::pow (2500.0 / 12000.0, std::clamp (params.param1, 0.0, 1.0));
            for (auto& f : ageLp) f.set (T::lowpass, fs, cut, 0.6);
        }
        else if (m == analog)
        {
            // bucket-brigade: a longer delay needs a slower clock, so the bandwidth falls with Time
            const double base = 6000.0 * std::pow (300.0 / std::clamp (params.timeMs, 20.0, kMaxTimeMs), 0.5);
            const double cut  = std::clamp (base * (1.0 - 0.6 * std::clamp (params.param1, 0.0, 1.0)), 700.0, 12000.0);
            for (auto& f : ageLp)  f.set (T::lowpass, fs, cut, 0.54);
            for (auto& f : ageLp2) f.set (T::lowpass, fs, cut, 1.31);
        }
    }

    // soft safety limit: transparent below 1.0, keeps 100 % repeats from running away
    static double limit (double v) { return std::abs (v) <= 1.0 ? v : std::copysign (1.0 + std::tanh (std::abs (v) - 1.0), v); }

    std::vector<melon::Smoother*> smoothers() { return { &feedback, &dryGain, &wetGain, &modDepth, &outGain, &bypassFade }; }
    std::vector<melon::Biquad*> allFilters()
    {
        std::vector<melon::Biquad*> v;
        for (auto* arr : { &toneLp, &lowCut, &headBump, &ageLp, &ageLp2, &lofiHp, &lofiLp })
            for (auto& f : *arr) v.push_back (&f);
        return v;
    }

    double fs = 48000.0;
    int    channels = 2, active = digital;
    Params params;

    std::array<melon::DelayLine, 2> lines;
    std::array<std::array<Allpass, 4>, 2> smear;
    std::array<GrainShifter, 2> shifters;
    std::array<melon::Biquad, 2> toneLp, lowCut, headBump, ageLp, ageLp2, lofiHp, lofiLp;
    std::array<melon::OnePole, 2> dcBlock;
    std::array<Svf, 2> svf;
    melon::Smoother timeS, ratioS, feedback, dryGain, wetGain, modDepth, outGain, bypassFade, machineGain, freeze;
    melon::Noise noise;

    bool   toneOpen = true;
    double smearAmt = 0.0;
    double lfo = 0.0, tremPhase = 0.0, sweepPhase = 0.0, revPhase = 0.0;
    double env = 0.0, duckNow = 0.0;
    double swellFast = 0.0, swellSlow = 0.0, swellGain = 1.0;
    bool   swellFalling = false;
    int    holdCount = 0;
    std::array<double, 2> held { 0.0, 0.0 };
    double flutterNoise = 0.0, flutterPhase = 0.0;
};

} // namespace mdelay
