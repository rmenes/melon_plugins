// DirtDsp.h - Melon Dirt DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// Eleven overdrive "models", each inspired by a classic pedal. Every model is described by a
// handful of numbers (filters, gain range, clipping curve, tone control) and runs through the
// same signal flow, per channel:
//
//   in ─┬──────────────────────────────────────────────── × clean amount ──┐
//       │                                                                   (+) ─► tone ─► post LP ─► level ─┐
//       └─► input HP ─► mid hump ─► [ 4x oversampled: gain ─► LP ─► clip    │                               │
//                                       (─► HP ─► gain ─► clip) ] ─────────┘                               │
//   in ─────────────────────────────────────────────────────────── dry/wet Mix ◄──────────────────────────────┘
//
// The clipping runs at 4x the sample rate so high Drive settings don't alias.
// These are not circuit models: they reproduce the voicing of each pedal family (where the
// mids sit, how much bass reaches the clipper, soft vs hard vs asymmetric clipping, the range
// of the tone control and whether clean signal is blended back in).
#pragma once

#include "MelonDsp.h"
#include <array>

namespace dirt
{
using melon::kPi;

enum class Clip { soft, asymmetric, hard, led, germanium, mosfet };
enum class Tone { lowpass, tilt };

struct Model
{
    const char* name;
    double inHpHz;              // high-pass before the clipper (how much bass gets distorted)
    double humpHz, humpDb, humpQ; // mid boost before the clipper
    double minGainDb, maxGainDb;  // Drive range
    double preClipLpHz;         // smoothing before the clipper (op-amp bandwidth)
    Clip   clip;
    double bias;                // asymmetry for asymmetric / mosfet clipping
    int    stages;              // 1 = pedal, 2 = "amp in a box" (cascaded gain stages)
    double interHpHz;           // high-pass between the two stages
    double clean;               // clean signal mixed back after the clipper
    bool   cleanFollowsDrive;   // clean blend falls as Drive rises (the "transparent" pedal)
    Tone   tone;
    double toneLoHz, toneHiHz;  // low-pass sweep range (Tone::lowpass)
    double tiltDb, tiltHz;      // ± shelf range and pivot (Tone::tilt)
    double postLpHz;            // fixed output smoothing
    double trimDb;              // level matching between models
};

// Evocative names (not the original pedal names). See README for what inspired each one.
inline const std::array<Model, 11>& models()
{
    static const std::array<Model, 11> m { {
        //  name              inHP   hump  dB   Q    gain lo/hi  preLP  clip               bias st  iHP  clean follow tone          lo     hi     tilt piv   postLP  trim
        { "Emerald '79",      720,   900, 3.0, 0.7, 12, 41,    6000, Clip::soft,        0.00, 1,   0, 1.00, false, Tone::lowpass, 800,  6000,   0,   0,   9000,   -6.6 },
        { "Emerald Nine",     720,  1000, 4.0, 0.7, 12, 41,    6500, Clip::soft,        0.00, 1,   0, 0.80, false, Tone::lowpass, 900,  7500,   0,   0,  10500,   -7.6 },
        { "Yellow Sun",       720,   800, 2.0, 0.7, 14, 44,    6000, Clip::asymmetric,  0.35, 1,   0, 1.00, false, Tone::lowpass, 700,  7000,   0,   0,   9000,   -8.3 },
        { "Plexi Grump",      120,  1200, 5.0, 0.8, 10, 40,    7000, Clip::hard,        0.20, 2, 300, 0.00, false, Tone::tilt,      0,     0,  10, 1000,   6500,  -16.2 },
        { "Gutter Rodent",    150,     0, 0.0, 0.7,  6, 58,    7000, Clip::hard,        0.00, 1,   0, 0.00, false, Tone::lowpass, 600, 12000,   0,   0,  11000,  -10.6 },
        { "Hot Rod Brown",    150,   700, 4.0, 0.9, 15, 50,    7000, Clip::led,         0.25, 2, 600, 0.00, false, Tone::tilt,      0,     0,  10, 1200,   7000,  -20.1 },
        { "Gilded Horse",     400,  1000, 2.0, 0.6,  6, 40,    9000, Clip::germanium,   0.00, 1,   0, 1.00, true,  Tone::tilt,      0,     0,   8, 1500,  12000,   -5.8 },
        { "Dawn Flower",      200,     0, 0.0, 0.7,  3, 30,    9000, Clip::hard,        0.00, 1,   0, 0.30, false, Tone::tilt,      0,     0,   8, 1000,   9000,   -0.9 },
        { "Delta Blues",      100,  1500, 2.0, 0.7,  5, 40,    8000, Clip::asymmetric,  0.15, 1,   0, 0.20, false, Tone::tilt,      0,     0,  10, 1000,   8000,   -6.4 },
        { "Obsession",        100,  1500, 2.0, 0.7,  8, 50,    9000, Clip::mosfet,      0.30, 1,   0, 0.00, false, Tone::lowpass, 1500, 12000,   0,   0,  11000,  -10.6 },
        { "Studio Green",      90,     0, 0.0, 0.7,  4, 36,    8000, Clip::soft,        0.00, 1,   0, 0.40, false, Tone::tilt,      0,     0,   8,  800,   8500,   -3.9 },
    } };
    return m;
}

constexpr int    kNumModels   = 11;
constexpr double kClipScale   = 0.35;  // overall level after the clipper
constexpr double kStage2Gain  = 4.0;   // extra gain of the second stage (amp-in-a-box models)
constexpr double kSmoothingMs = 20.0;
constexpr double kModelFadeMs = 8.0;
constexpr int    kMaxChannels = 2;

struct Params
{
    int    model    = 0;
    double drive    = 0.5; // 0..1
    double tone     = 0.5; // 0..1
    double level    = 0.5; // 0..1 (0.5 = unity)
    double mix      = 1.0; // 0..1
    double outputDb = 0.0;
    bool   bypassed = false;
};

//==============================================================================
// Clipping curves. All have slope 1 at the origin (asymmetric ones are re-centred so silence stays silent).
inline double softKnee (double x, double k) { return x / std::pow (1.0 + std::pow (std::abs (x), k), 1.0 / k); }

inline double clip (Clip type, double x, double bias)
{
    switch (type)
    {
        case Clip::soft:       return std::tanh (x);
        case Clip::asymmetric:
        {
            const double tb = std::tanh (bias);
            return (std::tanh (x + bias) - tb) / (1.0 - tb * tb);
        }
        case Clip::hard:       return softKnee (x, 4.0);
        case Clip::led:        return 1.6 * softKnee (x / 1.6, 3.0);
        case Clip::germanium:  return softKnee (x, 1.4);
        case Clip::mosfet:     return x >= 0.0 ? (1.0 + bias) * softKnee (x / (1.0 + bias), 2.5) : softKnee (x, 4.0);
    }
    return x;
}

inline double driveGainDb (const Model& m, double drive)
{
    return m.minGainDb + (m.maxGainDb - m.minGainDb) * std::pow (std::clamp (drive, 0.0, 1.0), 1.2);
}

inline double levelGain (double level)
{
    level = std::clamp (level, 0.0, 1.0);
    return 4.0 * level * level; // 50 % = unity, 100 % = +12 dB
}

inline double cleanAmount (const Model& m, double drive)
{
    return m.cleanFollowsDrive ? m.clean * (1.0 - 0.7 * std::clamp (drive, 0.0, 1.0)) : m.clean;
}

// Static drive curve (no filters) of a model, for the display: output for an input in -1..1.
inline double staticCurve (const Model& m, double drive, double x)
{
    const double g = melon::dbToGain (driveGainDb (m, drive)) * 0.1; // ~ a guitar's level
    double v = clip (m.clip, g * x, m.bias);
    if (m.stages == 2)
        v = clip (m.clip, kStage2Gain * v, m.bias);
    return v * kClipScale * 2.0 + cleanAmount (m, drive) * x * 0.1;
}

//==============================================================================
// Tone control (+ fixed post filter) of one model
class ToneStage
{
public:
    void set (const Model& m, double fs, double tone)
    {
        tone = std::clamp (tone, 0.0, 1.0);
        if (m.tone == Tone::lowpass)
        {
            const double f = m.toneLoHz * std::pow (m.toneHiHz / m.toneLoHz, tone);
            a.set (melon::Biquad::Type::lowpass, fs, f, 0.6);
            b.setIdentity();
        }
        else
        {
            const double t = 2.0 * tone - 1.0;
            a.set (melon::Biquad::Type::lowShelf, fs, m.tiltHz * 0.5, 0.6, -t * m.tiltDb);
            b.set (melon::Biquad::Type::highShelf, fs, m.tiltHz * 1.5, 0.6, t * m.tiltDb);
        }
        post.set (melon::Biquad::Type::lowpass, fs, m.postLpHz, 0.707);
    }
    double process (double x) { return post.process (b.process (a.process (x))); }
    void   reset() { a.reset(); b.reset(); post.reset(); }
    double responseDb (double f, double fs) const { return a.responseDb (f, fs) + b.responseDb (f, fs) + post.responseDb (f, fs); }

private:
    melon::Biquad a, b, post;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        for (auto* s : smoothers())
            s->prepare (fs, kSmoothingMs);
        fadeStep = 1.0 / std::max (1.0, kModelFadeMs * 0.001 * fs);
        for (auto& c : ch)
            c.os.prepare (fs);
        applyModel (params.model);
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.inHp.reset(); c.hump.reset(); c.tone.reset(); c.os.reset();
            c.preLp.reset(); c.interHp.reset();
        }
    }

    void setParams (const Params& p)
    {
        params = p;
        params.model = std::clamp (p.model, 0, kNumModels - 1);
        drive.setTarget (std::clamp (p.drive, 0.0, 1.0));
        tone.setTarget (std::clamp (p.tone, 0.0, 1.0));
        level.setTarget (levelGain (p.level));
        mix.setTarget (std::clamp (p.mix, 0.0, 1.0));
        outGain.setTarget (melon::dbToGain (p.outputDb));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
    }

    void snapToTargets()
    {
        for (auto* s : smoothers())
            s->snap();
        if (params.model != current)
            applyModel (params.model);
        fade = 1.0;
        updateCoefficients();
    }

    int currentModel() const { return current; }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int nch = std::min (numChannels, channels);

        for (int n = 0; n < numSamples; ++n)
        {
            // model switch: fade the effect out, swap, fade back in (no clicks)
            if (params.model != current)
            {
                fade -= fadeStep;
                if (fade <= 0.0)
                {
                    fade = 0.0;
                    applyModel (params.model);
                    reset();
                }
            }
            else if (fade < 1.0)
                fade = std::min (1.0, fade + fadeStep);

            const double dr = drive.next();
            tone.next();
            const double lv = level.next(), mx = mix.next(), og = outGain.next(), on = active.next();

            if ((n & 15) == 0)
                updateCoefficients();

            const Model& m    = models()[(size_t) current];
            const double gain = melon::dbToGain (driveGainDb (m, dr));
            const double cln  = cleanAmount (m, dr);

            for (int c = 0; c < nch; ++c)
            {
                auto&        st = ch[(size_t) c];
                const double x  = data[c][n];

                const double pre = st.hump.process (st.inHp.process (x));
                double hi[melon::Oversampler4x::kFactor];
                st.os.upsample (pre, hi);
                for (double& v : hi)
                {
                    v = st.preLp.lowpass (v * gain);
                    v = clip (m.clip, v, m.bias);
                    if (m.stages == 2)
                        v = clip (m.clip, kStage2Gain * st.interHp.highpass (v), m.bias);
                }
                const double driven = st.os.downsample (hi) * kClipScale;

                double wet = st.tone.process (driven + cln * x);
                wet *= lv * trim * fade;

                const double effect = ((1.0 - mx) * x + mx * wet) * og;
                data[c][n] = (float) (x + on * (effect - x));
            }
        }
    }

