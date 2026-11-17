// MuffsDsp.h - Melon Muffs DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// Six versions of the Electro-Harmonix Big Muff Pi fuzz. All Big Muffs share the same four
// stages; the versions differ in gain, bass, clipping hardness and, above all, the tone stack.
// Signal flow, per channel:
//
//   in ─► input HP ─► input boost ─► [ 4x oversampled:  × Sustain ─► clip ─► LP  ─► clip ─► LP ] ─┐
//                                     (two clipping stages, diodes in the feedback loop)            │
//   ┌───────────────────────────────────────────────────────────────────────────────────────────────┘
//   └─► tone stack: (1 - Tone) × low-pass  +  Tone × high-pass ─► recovery LP ─► Volume ─► Mix ─► out
//
// The tone stack is the passive Big Muff network: a first-order low-pass and a first-order
// high-pass blended by the Tone knob. With the corners far apart this makes the famous mid scoop
// at noon; each version uses its own corners.
#pragma once

#include "MelonDsp.h"
#include <array>

namespace muffs
{
struct Model
{
    const char* name;
    double inHpHz;            // input coupling (how much low end reaches the fuzz)
    double bassShelfDb;       // extra low end before the fuzz (the Russian versions)
    double minGainDb, maxGainDb; // Sustain range
    double hardness;          // clipping knee: 1.5 = soft (transistor + diodes) .. 4 = hard (op-amp)
    double bias;              // asymmetry of the clipping
    double stageLpHz;         // low-pass of each clipping stage (feedback capacitor)
    double toneLpHz, toneHpHz;// tone stack corners
    double postLpHz;          // recovery stage low-pass
    double trimDb;            // level matching between versions
};

inline const std::array<Model, 6>& models()
{
    static const std::array<Model, 6> m { {
        //  name                       inHP  bass  gain lo/hi  hard  bias  stageLP  toneLP toneHP postLP  trim
        { "Triangle",                   40,  0.0,  10, 40,    1.8,  0.10,  4500,    450,   1200,  7000,   -13.5 },
        { "Ram's Head",                 60,  0.0,   6, 34,    1.8,  0.15,  5000,    600,   1000,  7500,   -13.1 },
        { "Op-Amp",                     80,  0.0,  12, 46,    3.5,  0.00,  7000,    380,   1600,  9000,   -15.0 },
        { "Russian Civil War",          30,  3.0,  10, 38,    1.6,  0.10,  3500,    350,   1300,  6000,   -11.9 },
        { "NYC",                        50,  0.0,  10, 42,    1.8,  0.10,  4500,    410,   1800,  7000,   -12.5 },
        { "Big Muff Pi 2",             120,  0.0,  14, 48,    2.2,  0.05,  5500,    450,   1400,  8000,   -14.9 },
    } };
    return m;
}

constexpr int    kNumModels   = 6;
constexpr double kInputBoost  = 2.0;   // the input transistor stage
constexpr double kStageGain   = 3.0;   // gain of the second clipping stage
constexpr double kFuzzLevel   = 0.5;   // level after the clipping stages
constexpr double kSmoothingMs = 20.0;
constexpr double kModelFadeMs = 8.0;
constexpr int    kMaxChannels = 2;

struct Params
{
    int    model    = 0;
    double sustain  = 0.5; // 0..1
    double tone     = 0.5; // 0..1 (0 = bass, 1 = treble)
    double volume   = 0.5; // 0..1 (0.5 = unity)
    double mix      = 1.0; // 0..1
    double outputDb = 0.0;
    bool   bypassed = false;
};

inline double clip (const Model& m, double x)
{
    const double y = (x + m.bias) / std::pow (1.0 + std::pow (std::abs (x + m.bias), m.hardness), 1.0 / m.hardness);
    const double b = m.bias / std::pow (1.0 + std::pow (std::abs (m.bias), m.hardness), 1.0 / m.hardness);
    return y - b;
}

inline double sustainGainDb (const Model& m, double s)
{
    return m.minGainDb + (m.maxGainDb - m.minGainDb) * std::clamp (s, 0.0, 1.0);
}

inline double volumeGain (double v)
{
    v = std::clamp (v, 0.0, 1.0);
    return 4.0 * v * v;
}

// Tone stack response (for the display): (1 - t) / (1 + j f/fl) + t (j f/fh) / (1 + j f/fh)
inline double toneStackDb (const Model& m, double tone, double f)
{
    const std::complex<double> j (0.0, 1.0);
    const auto lp = 1.0 / (1.0 + j * f / m.toneLpHz);
    const auto hp = (j * f / m.toneHpHz) / (1.0 + j * f / m.toneHpHz);
    return melon::gainToDb (std::abs ((1.0 - tone) * lp + tone * hp), -200.0);
}

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
            c.inHp.reset(); c.bass.reset(); c.os.reset(); c.lp1.reset(); c.lp2.reset();
            c.toneLp.reset(); c.toneHp.reset(); c.post.reset(); c.dcBlock.reset();
        }
    }

    void setParams (const Params& p)
    {
        params       = p;
        params.model = std::clamp (p.model, 0, kNumModels - 1);
        sustain.setTarget (std::clamp (p.sustain, 0.0, 1.0));
        tone.setTarget (std::clamp (p.tone, 0.0, 1.0));
        volume.setTarget (volumeGain (p.volume));
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
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int nch = std::min (numChannels, channels);

        for (int n = 0; n < numSamples; ++n)
        {
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

            const Model& m  = models()[(size_t) current];
            const double g  = melon::dbToGain (sustainGainDb (m, sustain.next()));
            const double t  = tone.next();
            const double vo = volume.next(), mx = mix.next(), og = outGain.next(), on = active.next();

            for (int c = 0; c < nch; ++c)
            {
                auto&        st = ch[(size_t) c];
                const double x  = data[c][n];

                const double in = std::tanh (kInputBoost * st.bass.process (st.inHp.process (x)));
                double hi[melon::Oversampler4x::kFactor];
                st.os.upsample (in, hi);
                for (double& v : hi)
                {
                    v = st.lp1.lowpass (clip (m, g * v));
                    v = st.lp2.lowpass (clip (m, kStageGain * v));
                }
                const double fuzz = st.dcBlock.highpass (st.os.downsample (hi)) * kFuzzLevel;

                const double stack = (1.0 - t) * st.toneLp.lowpass (fuzz) + t * st.toneHp.highpass (fuzz);
                // the stack loses level at its scoop; make up a little so Tone at noon isn't quieter
                const double wet = st.post.process (stack * (1.0 + 2.4 * t * (1.0 - t))) * vo * trim * fade;

                const double effect = ((1.0 - mx) * x + mx * wet) * og;
                data[c][n] = (float) (x + on * (effect - x));
            }
        }
    }

