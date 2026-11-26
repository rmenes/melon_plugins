// Melon Monolithic Synthesis - sound library
//
// 12 modes x 6 variations = 72 sounds. Each mode has three performance
// macros (Param 1-3). A macro at 0.5 gives the variation's own value; turning it
// down moves toward the macro's low end, turning it up toward the high end.
#pragma once

#include <array>
#include <cmath>

namespace melon::mono
{
enum Wave { Saw = 0, Square, Triangle, Sine };
enum FilterType { LP24 = 0, LP12, BP12, HP12 };
enum LfoWave { LfoSine = 0, LfoTriangle, LfoSquare, LfoSampleHold };
enum PatternType { PatternNone = 0, PatternArp, PatternSeq };

struct Patch
{
    const char* name = "Init";

    // Oscillators
    int   osc1Wave = Saw, osc2Wave = Saw;
    float osc1Level = 1.0f, osc2Level = 0.0f;
    float osc2Semi = 0.0f, osc2Cents = 0.0f;
    float osc2EnvSemi = 0.0f;      // osc 2 pitch offset driven by filter envelope (sync sweeps)
    float pulseWidth = 0.5f;
    float subLevel = 0.0f, noiseLevel = 0.0f;
    float fmAmount = 0.0f;         // osc 2 (sine) phase-modulates osc 1 (sine)
    float fmEnv = 0.0f;            // 0..1: how much FM index follows the filter envelope
    float sync = 0.0f, ringMod = 0.0f;   // used as booleans (> 0.5)

    // Filter
    int   filterType = LP24;
    float cutoff = 2500.0f, resonance = 0.2f, keyTrack = 0.5f;
    float filterEnvAmt = 0.0f;     // octaves
    float drive = 1.0f;

    // Envelopes (seconds, sustain 0..1)
    float ampA = 0.004f, ampD = 0.3f, ampS = 1.0f, ampR = 0.15f;
    float fltA = 0.004f, fltD = 0.3f, fltS = 0.0f, fltR = 0.2f;

    // LFO
    int   lfoWave = LfoSine;
    float lfoRate = 5.0f, lfoDelay = 0.0f;
    float lfoPitch = 0.0f, lfoFilter = 0.0f, lfoAmp = 0.0f, lfoPWM = 0.0f;

    // Performance
    float glide = 0.0f;            // seconds
    float dynamics = 0.7f;         // 0 = fixed level, 1 = fully follows the guitar
    float pitchEnvAmt = 0.0f, pitchEnvTime = 0.1f;  // semitones, decaying to 0
    float octave = 0.0f;
    float legato = 0.0f;           // > 0.5: pitch changes without re-picking don't retrigger

    // Pattern (SEQ / ARPEGGIO)
    int   pattern = PatternNone, patternIndex = 0;
    float rateIdx = 3.0f;          // index into kDivisions
    float gateLen = 0.6f;
    float seqDepth = 1.0f;