private:
    struct Channel
    {
        melon::Biquad        inHp, hump;
        ToneStage            tone;
        melon::Oversampler4x os;
        melon::OnePole       preLp, interHp; // run at the oversampled rate
    };

    std::vector<melon::Smoother*> smoothers() { return { &drive, &tone, &level, &mix, &outGain, &active }; }

    void applyModel (int idx)
    {
        current        = std::clamp (idx, 0, kNumModels - 1);
        const Model& m = models()[(size_t) current];
        const double fsHi = fs * melon::Oversampler4x::kFactor;
        for (auto& c : ch)
        {
            c.inHp.set (melon::Biquad::Type::highpass, fs, m.inHpHz, 0.707);
            if (std::abs (m.humpDb) > 1e-9)
                c.hump.set (melon::Biquad::Type::peak, fs, m.humpHz, m.humpQ, m.humpDb);
            else
                c.hump.setIdentity();
            c.preLp.setCutoff (fsHi, m.preClipLpHz);
            c.interHp.setCutoff (fsHi, m.stages == 2 ? m.interHpHz : 20.0);
        }
        trim = melon::dbToGain (m.trimDb);
        updateCoefficients();
    }

    void updateCoefficients()
    {
        const Model& m = models()[(size_t) current];
        for (auto& c : ch)
            c.tone.set (m, fs, tone.value());
    }

    double fs = 48000.0, fade = 1.0, fadeStep = 0.01, trim = 1.0;
    int    channels = 2, current = 0;
    Params params;
    std::array<Channel, kMaxChannels> ch;
    melon::Smoother drive, tone, level, mix, outGain, active;
};

} // namespace dirt
