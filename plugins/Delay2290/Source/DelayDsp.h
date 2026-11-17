// DelayDsp.h - Melon 2290 DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// A clean stereo digital delay in the spirit of the TC Electronic 2290 rack unit: long delay
// times, tap tempo / host sync, filters in the feedback loop, delay modulation (from subtle
// chorus to flange), three stereo modes and the 2290's famous "dynamic delay" (ducking: the
// repeats get quieter while you play and swell back up when you stop).
//
//   in ─┬───────────────────────────────────────────────────────────── × dry ─┐
//       └─(+)─► delay (time ± modulation) ─► lo cut ─► hi cut ──┬─ × duck ─ × wet ─(+)─► × output ─► out
//         ▲                                                    │
//         └─────────────── soft limit ◄─── × feedback ◄────────┘
//
// Stereo modes:  Mono       one delay, both sides the same
//                Stereo     left and right delays; the right one is longer by Spread
//                Ping-pong  repeats alternate left / right
#pragma once

#include "MelonDsp.h"
#include <array>

namespace delay2290
{
enum class Mode { mono, stereo, pingPong };

constexpr double kMaxTimeMs    = 5000.0;
constexpr double kMaxSpread    = 0.5;    // right delay up to 50 % longer
constexpr double kMaxModMs     = 4.0;    // modulation depth at 100 %
constexpr double kTimeGlideMs  = 80.0;   // changing the time glides (no clicks)
constexpr double kSmoothingMs  = 20.0;
constexpr double kMaxFeedback  = 1.0;    // 100 % = (almost) endless repeats

struct Params
{
    double timeMs      = 400.0;
    double feedback    = 0.35;   // 0..1
    double mix         = 0.3;    // 0..1 (50 % = dry and wet both at full level)
    double hiCutHz     = 12000.0;
    double loCutHz     = 60.0;
    double spread      = 0.1;    // 0..1 of kMaxSpread
    double modDepth    = 0.0;    // 0..1
    double modSpeedHz  = 0.8;
    double duckThreshDb = -30.0;
    double duckAmountDb = 0.0;   // 0 = off
    double duckReleaseMs = 300.0;
    Mode   mode        = Mode::stereo;
    double outputDb    = 0.0;
    bool   bypassed    = false;
};

inline void mixGains (double mix, double& dry, double& wet)
{
    mix = std::clamp (mix, 0.0, 1.0);
    dry = std::min (1.0, 2.0 * (1.0 - mix));
    wet = std::min (1.0, 2.0 * mix);
}

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

// Ducking gain (dB) for an input envelope level: a 6 dB soft knee around the threshold
inline double duckDb (double envDb, double threshDb, double amountDb)
{
    const double t = std::clamp ((envDb - threshDb) / 6.0 + 0.5, 0.0, 1.0);
    return -std::abs (amountDb) * t;
}

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, 2);
        const int maxSamples = (int) ((kMaxTimeMs * (1.0 + kMaxSpread) + 2.0 * kMaxModMs + 10.0) * 0.001 * fs);
        for (auto& l : lines) l.prepare (maxSamples);
        timeL.prepare (fs, kTimeGlideMs);
        timeR.prepare (fs, kTimeGlideMs);
        for (auto* s : smoothers()) s->prepare (fs, kSmoothingMs);
        updateFilters();
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& f : hiCut) f.reset();
        for (auto& f : loCut) f.reset();
        env = 0.0; duckNow = 0.0; lfo = 0.0;
    }

    void setParams (const Params& p)
    {
        const bool filtersChanged = std::abs (p.hiCutHz - params.hiCutHz) > 1e-6 || std::abs (p.loCutHz - params.loCutHz) > 1e-6;
        params = p;
        if (filtersChanged) updateFilters();

        const double t = std::clamp (p.timeMs, 1.0, kMaxTimeMs) * 0.001 * fs;
        timeL.setTarget (t);
        timeR.setTarget (p.mode == Mode::stereo ? t * (1.0 + kMaxSpread * std::clamp (p.spread, 0.0, 1.0)) : t);
        feedback.setTarget (kMaxFeedback * std::clamp (p.feedback, 0.0, 1.0));
        double d, w;
        mixGains (p.mix, d, w);
        dryGain.setTarget (d);
        wetGain.setTarget (w);
        modDepth.setTarget (kMaxModMs * 0.001 * fs * std::clamp (p.modDepth, 0.0, 1.0));
        outGain.setTarget (melon::dbToGain (p.outputDb));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
    }

    void snapToTargets()
    {
        timeL.snap(); timeR.snap();
        for (auto* s : smoothers()) s->snap();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int    nch      = std::min (numChannels, channels);
        const auto   mode     = params.mode;
        const double envRel   = 1.0 - std::exp (-1.0 / std::max (1.0, std::max (5.0, params.duckReleaseMs) * 0.001 * fs));
        const double duckAtk  = 1.0 - std::exp (-1.0 / (0.005 * fs));
        const double lfoInc   = std::clamp (params.modSpeedHz, 0.01, 20.0) / fs;

        for (int n = 0; n < numSamples; ++n)
        {
            const double tl = timeL.next(), tr = timeR.next();
            const double fb = feedback.next(), dg = dryGain.next(), wg = wetGain.next();
            const double md = modDepth.next(), og = outGain.next(), on = active.next();

            lfo += lfoInc;
            if (lfo >= 1.0) lfo -= 1.0;
            const double mL = md * (1.0 + std::sin (melon::kTwoPi * lfo));
            const double mR = md * (1.0 + std::cos (melon::kTwoPi * lfo)); // 90° apart: wider chorus

            const double inL = data[0][n];
            const double inR = nch > 1 ? data[1][n] : inL;

            // dynamic delay: input envelope -> repeats turned down while playing
            const double lvl = std::max (std::abs (inL), std::abs (inR));
            env = lvl > env ? lvl : env + envRel * (lvl - env);
            const double target = duckDb (melon::gainToDb (env), params.duckThreshDb, params.duckAmountDb);
            duckNow += (target < duckNow ? duckAtk : envRel) * (target - duckNow);
            const double duck = melon::dbToGain (duckNow);

            // read the delays and filter the repeats (the filters sit inside the feedback loop)
            double wL = filter (0, lines[0].read (std::max (1.0, tl + mL)));
            double wR = filter (1, lines[1].read (std::max (1.0, tr + mR)));

            switch (mode)
            {
                case Mode::mono:
                {
                    const double in = 0.5 * (inL + inR);
                    lines[0].push ((float) limit (in + fb * wL));
                    lines[1].push ((float) limit (in + fb * wL)); // kept in step so switching modes is smooth
                    wR = wL;
                    break;
                }
                case Mode::stereo:
                    lines[0].push ((float) limit (inL + fb * wL));
                    lines[1].push ((float) limit (inR + fb * wR));
                    break;
                case Mode::pingPong:
                    // left feeds right feeds left: each bounce is one feedback step (1, fb, fb², ...)
                    lines[0].push ((float) limit (0.5 * (inL + inR) + fb * wR));
                    lines[1].push ((float) limit (fb * wL));
                    break;
            }

            const double outL = (dg * inL + wg * duck * wL) * og;
            const double outR = (dg * inR + wg * duck * wR) * og;
            data[0][n] = (float) (inL + on * (outL - inL));
            if (nch > 1)
                data[1][n] = (float) (inR + on * (outR - inR));
        }
        lastDuck = duckNow;
    }

    double currentTimeMs() const { return timeL.value() / fs * 1000.0; }
    double currentDuckDb() const { return lastDuck; }

