// MultiModDsp.h - Melon MultiMod DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// A stereo multi-machine modulation effect in the spirit of the Strymon Mobius: twelve
// modulation "machines" sharing the same knobs (Speed, Depth, Mix, Width), plus two
// machine-specific parameters (Param 1 / Param 2), an LFO waveform choice, host sync and
// tap tempo.
//
//   in ──┬───────────────────────────────────────────── × dry ─┐
//        └─► machine (LFO / envelope driven) ─► × wet ────────(+)─► × output ─► out
//                 ▲
//          LFO: Speed (or Sync), Wave, Width = phase offset of the right channel
//
// The LFO keeps a running position in cycles (not wrapped to one cycle), so the random
// wave and the step patterns of Pattern Trem stay in place. With Sync on and the host
// playing, the position is locked to the host's beat.
#pragma once

#include "MelonDsp.h"
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace mmod
{
using melon::kPi;
using melon::kTwoPi;

enum Machine { chorus, flanger, rotary, vibe, phaser, filter, formant, vintageTrem, patternTrem, autoswell, destroyer, quadrature, kNumMachines };

struct MachineInfo
{
    const char* name;
    const char* param1;
    const char* param2;
    const char* description;
    bool usesSpeed, usesDepth, usesWave;
};

inline const std::array<MachineInfo, kNumMachines>& machines()
{
    static const std::array<MachineInfo, kNumMachines> m { {
        { "Chorus",       "Mode",        "Tone",       "Classic, dimension or three-voice chorus",                     true,  true,  true  },
        { "Flanger",      "Regen",       "Manual",     "Swept short delay with positive or negative feedback",         true,  true,  true  },
        { "Rotary",       "Ramp",        "Horn Mix",   "Rotating horn and drum speaker. Speed changes glide",          true,  true,  false },
        { "Vibe",         "Bias",        "Mode",       "Photocell four-stage phaser with an uneven, throbbing sweep",  true,  true,  true  },
        { "Phaser",       "Stages",      "Regen",      "Classic swept phaser, 2 to 16 stages",                         true,  true,  true  },
        { "Filter",       "Resonance",   "Type",       "Resonant filter swept by the LFO or by your playing",          true,  true,  true  },
        { "Formant",      "Vowels",      "Resonance",  "Talking filter that morphs between two vowels",                true,  true,  true  },
        { "Vintage Trem", "Type",        "Shape",      "Amp tremolo: tube bias, harmonic (split band) or photocell",   true,  true,  true  },
        { "Pattern Trem", "Pattern",     "Smooth",     "Rhythmic step tremolo. Speed / Sync set the length of a step", true,  true,  false },
        { "Autoswell",    "Rise",        "Sensitivity","Every new note fades in like a volume pedal",                  false, true,  false },
        { "Destroyer",    "Bits",        "Downsample", "Bit crusher and sample-rate reducer, swept by the LFO",        true,  true,  true  },
        { "Quadrature",   "Mode",        "Range",      "Ring / amplitude modulation, FM and frequency shifting",       true,  true,  false },
    } };
    return m;
}

enum Wave { sine, triangle, square, rampUp, rampDown, randomWave, kNumWaves };
inline const char* waveName (int w)
{
    static const char* n[] = { "Sine", "Triangle", "Square", "Ramp Up", "Ramp Down", "Random" };
    return n[std::clamp (w, 0, kNumWaves - 1)];
}

constexpr double kMinSpeedHz    = 0.05;
constexpr double kMaxSpeedHz    = 15.0;
constexpr double kPositionWrap  = 48.0; // LFO position wraps here (a multiple of every pattern length)
constexpr double kSmoothingMs   = 20.0;
constexpr double kMachineFadeMs = 6.0;
constexpr double kMaxDelayMs    = 60.0;

struct Params
{
    int    machine  = chorus;
    double speedHz  = 1.0;
    int    wave     = sine;
    double depth    = 0.5; // 0..1
    double mix      = 0.5; // 0..1 (50 % = dry and effect in equal parts)
    double width    = 0.5; // 0..1: right-channel LFO offset (0 .. half a cycle; one step in Pattern Trem)
    double param1   = 0.5;
    double param2   = 0.5;
    double outputDb = 0.0;
    bool   bypassed = false;
};

//==============================================================================
// Mix: below 50 % the dry stays at full level, above it the dry fades out; the sum is
// normalised (by power) so the level stays about the same at 50 % (dry and effect in equal parts).
inline void mixGains (double mix, double& dry, double& wet)
{
    mix = std::clamp (mix, 0.0, 1.0);
    const double d = std::min (1.0, 2.0 * (1.0 - mix)), w = std::min (1.0, 2.0 * mix);
    const double norm = std::max (1.0, std::sqrt (d * d + w * w));
    dry = d / norm;
    wet = w / norm;
}

// Note values for host sync: { name, LFO cycle (or Pattern Trem step) length in quarter notes }
inline const std::array<std::pair<const char*, double>, 16>& syncValues()
{
    static const std::array<std::pair<const char*, double>, 16> v { {
        { "Off", 0.0 }, { "4 bars", 16.0 }, { "2 bars", 8.0 }, { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/2 D", 3.0 },
        { "1/2 T", 4.0 / 3.0 }, { "1/4", 1.0 }, { "1/4 D", 1.5 }, { "1/4 T", 2.0 / 3.0 }, { "1/8", 0.5 },
        { "1/8 D", 0.75 }, { "1/8 T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/16 T", 1.0 / 6.0 }, { "1/32", 0.125 },
    } };
    return v;
}

inline double syncLengthQuarters (int syncIndex)
{
    const auto& v = syncValues();
    return (syncIndex <= 0 || syncIndex >= (int) v.size()) ? 0.0 : v[(size_t) syncIndex].second;
}

// LFO rate for a sync value at `bpm` (0 = not synced)
inline double syncedHz (int syncIndex, double bpm)
{
    const double q = syncLengthQuarters (syncIndex);
    if (q <= 0.0 || bpm <= 0.0)
        return 0.0;
    return std::clamp (bpm / 60.0 / q, 0.005, 40.0);
}

//==============================================================================
// Stepped machine parameters
inline int stepIndex (double v, int count) { return std::clamp ((int) (std::clamp (v, 0.0, 1.0) * count), 0, count - 1); }
// Percentage that selects step `index` of `count` (used by the factory presets)
inline float stepPercent (int index, int count) { return (float) ((index + 0.5) * 100.0 / count); }

enum ChorusMode { classicChorus, dimensionChorus, multiChorus, kNumChorusModes };
enum VibeMode { vibeChorus, vibeVibrato, kNumVibeModes };
enum FilterType { lowPass, bandPass, highPass, envUp, envDown, kNumFilterTypes };
enum TremType { tubeTrem, harmonicTrem, photocellTrem, kNumTremTypes };
enum QuadMode { quadAM, quadFM, shiftUp, shiftDown, kNumQuadModes };

inline const char* chorusModeName (int i) { static const char* n[] = { "Classic", "Dimension", "Multi" }; return n[std::clamp (i, 0, 2)]; }
inline const char* vibeModeName (int i)   { static const char* n[] = { "Chorus", "Vibrato" }; return n[std::clamp (i, 0, 1)]; }
inline const char* filterTypeName (int i) { static const char* n[] = { "Low Pass", "Band Pass", "High Pass", "Env Up", "Env Down" }; return n[std::clamp (i, 0, 4)]; }
inline const char* tremTypeName (int i)   { static const char* n[] = { "Tube", "Harmonic", "Photocell" }; return n[std::clamp (i, 0, 2)]; }
inline const char* quadModeName (int i)   { static const char* n[] = { "AM", "FM", "Shift Up", "Shift Down" }; return n[std::clamp (i, 0, 3)]; }
inline const char* quadRangeName (int i)  { static const char* n[] = { "x1", "x10", "x100" }; return n[std::clamp (i, 0, 2)]; }
inline double      quadRangeMul (int i)   { static const double m[] = { 1.0, 10.0, 100.0 }; return m[std::clamp (i, 0, 2)]; }

inline const std::array<int, 6>& phaserStages() { static const std::array<int, 6> s { { 2, 4, 6, 8, 12, 16 } }; return s; }

struct Vowel { const char* name; double f[3]; double gainDb[3]; };
inline const std::array<Vowel, 5>& vowels()
{
    static const std::array<Vowel, 5> v { {
        { "A", { 730, 1090, 2440 }, { 0, -6, -15 } },
        { "E", { 530, 1840, 2480 }, { 0, -10, -14 } },
        { "I", { 300, 2290, 3010 }, { 0, -14, -12 } },
        { "O", { 570, 840, 2410 },  { 0, -4, -20 } },
        { "U", { 320, 870, 2240 },  { 0, -12, -24 } },
    } };
    return v;
}
struct VowelPair { const char* name; int a, b; };
inline const std::array<VowelPair, 8>& vowelPairs()
{
    static const std::array<VowelPair, 8> p { {
        { "A - E", 0, 1 }, { "A - I", 0, 2 }, { "A - O", 0, 3 }, { "A - U", 0, 4 },
        { "E - I", 1, 2 }, { "E - O", 1, 3 }, { "O - U", 3, 4 }, { "I - U", 2, 4 },
    } };
    return p;
}

struct StepPattern { const char* name; int count; std::array<double, 16> steps; };
inline const std::array<StepPattern, 8>& stepPatterns()
{
    static const std::array<StepPattern, 8> p { {
        { "Pulse",          8, { { 1, 0, 1, 0, 1, 0, 1, 0 } } },
        { "Gallop",         8, { { 1, 0, 1, 1, 1, 0, 1, 1 } } },
        { "Reverse Gallop", 8, { { 1, 1, 1, 0, 1, 1, 1, 0 } } },
        { "Syncopated",     8, { { 1, 0, 0, 1, 0, 0, 1, 0 } } },
        { "Triplets",       6, { { 1, 0.35, 0.35, 1, 0.35, 0.35 } } },
        { "Accents",        8, { { 1, 0.3, 0.55, 0.3, 0.8, 0.3, 0.55, 0.3 } } },
        { "Ramp Up",        8, { { 0.12, 0.25, 0.38, 0.5, 0.62, 0.75, 0.88, 1 } } },
        { "Stutter",       16, { { 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0 } } },
    } };
    return p;
}

// Parameter mappings (v = 0..1)
inline double chorusToneHz (double v)     { return 1500.0 * std::pow (13.333, std::clamp (v, 0.0, 1.0)); } // 1.5..20 kHz
inline double flangerRegen (double v)     { return (std::clamp (v, 0.0, 1.0) * 2.0 - 1.0) * 0.95; }        // -95..+95 %
inline double flangerManualMs (double v)  { return 8.0 * std::pow (0.025, std::clamp (v, 0.0, 1.0)); }     // 8 .. 0.2 ms
inline double rotaryRampS (double v)      { return 0.2 * std::pow (25.0, std::clamp (v, 0.0, 1.0)); }      // 0.2..5 s
inline double phaserRegen (double v)      { return 0.9 * std::clamp (v, 0.0, 1.0); }
inline double filterQ (double v)          { return 0.6 * std::pow (24.0, std::clamp (v, 0.0, 1.0)); }      // 0.6..14.4
inline double formantQ (double v)         { return 4.0 * std::pow (5.0, std::clamp (v, 0.0, 1.0)); }       // 4..20
inline double swellRiseMs (double v)      { return 20.0 * std::pow (150.0, std::clamp (v, 0.0, 1.0)); }    // 20 ms..3 s
inline double swellThreshDb (double v)    { return -20.0 - 45.0 * std::clamp (v, 0.0, 1.0); }              // more sensitive = lower
inline int    destroyerBits (double v)    { return 16 - (int) std::lround (13.0 * std::clamp (v, 0.0, 1.0)); } // 16..3 bits
constexpr double kDestroyerDrive = 3.0; // the crusher sees a hotter signal, so guitar-level input keeps its bits
inline double destroyerHold (double v)    { return std::pow (32.0, std::clamp (v, 0.0, 1.0)); }            // 1..32 samples

// Text shown for Param 1 / Param 2 (which = 1 or 2, v = 0..1)
inline std::string paramText (int machine, int which, double v)
{
    char buf[48];
    const int pct = (int) std::lround (v * 100.0);
    auto percent = [&] { std::snprintf (buf, sizeof buf, "%d %%", pct); return std::string (buf); };
    switch (machine)
    {
        case chorus:
            if (which == 1) return chorusModeName (stepIndex (v, kNumChorusModes));
            std::snprintf (buf, sizeof buf, "%.1f kHz", chorusToneHz (v) / 1000.0); return buf;
        case flanger:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%+d %%", (int) std::lround (flangerRegen (v) * 100.0)); return buf; }
            std::snprintf (buf, sizeof buf, "%.2f ms", flangerManualMs (v)); return buf;
        case rotary:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%.1f s", rotaryRampS (v)); return buf; }
            return percent();
        case vibe:
            return which == 1 ? percent() : vibeModeName (stepIndex (v, kNumVibeModes));
        case phaser:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%d", phaserStages()[(size_t) stepIndex (v, 6)]); return buf; }
            return percent();
        case filter:
            return which == 1 ? percent() : filterTypeName (stepIndex (v, kNumFilterTypes));
        case formant:
            return which == 1 ? vowelPairs()[(size_t) stepIndex (v, 8)].name : percent();
        case vintageTrem:
            return which == 1 ? tremTypeName (stepIndex (v, kNumTremTypes)) : percent();
        case patternTrem:
            return which == 1 ? stepPatterns()[(size_t) stepIndex (v, 8)].name : percent();
        case autoswell:
        {
            if (which == 1)
            {
                const double ms = swellRiseMs (v);
                if (ms >= 1000.0) std::snprintf (buf, sizeof buf, "%.2f s", ms / 1000.0);
                else              std::snprintf (buf, sizeof buf, "%d ms", (int) std::lround (ms));
                return buf;
            }
            std::snprintf (buf, sizeof buf, "%d dB", (int) std::lround (swellThreshDb (v))); return buf;
        }
        case destroyer:
            if (which == 1) { std::snprintf (buf, sizeof buf, "%d bits", destroyerBits (v)); return buf; }
            std::snprintf (buf, sizeof buf, "1/%.1f", destroyerHold (v)); return buf;
        case quadrature:
            return which == 1 ? quadModeName (stepIndex (v, kNumQuadModes)) : quadRangeName (stepIndex (v, 3));
        default: return percent();
    }
}

//==============================================================================
// LFO wave at running position `pos` (in cycles), bipolar -1..1. Edges of the square and
// ramps are slightly rounded so they don't click.
inline double randomTable (int k)
{
    uint32_t s = (uint32_t) (k % (int) kPositionWrap) * 2654435761u + 0x9e3779b9u;
    s ^= s >> 15; s *= 0x2c1b3c6du; s ^= s >> 12;
    return (double) (s & 0xffff) / 32767.5 - 1.0;
}

inline double waveValue (int wave, double pos)
{
    const double ph = pos - std::floor (pos);
    switch (wave)
    {
        case triangle: return ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph;
        case square:   return std::tanh (6.0 * std::sin (kTwoPi * ph)) / std::tanh (6.0);
        case rampUp:   return ph < 0.96 ? -1.0 + 2.0 * ph / 0.96 : 1.0 - 2.0 * (ph - 0.96) / 0.04;
        case rampDown: return ph < 0.96 ? 1.0 - 2.0 * ph / 0.96 : -1.0 + 2.0 * (ph - 0.96) / 0.04;
        case randomWave:
        {
            const int    k = (int) std::floor (pos);
            const double t = 0.5 - 0.5 * std::cos (kPi * ph);
            return randomTable (k) + (randomTable (k + 1) - randomTable (k)) * t;
        }
        default: return std::sin (kTwoPi * ph);
    }
}

inline double wrapPosition (double pos)
{
    pos = std::fmod (pos, kPositionWrap);
    return pos < 0.0 ? pos + kPositionWrap : pos;
}

// Step level of Pattern Trem at running position `pos` (in steps)
inline double patternLevel (int pattern, double pos)
{
    const auto& p = stepPatterns()[(size_t) std::clamp (pattern, 0, 7)];
    const int   s = (int) std::floor (wrapPosition (pos)) % p.count;
    return p.steps[(size_t) s];
}

// Sharpened LFO for Vintage Trem's Shape (0 = as is, 1 = almost square)
inline double shapeLfo (double l, double shape)
{
    const double k = 1.0 + 8.0 * std::clamp (shape, 0.0, 1.0);
    return std::tanh (l * k) / std::tanh (k);
}

// What the display draws for one channel at `pos`: 0..1 (gain for the trems, sweep position otherwise)
inline double sketchValue (const Params& p, double pos)
{
    const double d = std::clamp (p.depth, 0.0, 1.0);
    switch (p.machine)
    {
        case patternTrem: return 1.0 - d * (1.0 - patternLevel (stepIndex (p.param1, 8), pos));
        case vintageTrem: return 1.0 - d * (0.5 - 0.5 * shapeLfo (waveValue (p.wave, pos), p.param2));
        case rotary:
        case quadrature:  return 0.5 + 0.5 * d * waveValue (sine, pos);
        default:          return 0.5 + 0.5 * d * waveValue (p.wave, pos);
    }
}

//==============================================================================
// Building blocks
struct Svf // TPT state-variable filter
{
    double ic1 = 0.0, ic2 = 0.0;
    void reset() { ic1 = ic2 = 0.0; }
    void process (double x, double g, double k, double& lp, double& bp, double& hp)
    {
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

struct Allpass1 // first-order TPT allpass, g = tan (pi f / fs)
{
    double s = 0.0;
    void   reset() { s = 0.0; }
    double process (double x, double g)
    {
        const double v  = g / (1.0 + g) * (x - s);
        const double lp = v + s;
        s = lp + v;
        return 2.0 * lp - x;
    }
};

// 90-degree phase-difference network (two allpass chains, Olli Niemitalo's coefficients).
struct Hilbert
{
    void reset() { *this = Hilbert(); }
    void process (double x, double& i, double& q)
    {
        static constexpr double ca[4] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
        static constexpr double cb[4] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
        double a = x, b = x;
        for (int k = 0; k < 4; ++k)
        {
            const double ya = ca[k] * ca[k] * (a + ya2[k]) - xa2[k];
            xa2[k] = xa1[k]; xa1[k] = a; ya2[k] = ya1[k]; ya1[k] = ya; a = ya;
            const double yb = cb[k] * cb[k] * (b + yb2[k]) - xb2[k];
            xb2[k] = xb1[k]; xb1[k] = b; yb2[k] = yb1[k]; yb1[k] = yb; b = yb;
        }
        i = aDelay; aDelay = a;
        q = b;
    }

private:
    double xa1[4] {}, xa2[4] {}, ya1[4] {}, ya2[4] {};
    double xb1[4] {}, xb2[4] {}, yb1[4] {}, yb2[4] {};
    double aDelay = 0.0;
};

// Envelope follower (peak, attack / release in ms)
struct Envelope
{
    void prepare (double fs, double attackMs, double releaseMs)
    {
        att = 1.0 - std::exp (-1.0 / std::max (1.0, attackMs * 0.001 * fs));
        rel = 1.0 - std::exp (-1.0 / std::max (1.0, releaseMs * 0.001 * fs));
    }
    double process (double x)
    {
        x = std::abs (x);
        env += (x > env ? att : rel) * (x - env);
        return env;
    }
    double env = 0.0, att = 1.0, rel = 1.0;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, 2);
        const int maxDelay = (int) std::ceil (kMaxDelayMs * 0.001 * fs) + 8;
        for (auto& c : ch)
            c.delay.prepare (maxDelay);
        hornLine.prepare (maxDelay);
        drumLine.prepare (maxDelay);

        for (auto* s : { &depthS, &dryS, &wetS, &widthS, &p1S, &p2S, &outS })
            s->prepare (fs, kSmoothingMs);
        speedS.prepare (fs, 50.0);
        bypassS.prepare (fs, 10.0);
        machineS.prepare (fs, kMachineFadeMs);

        // Rotary crossover (Linkwitz-Riley 4th order at 800 Hz)
        for (int i = 0; i < 2; ++i)
        {
            xoverLow[i].set (melon::Biquad::Type::lowpass, fs, 800.0, 0.70710678);
            xoverHigh[i].set (melon::Biquad::Type::highpass, fs, 800.0, 0.70710678);
        }
        for (auto& c : ch)
        {
            c.harmonicSplit.setCutoff (fs, 650.0);
            c.destroyerSmooth.setCutoff (fs, 12000.0);
        }
        swellFast.prepare (fs, 0.5, 20.0);
        swellSlow.prepare (fs, 30.0, 200.0);
        gainSlew.prepare (fs, 1.0);
        active = params.machine;
        resetMachines();
    }

    void setParams (const Params& p)
    {
        params = p;
        double dry, wet;
        mixGains (p.mix, dry, wet);
        speedS.setTarget (std::clamp (p.speedHz, 0.001, 40.0));
        depthS.setTarget (std::clamp (p.depth, 0.0, 1.0));
        dryS.setTarget (dry);
        wetS.setTarget (wet);
        widthS.setTarget (std::clamp (p.width, 0.0, 1.0));
        p1S.setTarget (std::clamp (p.param1, 0.0, 1.0));
        p2S.setTarget (std::clamp (p.param2, 0.0, 1.0));
        outS.setTarget (melon::dbToGain (p.outputDb));
        bypassS.setTarget (p.bypassed ? 0.0 : 1.0);
        machineS.setTarget (p.machine == active ? 1.0 : 0.0);
    }

    void snapToTargets()
    {
        for (auto* s : { &speedS, &depthS, &dryS, &wetS, &widthS, &p1S, &p2S, &outS, &bypassS })
            s->snap();
        if (params.machine != active)
        {
            active = params.machine;
            resetMachines();
        }
        machineS.setTarget (1.0);
        machineS.snap();
        hornHz = drumHz = params.speedHz;
    }

    // Locks the LFO to the host: `cycles` = host position in LFO cycles (Pattern Trem: steps)
    void setHostPosition (double cycles) { position = wrapPosition (cycles); }
    double getPosition() const { return position; }

    void process (float* const* data, int numChannels, int numSamples)
    {
        numChannels = std::min (numChannels, 2);
        for (int n = 0; n < numSamples; ++n)
        {
            // machine change: fade the effect out, switch, fade back in
            if (params.machine != active && machineS.value() < 0.001)
            {
                active = params.machine;
                resetMachines();
                machineS.setTarget (1.0);
            }

            const double speed = speedS.next(), depth = depthS.next(), dry = dryS.next(), wet = wetS.next();
            const double width = widthS.next(), p1 = p1S.next(), p2 = p2S.next(), og = outS.next();
            const double on = bypassS.next(), mg = machineS.next();

            position = wrapPosition (position + speed / fs);

            double in[2], out[2];
            in[0] = data[0][n];
            in[1] = numChannels > 1 ? data[1][n] : in[0];

            runMachine (in, out, speed, depth, width, p1, p2);

            for (int c = 0; c < numChannels; ++c)
            {
                const double effect    = dry * in[c] + wet * out[c];
                const double processed = in[c] + mg * (effect - in[c]);
                double y = in[c] + on * (processed * og - in[c]);
                if (params.bypassed && on < 1.0e-5)
                    y = in[c];
                data[c][n] = (float) y;
            }
        }
    }

private:
    struct Channel
    {
        melon::DelayLine delay;
        melon::OnePole   tone, harmonicSplit, destroyerSmooth;
        Allpass1         ap[16];
        Svf              svf[3];
        Hilbert          hilbert;
        double           feedback = 0.0, lamp = 0.0, held = 0.0, holdPhase = 1.0, oscPhase = 0.0, gain = 1.0;
    };

    void resetMachines()
    {
        for (auto& c : ch)
        {
            c.delay.reset();
            c.tone.reset();
            c.harmonicSplit.reset();
            c.destroyerSmooth.reset();
            for (auto& a : c.ap) a.reset();
            for (auto& s : c.svf) s.reset();
            c.hilbert.reset();
            c.feedback = c.lamp = c.held = c.oscPhase = 0.0;
            c.holdPhase = 1.0;
            c.gain = 1.0;
        }
        hornLine.reset();
        drumLine.reset();
        for (auto& b : xoverLow) b.reset();
        for (auto& b : xoverHigh) b.reset();
        filterEnv.env = 0.0;
        swellFast.env = swellSlow.env = 0.0;
        swellPos = 1.0e9;
        sinceTrigger = 1.0e9;
        gainSlew.snap();
    }

    // LFO position of channel c (the right one is offset by Width)
    double chanPos (int c, double width) const
    {
        if (c == 0) return position;
        const double offset = active == patternTrem ? width : 0.5 * width;
        return position - offset;
    }

    double msToSamples (double ms) const { return ms * 0.001 * fs; }
    double tanG (double f) const { return std::tan (kPi * std::clamp (f, 10.0, 0.45 * fs) / fs); }

    void runMachine (const double* in, double* out, double speed, double depth, double width, double p1, double p2)
    {
        switch (active)
        {
            case chorus:      return runChorus (in, out, depth, width, p1, p2);
            case flanger:     return runFlanger (in, out, depth, width, p1, p2);
            case rotary:      return runRotary (in, out, speed, depth, width, p1, p2);
            case vibe:        return runVibe (in, out, depth, width, p1, p2);
            case phaser:      return runPhaser (in, out, depth, width, p1, p2);
            case filter:      return runFilter (in, out, speed, depth, width, p1, p2);
            case formant:     return runFormant (in, out, depth, width, p1, p2);
            case vintageTrem: return runVintageTrem (in, out, depth, width, p1, p2);
            case patternTrem: return runPatternTrem (in, out, speed, depth, width, p1, p2);
            case autoswell:   return runAutoswell (in, out, depth, p1, p2);
            case destroyer:   return runDestroyer (in, out, depth, width, p1, p2);
            case quadrature:  return runQuadrature (in, out, speed, depth, width, p1, p2);
            default:          out[0] = in[0]; out[1] = in[1];
        }
    }

    double lfo (int c, double width) const { return waveValue (params.wave, chanPos (c, width)); }

    //--------------------------------------------------------------------------
    void runChorus (const double* in, double* out, double depth, double width, double p1, double p2)
    {
        const int    mode = stepIndex (p1, kNumChorusModes);
        const double tone = chorusToneHz (p2);
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            s.delay.push ((float) in[c]);
            const double pos = chanPos (c, width);
            double y = 0.0;
            if (mode == classicChorus)
            {
                y = s.delay.read (msToSamples (7.0 + 4.0 * depth * waveValue (params.wave, pos)));
            }
            else if (mode == dimensionChorus)
            {
                const double l = waveValue (params.wave, pos);
                y = 0.7071 * (s.delay.read (msToSamples (11.0 + 2.5 * depth * l))
                           + s.delay.read (msToSamples (11.0 - 2.5 * depth * l)));
            }
            else
            {
                for (int v = 0; v < 3; ++v)
                    y += s.delay.read (msToSamples (8.0 + 2.0 * v + 3.5 * depth * waveValue (params.wave, pos + v / 3.0)));
                y *= 0.58;
            }
            s.tone.setCutoff (fs, tone);
            out[c] = s.tone.lowpass (y);
        }
    }

    void runFlanger (const double* in, double* out, double depth, double width, double p1, double p2)
    {
        const double fb = flangerRegen (p1), manual = flangerManualMs (p2);
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            const double u  = 0.5 + 0.5 * lfo (c, width);
            const double ms = std::min (15.0, manual * std::pow (2.0, depth * 4.0 * u));
            const double y  = s.delay.read (std::max (1.0, msToSamples (ms) - 1.0));
            s.delay.push ((float) std::clamp (in[c] + fb * y, -4.0, 4.0));
            out[c] = y * (1.0 - 0.6 * std::abs (fb)); // keep the level steady at high Regen
        }
    }

    void runRotary (const double* in, double* out, double speed, double depth, double width, double p1, double p2)
    {
        // rotors accelerate / brake towards the Speed setting; the heavy drum is slower
        const double ramp = rotaryRampS (p1);
        const double hc = 1.0 - std::exp (-1.0 / (ramp * 0.5 * fs)), dc = 1.0 - std::exp (-1.0 / (ramp * 1.5 * fs));
        hornHz += hc * (speed - hornHz);
        drumHz += dc * (speed * 0.86 - drumHz);
        hornPhase = std::fmod (hornPhase + hornHz / fs, 1.0);
        drumPhase = std::fmod (drumPhase + drumHz / fs, 1.0);

        double m = channels > 1 ? 0.5 * (in[0] + in[1]) : in[0];
        double lo = xoverLow[1].process (xoverLow[0].process (m));
        double hi = xoverHigh[1].process (xoverHigh[0].process (m));
        hornLine.push ((float) hi);
        drumLine.push ((float) lo);

        const double hornG = std::min (1.0, 2.0 * p2), drumG = std::min (1.0, 2.0 * (1.0 - p2));
        const double mic   = width * 0.5 * kPi; // the two microphones
        for (int c = 0; c < 2; ++c)
        {
            const double a  = c == 0 ? mic : -mic;
            const double th = kTwoPi * hornPhase + a, td = kTwoPi * drumPhase + a;
            const double h  = hornLine.read (msToSamples (1.3 - 0.9 * depth * std::sin (th)))
                             * (1.0 - 0.55 * depth * (0.5 - 0.5 * std::cos (th)));
            const double d  = drumLine.read (msToSamples (1.0 - 0.25 * depth * std::sin (td)))
                             * (1.0 - 0.35 * depth * (0.5 - 0.5 * std::cos (td)));
            out[c] = hornG * h + drumG * d;
        }
    }

    void runVibe (const double* in, double* out, double depth, double width, double p1, double p2)
    {
        static constexpr double base[4] = { 3600.0, 1100.0, 320.0, 90.0 };
        const double bias = std::pow (2.0, (p1 - 0.5) * 2.0);
        const double att = 1.0 - std::exp (-1.0 / (0.004 * fs)), rel = 1.0 - std::exp (-1.0 / (0.030 * fs));
        const bool   vibrato = stepIndex (p2, kNumVibeModes) == vibeVibrato;
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            const double bulb = std::pow (0.5 + 0.5 * lfo (c, width), 1.6); // lamp brightness
            s.lamp += (bulb > s.lamp ? att : rel) * (bulb - s.lamp);        // photocell lag
            const double sweep = bias * std::pow (2.0, depth * 4.0 * (s.lamp - 1.0));
            double y = in[c];
            for (int k = 0; k < 4; ++k)
                y = s.ap[k].process (y, tanG (base[k] * sweep));
            out[c] = vibrato ? y : 0.5 * (in[c] + y);
        }
    }

    void runPhaser (const double* in, double* out, double depth, double width, double p1, double p2)
    {
        const int    stages = phaserStages()[(size_t) stepIndex (p1, 6)];
        const double fb     = phaserRegen (p2);
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            const double u = 0.5 + 0.5 * lfo (c, width);
            const double g = tanG (150.0 * std::pow (2.0, depth * 6.0 * u));
            double y = in[c] + fb * s.feedback;
            for (int k = 0; k < stages; ++k)
                y = s.ap[k].process (y, g);
            s.feedback = std::clamp (y, -4.0, 4.0);
            out[c] = y;
        }
    }

    void runFilter (const double* in, double* out, double speed, double depth, double width, double p1, double p2)
    {
        const int    type = stepIndex (p2, kNumFilterTypes);
        const double k    = 1.0 / filterQ (p1);
        double envU = 0.0;
        if (type == envUp || type == envDown)
        {
            // Speed sets how fast the filter falls back after each note
            filterEnv.prepare (fs, 3.0, std::clamp (250.0 / std::max (0.01, speed), 15.0, 2000.0));
            const double e   = filterEnv.process (channels > 1 ? 0.5 * (in[0] + in[1]) : in[0]);
            envU = std::clamp ((melon::gainToDb (e) + 48.0) / 42.0, 0.0, 1.0);
        }
        for (int c = 0; c < 2; ++c)
        {
            double u = type == envUp ? envU : type == envDown ? 1.0 - envU : 0.5 + 0.5 * lfo (c, width);
            const double g = tanG (120.0 * std::pow (2.0, depth * 7.0 * u));
            double lp, bp, hp;
            ch[c].svf[0].process (in[c], g, k, lp, bp, hp);
            out[c] = type == bandPass ? k * bp * 2.0 : type == highPass ? hp : lp;
        }
    }

    void runFormant (const double* in, double* out, double depth, double width, double p1, double p2)
    {
        const auto& pair = vowelPairs()[(size_t) stepIndex (p1, 8)];
        const auto& va = vowels()[(size_t) pair.a];
        const auto& vb = vowels()[(size_t) pair.b];
        const double q = formantQ (p2);
        for (int c = 0; c < 2; ++c)
        {
            const double m = depth * (0.5 + 0.5 * lfo (c, width));
            double y = 0.0;
            for (int f = 0; f < 3; ++f)
            {
                const double freq = std::exp (std::log (va.f[f]) + m * (std::log (vb.f[f]) - std::log (va.f[f])));
                const double gain = melon::dbToGain (va.gainDb[f] + m * (vb.gainDb[f] - va.gainDb[f]));
                const double k    = 1.0 / q;
                double lp, bp, hp;
                ch[c].svf[f].process (in[c], tanG (freq), k, lp, bp, hp);
                y += gain * k * bp;
            }
            out[c] = 2.2 * std::sqrt (q / 4.0) * y; // narrower formants: more make-up gain
        }
    }

    void runVintageTrem (const double* in, double* out, double depth, double width, double p1, double p2)
    {
        const int    type = stepIndex (p1, kNumTremTypes);
        const double att = 1.0 - std::exp (-1.0 / (0.004 * fs)), rel = 1.0 - std::exp (-1.0 / (0.040 * fs));
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            const double l = shapeLfo (lfo (c, width), p2);
            if (type == harmonicTrem)
            {
                const double low = s.harmonicSplit.lowpass (in[c]), high = in[c] - low;
                out[c] = low * (1.0 - depth * (0.5 - 0.5 * l)) + high * (1.0 - depth * (0.5 + 0.5 * l));
            }
            else if (type == photocellTrem)
            {
                const double lamp = 0.5 + 0.5 * l;
                s.lamp += (lamp > s.lamp ? att : rel) * (lamp - s.lamp);
                out[c] = in[c] * (1.0 - depth * (1.0 - s.lamp));
            }
            else
            {
                out[c] = in[c] * (1.0 - depth * (0.5 - 0.5 * l));
            }
        }
    }

    void runPatternTrem (const double* in, double* out, double speed, double depth, double width, double p1, double p2)
    {
        const int    pattern = stepIndex (p1, 8);
        const double slewMs  = 1.0 + p2 * 0.5 * 1000.0 / std::max (0.01, speed);
        const double coeff   = 1.0 - std::exp (-1.0 / (slewMs * 0.001 * fs));
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            const double target = 1.0 - depth * (1.0 - patternLevel (pattern, chanPos (c, width)));
            s.gain += coeff * (target - s.gain);
            out[c] = in[c] * s.gain;
        }
    }

    void runAutoswell (const double* in, double* out, double depth, double p1, double p2)
    {
        const double m      = channels > 1 ? 0.5 * (in[0] + in[1]) : in[0];
        const double fastDb = melon::gainToDb (swellFast.process (m)), slowDb = melon::gainToDb (swellSlow.process (m));
        sinceTrigger += 1.0;
        if (fastDb > swellThreshDb (p2) && fastDb > slowDb + 6.0 && sinceTrigger > 0.06 * fs)
        {
            swellPos     = 0.0; // a new note: start the swell again
            sinceTrigger = 0.0;
        }
        const double rise = std::max (1.0, msToSamples (swellRiseMs (p1)));
        const double t    = std::min (1.0, swellPos / rise);
        swellPos += 1.0;
        const double target = (1.0 - depth) + depth * t * t * (3.0 - 2.0 * t);
        gainSlew.setTarget (target);
        const double g = gainSlew.next();
        out[0] = in[0] * g;
        out[1] = in[1] * g;
    }

    void runDestroyer (const double* in, double* out, double depth, double width, double p1, double p2)
    {
        const double q = std::pow (2.0, destroyerBits (p1) - 1);
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            const double hold = std::max (1.0, destroyerHold (p2) * std::pow (2.0, depth * 1.5 * lfo (c, width)));
            s.holdPhase += 1.0 / hold;
            if (s.holdPhase >= 1.0)
            {
                s.holdPhase -= std::floor (s.holdPhase);
                s.held = std::round (std::clamp (in[c] * kDestroyerDrive, -1.0, 1.0) * q) / (q * kDestroyerDrive);
            }
            out[c] = s.destroyerSmooth.lowpass (s.held);
        }
    }

    void runQuadrature (const double* in, double* out, double speed, double depth, double width, double p1, double p2)
    {
        const int    mode = stepIndex (p1, kNumQuadModes);
        const double freq = speed * quadRangeMul (stepIndex (p2, 3));
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c];
            if (c == 0)
                s.oscPhase = std::fmod (s.oscPhase + freq / fs, 1.0);
            const double ph = c == 0 ? s.oscPhase : ch[0].oscPhase - 0.5 * width;
            const double th = kTwoPi * ph;
            if (mode == quadAM)
            {
                out[c] = in[c] * ((1.0 - depth) + depth * std::cos (th));
            }
            else if (mode == quadFM)
            {
                s.delay.push ((float) in[c]);
                out[c] = s.delay.read (msToSamples (3.0 + 1.5 * depth * std::sin (th)));
            }
            else
            {
                double i, qq;
                s.hilbert.process (in[c] + 0.7 * depth * s.feedback, i, qq);
                // qq leads i by 90 degrees: i cos + qq sin moves every frequency up
                const double y = mode == shiftUp ? i * std::cos (th) + qq * std::sin (th)
                                                 : i * std::cos (th) - qq * std::sin (th);
                s.feedback = std::clamp (y, -4.0, 4.0);
                out[c] = y;
            }
        }
    }

    //--------------------------------------------------------------------------
    Params  params;
    double  fs = 48000.0;
    int     channels = 2;
    int     active = chorus;
    double  position = 0.0;

    Channel ch[2];
    melon::DelayLine hornLine, drumLine;
    melon::Biquad    xoverLow[2], xoverHigh[2];
    double           hornHz = 1.0, drumHz = 1.0, hornPhase = 0.0, drumPhase = 0.25;
    Envelope         filterEnv, swellFast, swellSlow;
    double           swellPos = 1.0e9, sinceTrigger = 1.0e9;
    melon::Smoother  gainSlew;

    melon::Smoother speedS, depthS, dryS, wetS, widthS, p1S, p2S, outS, bypassS, machineS;
};

} // namespace mmod