private:
    struct Channel
    {
        melon::Biquad        inHp, bass, post;
        melon::Oversampler4x os;
        melon::OnePole       lp1, lp2;             // oversampled rate
        melon::OnePole       toneLp, toneHp, dcBlock;
    };

    std::vector<melon::Smoother*> smoothers() { return { &sustain, &tone, &volume, &mix, &outGain, &active }; }

    void applyModel (int idx)
    {
        current        = std::clamp (idx, 0, kNumModels - 1);
        const Model& m = models()[(size_t) current];
        const double fsHi = fs * melon::Oversampler4x::kFactor;
        for (auto& c : ch)
        {
            c.inHp.set (melon::Biquad::Type::highpass, fs, m.inHpHz, 0.707);
            if (m.bassShelfDb > 0.0)
                c.bass.set (melon::Biquad::Type::lowShelf, fs, 120.0, 0.7, m.bassShelfDb);
            else
                c.bass.setIdentity();
            c.lp1.setCutoff (fsHi, m.stageLpHz);
            c.lp2.setCutoff (fsHi, m.stageLpHz);
            c.toneLp.setCutoff (fs, m.toneLpHz);
            c.toneHp.setCutoff (fs, m.toneHpHz);
            c.dcBlock.setCutoff (fs, 15.0);
            c.post.set (melon::Biquad::Type::lowpass, fs, m.postLpHz, 0.707);
        }
        trim = melon::dbToGain (m.trimDb);
    }

    double fs = 48000.0, fade = 1.0, fadeStep = 0.01, trim = 1.0;
    int    channels = 2, current = 0;
    Params params;
    std::array<Channel, kMaxChannels> ch;
    melon::Smoother sustain, tone, volume, mix, outGain, active;
};

} // namespace muffs