private:
    double filter (int c, double x) { return hiCut[(size_t) c].process (loCut[(size_t) c].process (x)); }

    // soft safety limit: transparent below 1.0, keeps 100 % feedback from running away
    static double limit (double v) { return std::abs (v) <= 1.0 ? v : std::copysign (1.0 + std::tanh (std::abs (v) - 1.0), v); }

    void updateFilters()
    {
        for (auto& f : hiCut) f.set (melon::Biquad::Type::lowpass, fs, std::clamp (params.hiCutHz, 500.0, 20000.0), 0.707);
        for (auto& f : loCut) f.set (melon::Biquad::Type::highpass, fs, std::clamp (params.loCutHz, 10.0, 2000.0), 0.707);
    }

    std::vector<melon::Smoother*> smoothers() { return { &feedback, &dryGain, &wetGain, &modDepth, &outGain, &active }; }

    double fs = 48000.0, env = 0.0, duckNow = 0.0, lastDuck = 0.0, lfo = 0.0;
    int    channels = 2;
    Params params;
    std::array<melon::DelayLine, 2> lines;
    std::array<melon::Biquad, 2> hiCut, loCut;
    melon::Smoother timeL, timeR, feedback, dryGain, wetGain, modDepth, outGain, active;
};

} // namespace delay2290