    // Output
    float chorusMix = 0.0f;
    float level = 0.8f;
};

// Pattern divisions in beats, slow -> fast
constexpr std::array<double, 6> kDivisions { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
constexpr std::array<const char*, 6> kDivisionNames { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };

struct ArpPattern { int length; int steps[8]; };
constexpr std::array<ArpPattern, 7> kArpPatterns {{
    { 2, { 0, 12 } },
    { 4, { 0, 4, 7, 12 } },
    { 4, { 0, 3, 7, 12 } },
    { 4, { 0, 7, 12, 7 } },
    { 8, { 0, 7, 12, 19, 24, 19, 12, 7 } },
    { 4, { 0, 12, 7, 12 } },
    { 6, { 0, 5, 7, 12, 7, 5 } },
}};

struct SeqStep { int pitch; float filter; bool on; };
struct SeqPattern { SeqStep steps[16]; };
constexpr std::array<SeqPattern, 6> kSeqPatterns {{
    // 0 Pulse
    {{ {0,1.f,1},{0,.35f,1},{0,.7f,1},{0,.35f,1},{0,1.f,1},{0,.35f,1},{0,.7f,1},{0,.5f,1},
       {0,1.f,1},{0,.35f,1},{0,.7f,1},{0,.35f,1},{0,1.f,1},{0,.5f,1},{0,.8f,1},{0,.4f,1} }},
    // 1 Gallop
    {{ {0,1.f,1},{0,.3f,0},{0,.6f,1},{0,.6f,1},{0,1.f,1},{0,.3f,0},{0,.6f,1},{0,.6f,1},
       {0,1.f,1},{0,.3f,0},{0,.6f,1},{0,.6f,1},{0,1.f,1},{0,.3f,0},{0,.6f,1},{0,.7f,1} }},
    // 2 Octave bounce
    {{ {0,1.f,1},{12,.6f,1},{0,.8f,1},{12,.5f,1},{0,1.f,1},{12,.6f,1},{0,.8f,1},{12,.7f,1},
       {0,1.f,1},{12,.6f,1},{0,.8f,1},{12,.5f,1},{0,1.f,1},{12,.8f,1},{7,.7f,1},{12,.6f,1} }},
    // 3 Acid line
    {{ {0,1.f,1},{0,.4f,1},{12,.9f,1},{0,.3f,1},{3,.7f,1},{0,.4f,0},{0,.8f,1},{7,.6f,1},
       {0,1.f,1},{12,.5f,1},{0,.4f,1},{10,.9f,1},{0,.3f,1},{5,.6f,1},{0,.8f,1},{12,.7f,1} }},
    // 4 Trance gate
    {{ {0,1.f,1},{0,1.f,0},{0,.8f,1},{0,.8f,1},{0,1.f,0},{0,.9f,1},{0,.8f,1},{0,1.f,0},
       {0,1.f,1},{0,1.f,0},{0,.8f,1},{0,.9f,1},{0,1.f,0},{0,.9f,1},{0,.8f,1},{0,.9f,1} }},
    // 5 Stutter
    {{ {0,1.f,1},{0,.9f,1},{0,.8f,1},{0,.7f,1},{0,.3f,0},{0,.3f,0},{12,1.f,1},{0,.6f,1},
       {0,1.f,1},{0,.9f,1},{0,.3f,0},{0,.6f,1},{7,1.f,1},{0,.3f,0},{0,.8f,1},{0,.6f,1} }},
}};

// ---------------------------------------------------------------------------
// Macros
struct MacroTarget
{
    float Patch::* field = nullptr;
    float lo = 0.0f, hi = 1.0f;
    bool exponential = false;
};

struct Macro
{
    const char* name;
    MacroTarget targets[2];
    int numTargets;
};

struct ModeInfo
{
    const char* name;
    Macro macros[3];
};

enum Mode { Lead = 0, Pad, String, Bell, Organ, Bass, Dual, Sweep, Noise, Sfx, Seq, Arpeggio, NumModes };
constexpr int kVariationsPerMode = 6;

inline const std::array<ModeInfo, NumModes>& getModeInfos()
{
    using P = Patch;
    static const std::array<ModeInfo, NumModes> infos {{
        { "LEAD",     {{ "Filter",     {{ &P::cutoff, 200, 12000, true }}, 1 },
                       { "Resonance",  {{ &P::resonance, 0, 0.95f }}, 1 },
                       { "Glide",      {{ &P::glide, 0, 0.4f }}, 1 } } },
        { "PAD",      {{ "Attack",     {{ &P::ampA, 0.005f, 3.0f, true }}, 1 },
                       { "Release",    {{ &P::ampR, 0.05f, 5.0f, true }}, 1 },
                       { "Filter",     {{ &P::cutoff, 200, 12000, true }}, 1 } } },
        { "STRING",   {{ "Attack",     {{ &P::ampA, 0.002f, 2.5f, true }}, 1 },
                       { "Release",    {{ &P::ampR, 0.05f, 4.0f, true }}, 1 },
                       { "Brightness", {{ &P::cutoff, 400, 12000, true }}, 1 } } },
        { "BELL",     {{ "Decay",      {{ &P::ampD, 0.15f, 8.0f, true }, { &P::ampR, 0.1f, 6.0f, true }}, 2 },
                       { "Tone",       {{ &P::fmAmount, 0, 7.0f }}, 1 },
                       { "Brightness", {{ &P::cutoff, 600, 16000, true }}, 1 } } },
        { "ORGAN",    {{ "Tone",       {{ &P::cutoff, 500, 14000, true }, { &P::drive, 1.0f, 4.0f }}, 2 },
                       { "Vibrato",    {{ &P::lfoPitch, 0, 0.6f }}, 1 },
                       { "Sub",        {{ &P::subLevel, 0, 1.0f }}, 1 } } },
        { "BASS",     {{ "Filter",     {{ &P::cutoff, 80, 5000, true }}, 1 },
                       { "Resonance",  {{ &P::resonance, 0, 0.95f }}, 1 },
                       { "Decay",      {{ &P::fltD, 0.03f, 1.5f, true }}, 1 } } },
        { "DUAL",     {{ "Detune",     {{ &P::osc2Cents, 0, 40.0f }}, 1 },
                       { "Filter",     {{ &P::cutoff, 200, 12000, true }}, 1 },
                       { "Balance",    {{ &P::osc2Level, 0, 1.0f }, { &P::osc1Level, 1.0f, 0.3f }}, 2 } } },
        { "SWEEP",    {{ "Time",       {{ &P::fltD, 0.05f, 5.0f, true }, { &P::lfoRate, 4.0f, 0.05f, true }}, 2 },
                       { "Depth",      {{ &P::filterEnvAmt, 0, 7.0f }, { &P::lfoFilter, 0, 4.0f }}, 2 },
                       { "Resonance",  {{ &P::resonance, 0, 0.95f }}, 1 } } },
        { "NOISE",    {{ "Color",      {{ &P::cutoff, 100, 12000, true }}, 1 },
                       { "Resonance",  {{ &P::resonance, 0, 0.97f }}, 1 },
                       { "Decay",      {{ &P::ampD, 0.03f, 3.0f, true }, { &P::ampR, 0.05f, 4.0f, true }}, 2 } } },
        { "SFX",      {{ "Rate",       {{ &P::lfoRate, 0.1f, 30.0f, true }, { &P::pitchEnvTime, 0.02f, 2.0f, true }}, 2 },
                       { "Depth",      {{ &P::lfoPitch, 0, 24.0f }, { &P::pitchEnvAmt, 0, 36.0f }}, 2 },
                       { "Filter",     {{ &P::cutoff, 200, 12000, true }}, 1 } } },
        { "SEQ",      {{ "Rate",       {{ &P::rateIdx, 0, 5.0f }}, 1 },
                       { "Depth",      {{ &P::seqDepth, 0, 1.6f }}, 1 },
                       { "Resonance",  {{ &P::resonance, 0, 0.95f }}, 1 } } },
        { "ARPEGGIO", {{ "Rate",       {{ &P::rateIdx, 0, 5.0f }}, 1 },
                       { "Gate",       {{ &P::gateLen, 0.08f, 1.0f }}, 1 },
                       { "Filter",     {{ &P::cutoff, 300, 12000, true }}, 1 } } },
    }};
    return infos;
}

// ---------------------------------------------------------------------------
// Variations
inline Patch makePatch (const char* name) { Patch p; p.name = name; return p; }

inline const std::array<std::array<Patch, kVariationsPerMode>, NumModes>& getPatchBank()
{
    static const auto bank = []
    {
        std::array<std::array<Patch, kVariationsPerMode>, NumModes> b {};

        // ----- LEAD
        {
            auto& m = b[Lead];
            auto p = makePatch ("Saw Lead");
            p.osc2Level = 0.6f; p.osc2Cents = 7; p.cutoff = 2500; p.resonance = 0.3f;
            p.filterEnvAmt = 1.5f; p.fltD = 0.4f; p.glide = 0.03f; p.dynamics = 0.8f;
            m[0] = p;

            p = makePatch ("Square Lead");
            p.osc1Wave = Square; p.cutoff = 3000; p.resonance = 0.2f; p.filterEnvAmt = 1.0f;
            p.lfoRate = 5.5f; p.lfoDelay = 0.4f; p.lfoPitch = 0.15f; p.glide = 0.02f;
            m[1] = p;

            p = makePatch ("Sync Lead");
            p.osc1Level = 0.0f; p.osc2Level = 1.0f; p.sync = 1; p.osc2Semi = 5;
            p.osc2EnvSemi = 18; p.fltD = 0.6f; p.fltS = 0.2f; p.cutoff = 6000; p.resonance = 0.1f;
            m[2] = p;

            p = makePatch ("Fat Lead");
            p.osc2Level = 0.7f; p.osc2Semi = -12; p.osc2Cents = 10; p.subLevel = 0.3f;
            p.cutoff = 1800; p.resonance = 0.4f; p.drive = 2.0f; p.filterEnvAmt = 1.2f;
            m[3] = p;

            p = makePatch ("Soft Lead");
            p.osc1Wave = Triangle; p.osc2Wave = Sine; p.osc2Level = 0.5f; p.osc2Semi = 12;
            p.filterType = LP12; p.cutoff = 2000; p.ampA = 0.02f; p.chorusMix = 0.3f;
            p.lfoRate = 5.0f; p.lfoDelay = 0.3f; p.lfoPitch = 0.12f;
            m[4] = p;

            p = makePatch ("Resonant Lead");
            p.cutoff = 900; p.resonance = 0.75f; p.filterEnvAmt = 3.0f; p.fltD = 0.25f; p.fltS = 0.1f;
            m[5] = p;
        }

        // ----- PAD
        {
            auto& m = b[Pad];
            auto p = makePatch ("Warm Pad");
            p.osc2Level = 0.9f; p.osc2Cents = 12; p.cutoff = 1500; p.resonance = 0.1f;
            p.ampA = 0.6f; p.ampR = 1.5f; p.chorusMix = 0.6f; p.dynamics = 0.0f; p.legato = 1;
            m[0] = p;

            p = makePatch ("Glass Pad");
            p.osc1Wave = Square; p.pulseWidth = 0.3f; p.osc2Wave = Triangle; p.osc2Level = 0.7f;
            p.osc2Semi = 12; p.filterType = LP12; p.cutoff = 3500; p.ampA = 1.0f; p.ampR = 2.0f;
            p.lfoRate = 0.4f; p.lfoPWM = 0.3f; p.chorusMix = 0.5f; p.dynamics = 0.0f; p.legato = 1;
            m[1] = p;

            p = makePatch ("Dark Pad");
            p.subLevel = 0.6f; p.osc2Level = 0.6f; p.osc2Cents = -9; p.cutoff = 600; p.resonance = 0.2f;
            p.ampA = 0.8f; p.ampR = 2.5f; p.chorusMix = 0.4f; p.dynamics = 0.0f; p.legato = 1;
            m[2] = p;

            p = makePatch ("Swell Pad");
            p.osc2Level = 0.5f; p.osc2Semi = 7; p.cutoff = 900; p.filterEnvAmt = 2.0f;
            p.fltA = 1.5f; p.fltD = 1.0f; p.fltS = 0.5f; p.ampA = 1.2f; p.ampR = 1.8f;
            p.chorusMix = 0.5f; p.dynamics = 0.0f; p.legato = 1;
            m[3] = p;

            p = makePatch ("Shimmer Pad");
            p.osc2Level = 0.7f; p.osc2Semi = 12; p.osc2Cents = 6; p.cutoff = 5000; p.resonance = 0.3f;
            p.lfoRate = 0.2f; p.lfoFilter = 0.6f; p.ampA = 0.9f; p.ampR = 3.0f; p.chorusMix = 0.8f;
            p.dynamics = 0.0f; p.legato = 1;
            m[4] = p;

            p = makePatch ("Choir Pad");
            p.osc1Wave = Square; p.pulseWidth = 0.4f; p.osc2Level = 0.5f; p.osc2Cents = 8;
            p.filterType = BP12; p.cutoff = 900; p.resonance = 0.5f; p.keyTrack = 0.3f;
            p.lfoRate = 5.0f; p.lfoPitch = 0.1f; p.ampA = 0.5f; p.ampR = 1.5f; p.chorusMix = 0.7f;
            p.dynamics = 0.0f; p.legato = 1; p.level = 1.0f;
            m[5] = p;
        }

        // ----- STRING
        {
            auto& m = b[String];
            auto p = makePatch ("Ensemble");
            p.osc2Level = 0.9f; p.osc2Cents = 8; p.cutoff = 3500; p.resonance = 0.05f;
            p.ampA = 0.25f; p.ampR = 0.8f; p.chorusMix = 0.9f; p.lfoRate = 5.5f; p.lfoPitch = 0.08f;
            p.dynamics = 0.3f;
            m[0] = p;

            p = makePatch ("Cello");
            p.octave = -1; p.osc2Level = 0.6f; p.osc2Cents = 5; p.cutoff = 1500; p.ampA = 0.15f;
            p.ampR = 0.6f; p.lfoRate = 5.0f; p.lfoDelay = 0.3f; p.lfoPitch = 0.12f; p.chorusMix = 0.3f;
            m[1] = p;

            p = makePatch ("Pizzicato");
            p.cutoff = 2500; p.filterEnvAmt = 2.0f; p.fltD = 0.15f; p.ampA = 0.002f; p.ampD = 0.25f;
            p.ampS = 0.0f; p.ampR = 0.2f; p.dynamics = 0.5f; p.chorusMix = 0.4f;
            m[2] = p;

            p = makePatch ("Slow Strings");
            p.osc2Level = 0.9f; p.osc2Cents = 10; p.cutoff = 2800; p.ampA = 1.0f; p.ampR = 1.5f;
            p.chorusMix = 0.8f; p.dynamics = 0.0f; p.legato = 1;
            m[3] = p;

            p = makePatch ("Bright Strings");
            p.osc2Level = 0.8f; p.osc2Cents = 9; p.cutoff = 7000; p.resonance = 0.1f;
            p.ampA = 0.12f; p.ampR = 0.7f; p.chorusMix = 0.7f;
            m[4] = p;

            p = makePatch ("Tremolo Strings");
            p.osc2Level = 0.8f; p.osc2Cents = 8; p.cutoff = 3200; p.ampA = 0.2f; p.ampR = 0.7f;
            p.lfoRate = 7.0f; p.lfoAmp = 0.6f; p.chorusMix = 0.6f; p.dynamics = 0.2f;
            m[5] = p;
        }

        // ----- BELL (FM ratio set through osc2Semi)
        {
            auto& m = b[Bell];
            auto ratioSemi = [] (float ratio) { return 12.0f * std::log2 (ratio); };

            auto p = makePatch ("FM Bell");
            p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Semi = ratioSemi (3.5f); p.fmAmount = 2.5f;
            p.fmEnv = 0.7f; p.fltD = 1.5f; p.ampA = 0.001f; p.ampD = 2.5f; p.ampS = 0.0f; p.ampR = 2.0f;
            p.cutoff = 12000; p.keyTrack = 0.0f; p.dynamics = 0.3f; p.chorusMix = 0.2f;
            m[0] = p;

            p = makePatch ("Tubular Bell");
            p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Semi = ratioSemi (2.0f); p.fmAmount = 3.0f;
            p.fmEnv = 0.6f; p.fltD = 2.0f; p.ampD = 3.5f; p.ampS = 0.0f; p.ampR = 3.0f;
            p.cutoff = 10000; p.keyTrack = 0.0f; p.dynamics = 0.3f;
            m[1] = p;

            p = makePatch ("Ring Bell");
            p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Semi = ratioSemi (3.0f) + 0.3f; p.ringMod = 1;
            p.ampD = 2.0f; p.ampS = 0.0f; p.ampR = 1.5f; p.cutoff = 9000; p.keyTrack = 0.0f;
            p.dynamics = 0.3f; p.chorusMix = 0.3f;
            m[2] = p;

            p = makePatch ("Glockenspiel");
            p.octave = 1; p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Semi = ratioSemi (4.0f);
            p.fmAmount = 1.5f; p.fmEnv = 0.8f; p.fltD = 0.4f; p.ampD = 1.2f; p.ampS = 0.0f; p.ampR = 1.0f;
            p.cutoff = 14000; p.keyTrack = 0.0f; p.dynamics = 0.3f;
            m[3] = p;

            p = makePatch ("Soft Mallet");
            p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Semi = 0; p.fmAmount = 1.0f; p.fmEnv = 1.0f;
            p.fltD = 0.15f; p.ampD = 0.9f; p.ampS = 0.0f; p.ampR = 0.8f; p.cutoff = 8000;
            p.keyTrack = 0.0f; p.dynamics = 0.4f; p.chorusMix = 0.3f;
            m[4] = p;

            p = makePatch ("Metal Bell");
            p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Semi = ratioSemi (1.41f); p.fmAmount = 4.0f;
            p.fmEnv = 0.5f; p.fltD = 2.5f; p.ampD = 3.0f; p.ampS = 0.0f; p.ampR = 2.5f;
            p.cutoff = 12000; p.keyTrack = 0.0f; p.dynamics = 0.3f; p.chorusMix = 0.3f;
            m[5] = p;
        }

        // ----- ORGAN
        {
            auto& m = b[Organ];
            auto p = makePatch ("Drawbar Organ");
            p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Level = 0.6f; p.osc2Semi = 12; p.subLevel = 0.4f;
            p.cutoff = 6000; p.keyTrack = 0.3f; p.lfoRate = 6.5f; p.lfoPitch = 0.1f; p.chorusMix = 0.4f;
            p.ampA = 0.005f; p.ampR = 0.05f; p.dynamics = 0.2f; p.level = 0.7f;
            m[0] = p;

            p = makePatch ("Rock Organ");
            p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Level = 0.5f; p.osc2Semi = 19; p.subLevel = 0.5f;
            p.cutoff = 5000; p.drive = 2.5f; p.lfoRate = 6.8f; p.lfoPitch = 0.12f; p.chorusMix = 0.5f;
            p.ampR = 0.05f; p.dynamics = 0.2f; p.level = 0.7f;
            m[1] = p;

            p = makePatch ("Church Organ");
            p.osc1Wave = Sine; p.osc2Wave = Triangle; p.osc2Level = 0.7f; p.osc2Semi = 12; p.subLevel = 0.6f;
            p.cutoff = 4000; p.ampA = 0.1f; p.ampR = 0.8f; p.chorusMix = 0.7f; p.dynamics = 0.0f;
            p.legato = 1; p.level = 0.7f;
            m[2] = p;

            p = makePatch ("Combo Organ");
            p.osc1Wave = Square; p.pulseWidth = 0.3f; p.osc2Wave = Triangle; p.osc2Level = 0.6f;
            p.osc2Semi = 12; p.filterType = LP12; p.cutoff = 4000; p.lfoRate = 6.0f; p.lfoPitch = 0.15f;
            p.ampR = 0.05f; p.dynamics = 0.2f; p.level = 0.6f;
            m[3] = p;

            p = makePatch ("Jazz Organ");
            p.osc1Wave = Sine; p.osc2Wave = Sine; p.osc2Level = 0.3f; p.osc2Semi = 19; p.subLevel = 0.2f;
            p.cutoff = 3500; p.filterEnvAmt = 1.0f; p.fltD = 0.12f; p.lfoRate = 6.8f; p.lfoPitch = 0.12f;
            p.chorusMix = 0.5f; p.ampR = 0.06f; p.dynamics = 0.3f; p.level = 0.8f;
            m[4] = p;

            p = makePatch ("Reed Organ");
            p.osc1Wave = Square; p.pulseWidth = 0.25f; p.cutoff = 2200; p.resonance = 0.15f;
            p.lfoRate = 5.0f; p.lfoAmp = 0.15f; p.ampA = 0.04f; p.ampR = 0.2f; p.chorusMix = 0.3f;
            p.dynamics = 0.2f; p.level = 0.6f;
            m[5] = p;
        }

        // ----- BASS
        {
            auto& m = b[Bass];
            auto p = makePatch ("Synth Bass");
            p.octave = -1; p.subLevel = 0.5f; p.cutoff = 600; p.resonance = 0.35f; p.filterEnvAmt = 2.5f;
            p.fltD = 0.2f; p.ampS = 0.8f; p.ampR = 0.08f; p.dynamics = 0.8f; p.keyTrack = 0.3f;
            m[0] = p;

            p = makePatch ("Acid Bass");
            p.octave = -1; p.cutoff = 400; p.resonance = 0.85f; p.filterEnvAmt = 3.5f; p.fltD = 0.18f;
            p.drive = 2.5f; p.glide = 0.05f; p.ampR = 0.06f; p.dynamics = 0.8f;
            m[1] = p;

            p = makePatch ("Sub Bass");
            p.octave = -1; p.osc1Wave = Sine; p.subLevel = 0.8f; p.cutoff = 300; p.keyTrack = 0.5f;
            p.ampR = 0.1f; p.dynamics = 0.8f; p.level = 0.9f;
            m[2] = p;

            p = makePatch ("Pluck Bass");
            p.octave = -1; p.osc1Wave = Square; p.cutoff = 500; p.filterEnvAmt = 3.0f; p.fltD = 0.12f;
            p.ampD = 0.4f; p.ampS = 0.3f; p.ampR = 0.1f; p.dynamics = 0.7f;
            m[3] = p;

            p = makePatch ("Fat Bass");
            p.octave = -1; p.osc2Level = 0.8f; p.osc2Cents = 10; p.subLevel = 0.5f; p.cutoff = 800;
            p.resonance = 0.2f; p.filterEnvAmt = 1.5f; p.fltD = 0.3f; p.drive = 1.8f; p.ampR = 0.08f;
            m[4] = p;

            p = makePatch ("Wobble Bass");
            p.octave = -1; p.subLevel = 0.4f; p.cutoff = 400; p.resonance = 0.6f; p.lfoRate = 4.0f;
            p.lfoFilter = 2.5f; p.lfoWave = LfoTriangle; p.drive = 2.0f; p.dynamics = 0.5f;
            m[5] = p;
        }

        // ----- DUAL
        {
            auto& m = b[Dual];
            auto p = makePatch ("Octave Dual");
            p.osc2Wave = Square; p.osc2Level = 0.7f; p.osc2Semi = 12; p.cutoff = 3000; p.chorusMix = 0.2f;
            m[0] = p;

            p = makePatch ("Fifth Dual");
            p.osc2Level = 0.7f; p.osc2Semi = 7; p.cutoff = 2500; p.chorusMix = 0.3f;
            m[1] = p;

            p = makePatch ("Detuned Dual");
            p.osc2Level = 1.0f; p.osc2Cents = 18; p.cutoff = 3500; p.chorusMix = 0.5f;
            m[2] = p;

            p = makePatch ("Sub Dual");
            p.osc2Wave = Square; p.osc2Level = 0.8f; p.osc2Semi = -12; p.cutoff = 2000; p.resonance = 0.25f;
            m[3] = p;

            p = makePatch ("Fourth Dual");
            p.osc1Wave = Square; p.osc2Level = 0.6f; p.osc2Semi = 5; p.cutoff = 2800; p.chorusMix = 0.3f;
            m[4] = p;

            p = makePatch ("Wide Dual");
            p.osc2Level = 0.8f; p.osc2Semi = 12; p.osc2Cents = 10; p.cutoff = 4000; p.chorusMix = 0.8f;
            m[5] = p;
        }

        // ----- SWEEP
        {
            auto& m = b[Sweep];
            auto p = makePatch ("Filter Sweep");
            p.cutoff = 200; p.resonance = 0.6f; p.filterEnvAmt = 5.0f; p.fltA = 0.05f; p.fltD = 1.5f;
            p.keyTrack = 0.3f;
            m[0] = p;

            p = makePatch ("Rising Sweep");
            p.osc2Level = 0.6f; p.osc2Cents = 9; p.cutoff = 150; p.resonance = 0.5f; p.filterEnvAmt = 4.5f;
            p.fltA = 1.2f; p.fltD = 0.1f; p.fltS = 1.0f; p.keyTrack = 0.3f; p.chorusMix = 0.3f;
            m[1] = p;

            p = makePatch ("Sync Sweep");
            p.osc1Level = 0.0f; p.osc2Level = 1.0f; p.sync = 1; p.osc2EnvSemi = 24; p.fltD = 0.8f;
            p.cutoff = 7000; p.resonance = 0.1f;
            m[2] = p;

            p = makePatch ("PWM Sweep");
            p.osc1Wave = Square; p.lfoRate = 0.7f; p.lfoPWM = 0.45f; p.cutoff = 3000; p.chorusMix = 0.4f;
            m[3] = p;

            p = makePatch ("Slow LFO Sweep");
            p.cutoff = 400; p.resonance = 0.7f; p.lfoRate = 0.25f; p.lfoFilter = 3.0f; p.lfoWave = LfoTriangle;
            m[4] = p;

            p = makePatch ("Band Sweep");
            p.filterType = BP12; p.cutoff = 300; p.resonance = 0.7f; p.filterEnvAmt = 4.0f; p.fltD = 1.0f;
            p.level = 1.0f;
            m[5] = p;
        }

        // ----- NOISE
        {
            auto& m = b[Noise];
            auto p = makePatch ("Wind");
            p.osc1Level = 0.0f; p.noiseLevel = 1.0f; p.cutoff = 262; p.resonance = 0.85f; p.keyTrack = 1.0f;
            p.filterType = BP12; p.ampA = 0.2f; p.ampR = 0.6f; p.dynamics = 0.5f; p.level = 1.0f;
            m[0] = p;

            p = makePatch ("Breath Lead");
            p.osc1Level = 0.5f; p.noiseLevel = 0.5f; p.filterType = BP12; p.cutoff = 1500; p.resonance = 0.3f;
            p.keyTrack = 1.0f; p.ampA = 0.03f; p.lfoRate = 5.0f; p.lfoDelay = 0.3f; p.lfoPitch = 0.1f;
            m[1] = p;

            p = makePatch ("Noise Burst");
            p.osc1Level = 0.2f; p.noiseLevel = 1.0f; p.filterType = HP12; p.cutoff = 1200;
            p.ampD = 0.15f; p.ampS = 0.0f; p.ampR = 0.1f; p.dynamics = 0.5f;
            m[2] = p;

            p = makePatch ("Tuned Noise");
            p.osc1Level = 0.0f; p.noiseLevel = 1.0f; p.filterType = BP12; p.cutoff = 262; p.resonance = 0.95f;
            p.keyTrack = 1.0f; p.level = 1.0f;
            m[3] = p;

            p = makePatch ("Ocean");
            p.osc1Level = 0.0f; p.noiseLevel = 1.0f; p.cutoff = 800; p.resonance = 0.2f; p.keyTrack = 0.0f;
            p.lfoRate = 0.15f; p.lfoFilter = 1.5f; p.ampA = 1.0f; p.ampR = 2.0f; p.dynamics = 0.0f;
            p.legato = 1; p.chorusMix = 0.5f;
            m[4] = p;

            p = makePatch ("Static");
            p.osc1Wave = Square; p.osc1Level = 0.5f; p.noiseLevel = 0.6f; p.cutoff = 1500; p.resonance = 0.6f;
            p.lfoWave = LfoSampleHold; p.lfoRate = 12.0f; p.lfoFilter = 2.0f;
            m[5] = p;
        }

        // ----- SFX
        {
            auto& m = b[Sfx];
            auto p = makePatch ("Laser");
            p.osc1Wave = Square; p.pitchEnvAmt = 24; p.pitchEnvTime = 0.25f; p.cutoff = 4000;
            p.ampD = 0.5f; p.ampS = 0.4f;
            m[0] = p;

            p = makePatch ("Siren");
            p.osc1Wave = Triangle; p.osc2Wave = Square; p.osc2Level = 0.3f; p.lfoRate = 0.8f; p.lfoPitch = 5.0f;
            p.lfoWave = LfoTriangle; p.cutoff = 5000; p.dynamics = 0.3f;
            m[1] = p;

            p = makePatch ("Ring Mod");
            p.osc2Wave = Sine; p.osc2Semi = 6.3f; p.ringMod = 1; p.cutoff = 6000; p.dynamics = 0.7f;
            m[2] = p;

            p = makePatch ("Robot");
            p.osc1Wave = Square; p.cutoff = 900; p.resonance = 0.7f; p.lfoWave = LfoSampleHold; p.lfoRate = 8.0f;
            p.lfoFilter = 3.0f;
            m[3] = p;

            p = makePatch ("Rise Up");
            p.osc2Level = 0.6f; p.osc2Cents = 12; p.pitchEnvAmt = -24; p.pitchEnvTime = 0.6f;
            p.cutoff = 3000; p.chorusMix = 0.4f;
            m[4] = p;

            p = makePatch ("Trill");
            p.osc1Wave = Square; p.pulseWidth = 0.35f; p.lfoWave = LfoSquare; p.lfoRate = 8.0f; p.lfoPitch = 2.0f;
            p.cutoff = 3500;
            m[5] = p;
        }

        // ----- SEQ
        {
            auto& m = b[Seq];
            const char* names[] { "Pulse 16", "Gallop", "Octave Bounce", "Acid Line", "Trance Gate", "Stutter" };
            for (int i = 0; i < kVariationsPerMode; ++i)
            {
                auto p = makePatch (names[i]);
                p.pattern = PatternSeq; p.patternIndex = i; p.rateIdx = 3; p.gateLen = 0.7f;
                p.cutoff = 2500; p.resonance = 0.45f; p.filterEnvAmt = 1.5f; p.fltD = 0.12f;
                p.ampA = 0.002f; p.ampR = 0.04f; p.seqDepth = 1.0f; p.dynamics = 0.3f;
                m[(size_t) i] = p;
            }
            m[3].resonance = 0.75f; m[3].drive = 2.0f; m[3].cutoff = 1200; m[3].filterEnvAmt = 3.0f;
            m[4].osc2Level = 0.9f; m[4].osc2Cents = 12; m[4].chorusMix = 0.6f; m[4].gateLen = 0.85f;
            m[4].resonance = 0.2f; m[4].filterEnvAmt = 0.5f;
            m[5].rateIdx = 5; m[5].osc1Wave = Square;
        }

        // ----- ARPEGGIO
        {
            auto& m = b[Arpeggio];
            const char* names[] { "Octave Arp", "Major Arp", "Minor Arp", "Fifths Arp", "Up & Down", "Sus Arp" };
            const int patternIdx[] { 0, 1, 2, 3, 4, 6 };
            const float rates[] { 3, 3, 3, 1, 3, 2 };
            for (int i = 0; i < kVariationsPerMode; ++i)
            {
                auto p = makePatch (names[i]);
                p.pattern = PatternArp; p.patternIndex = patternIdx[i]; p.rateIdx = rates[i];
                p.gateLen = 0.55f; p.cutoff = 3000; p.resonance = 0.3f; p.filterEnvAmt = 1.5f;
                p.fltD = 0.15f; p.ampA = 0.002f; p.ampR = 0.08f; p.dynamics = 0.3f; p.chorusMix = 0.3f;
                m[(size_t) i] = p;
            }
            m[1].osc1Wave = Square; m[3].osc2Level = 0.5f; m[3].osc2Cents = 8;
            m[5].osc1Wave = Triangle; m[5].filterType = LP12;
        }

        return b;
    }();
    return bank;
}

// Builds the sound actually played from mode, variation and the three macros.
inline Patch buildPatch (int mode, int variation, float p1, float p2, float p3)
{
    mode = mode < 0 ? 0 : (mode >= NumModes ? NumModes - 1 : mode);
    variation = variation < 0 ? 0 : (variation >= kVariationsPerMode ? kVariationsPerMode - 1 : variation);

    Patch p = getPatchBank()[(size_t) mode][(size_t) variation];
    const ModeInfo& info = getModeInfos()[(size_t) mode];
    const float macroValues[3] { p1, p2, p3 };

    for (int mi = 0; mi < 3; ++mi)
    {
        const float v = macroValues[mi] < 0.0f ? 0.0f : (macroValues[mi] > 1.0f ? 1.0f : macroValues[mi]);
        const Macro& macro = info.macros[mi];
        for (int t = 0; t < macro.numTargets; ++t)
        {
            const MacroTarget& target = macro.targets[t];
            float& field = p.*(target.field);
            const float base = field;
            const float end = v < 0.5f ? target.lo : target.hi;
            const float amount = v < 0.5f ? (0.5f - v) * 2.0f : (v - 0.5f) * 2.0f;

            if (target.exponential && base > 0.0f && end > 0.0f)
                field = base * std::pow (end / base, amount);
            else
                field = base + (end - base) * amount;
        }
    }
    return p;
}

} // namespace melon::mono
