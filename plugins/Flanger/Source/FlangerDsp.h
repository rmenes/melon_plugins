// FlangerDsp.h - Melon Flanger DSP core, in plain C++ (no JUCE) so it can be tested on its own.
//
// Modelled on the controls of the classic MXR M117R flanger pedal (Manual, Width, Speed, Regen),
// plus a Mix control. Signal flow, per channel:
//
//   in ──┬─────────────────────────────────────────────────── × dry ──┐
//        │                                                             (+) ── × level comp ── × output ── out
//        └─(+)─► short delay ─► BBD-style low-pass ──┬──────── × wet ──┘
//          ▲          ▲                              │
//          │   triangle LFO (Speed), sweep           │
//          │   centred on Manual, span = Width       │
//          └──────── soft clip ◄──── × Regen ◄───────┘
//
// Mixing the input with a copy delayed by a few milliseconds makes a comb filter: a series of
// notches spread across the spectrum. Sweeping the delay moves the notches up and down, which
// gives the "jet plane" whoosh. Regen feeds the delayed signal back into the delay, turning the
// peaks between the notches into resonances (the metallic, ringing flanger sound).
//
// The sweep is done in octaves (the delay is multiplied/divided, not shifted), so the notches
// move evenly across the spectrum, like the clock sweep of the pedal's bucket-brigade delay.
// Both channels share one LFO, like the mono pedal; the plugin still runs in stereo.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace flanger
{
constexpr double kPi = 3.14159265358979323846;

// --- Tuning ------------------------------------------------------------------
constexpr double kMinManualMs   = 0.25;  // Manual knob range (centre of the sweep)
constexpr double kMaxManualMs   = 10.0;
constexpr double kMaxSweepOct   = 1.5;   // ± octaves of delay sweep at 100 % Width (an 8:1 range)
constexpr double kMinDelayMs    = 0.1;   // the sweep never goes below / above these
constexpr double kMaxDelayMs    = 30.0;
constexpr double kMinSpeedHz    = 0.05;
constexpr double kMaxSpeedHz    = 10.0;
constexpr double kMaxFeedback   = 0.95;  // Regen 100 %
constexpr double kWetLowpassHz  = 10000.0; // the pedal's delay chip doesn't pass the very top end
constexpr double kSmoothingMs   = 30.0;
constexpr int    kMaxChannels   = 2;

struct Params
{
    double manualMs = 2.0;  // centre of the sweep (sets where the notches sit)
    double width    = 0.6;  // 0..1, how far the delay sweeps (0 = static "manual" flange)
    double speedHz  = 0.4;  // LFO speed
    double regen    = 0.5;  // 0..1, feedback amount
    double mix      = 0.5;  // 0..1, 0 = dry, 0.5 = dry and flanged both at full level (deepest notches), 1 = wet only
    double outputDb = 0.0;
    bool   bypassed = false;
};

// Dry and wet gains for a Mix value (same law as Melon Chorus). At 50 % both are at full level,
// which gives the deepest notches, as on the pedal.
inline void mixGains (double mix, double& dry, double& wet)
{
    mix = std::clamp (mix, 0.0, 1.0);
    dry = std::min (1.0, 2.0 * (1.0 - mix));
    wet = std::min (1.0, 2.0 * mix);
}

inline double feedbackFor (double regen) { return kMaxFeedback * std::clamp (regen, 0.0, 1.0); }

// Regen makes the effect louder (the resonances add energy). This keeps the overall level roughly
// steady: it is applied in proportion to the wet level, so Mix 0 % still passes the dry signal untouched.
inline double levelCompensation (double feedback, double wetGain)
{
    const double c = 1.0 / (1.0 + feedback);
    return 1.0 + std::clamp (wetGain, 0.0, 1.0) * (c - 1.0);
}

// Triangle LFO in -1..1 for a phase in 0..1 (starts at the bottom of the sweep).
inline double triangle (double phase)
{
    phase -= std::floor (phase);
    return 4.0 * std::abs (phase - 0.5) - 1.0;
}

// Delay in milliseconds for an LFO value in -1..1. LFO = +1 is the longest delay (lowest notches).
inline double delayMs (double lfo, double manualMs, double width)
{
    const double oct = kMaxSweepOct * std::clamp (width, 0.0, 1.0) * std::clamp (lfo, -1.0, 1.0);
    return std::clamp (std::clamp (manualMs, kMinManualMs, kMaxManualMs) * std::exp2 (oct), kMinDelayMs, kMaxDelayMs);
}

// Frequency of the first (lowest) notch for a delay, with Regen 0 and Mix 50 %.
inline double firstNotchHz (double delayMs) { return 500.0 / delayMs; }

// One-pole low-pass coefficient (bilinear / TPT form) for the wet path.
inline double lowpassCoeff (double fs)
{
    const double g = std::tan (kPi * std::min (kWetLowpassHz, 0.45 * fs) / fs);
    return g / (1.0 + g);
}

// Small-signal magnitude response (dB) of the whole effect at one frequency, for the display.
// Ignores the soft clip, which only matters at high levels.
inline double responseDb (double freqHz, double fs, const Params& p, double delayNowMs)
{
    const double w  = 2.0 * kPi * freqHz / fs;
    const auto   z1 = std::polar (1.0, -w);                         // z^-1
    const double a  = lowpassCoeff (fs);
    const auto   lp = a * (1.0 + z1) / (1.0 - (1.0 - 2.0 * a) * z1); // the TPT one-pole low-pass
    const auto   d  = std::polar (1.0, -w * delayNowMs * 0.001 * fs) * lp; // delay + low-pass
    const double g  = feedbackFor (p.regen);

    double dry, wet;
    mixGains (p.mix, dry, wet);
    const auto h = (dry + wet * d / (1.0 - g * d)) * levelCompensation (g, wet);
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

//==============================================================================
class DelayLine
{
public:
    void prepare (double fs, double maxDelayMs)
    {
        const int needed = (int) std::ceil (maxDelayMs * 0.001 * fs) + 8;
        int size = 1;
        while (size < needed)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask  = size - 1;
        write = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x)
    {
        buffer[(size_t) write] = x;
        write = (write + 1) & mask;
    }

    // Reads the signal 'delay' samples ago, BEFORE the current sample is pushed
    // (so read (1) is the previous input). Needs delay >= 3 for the 4-point Hermite interpolation.
    float read (double delay) const
    {
        const double pos  = (double) write - delay;
        const double base = std::floor (pos);
        const float  t    = (float) (pos - base);
        const int    i    = (int) base;

        const float y0 = buffer[(size_t) ((i - 1) & mask)];
        const float y1 = buffer[(size_t) (i & mask)];
        const float y2 = buffer[(size_t) ((i + 1) & mask)];
        const float y3 = buffer[(size_t) ((i + 2) & mask)];

        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

private:
    std::vector<float> buffer;
    int mask = 0, write = 0;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        lpCoeff  = lowpassCoeff (fs);

        for (auto& d : delays)
            d.prepare (fs, kMaxDelayMs + 1.0);
        for (auto* s : allSmoothers())
            s->prepare (fs, kSmoothingMs);
        reset();
    }

    void reset()
    {
        for (auto& d : delays)
            d.reset();
        for (auto& z : lpState)
            z = 0.0;
        phase    = 0.0;
        delayNow = 0.0;
    }

    void setParams (const Params& p)
    {
        logManual.setTarget (std::log2 (std::clamp (p.manualMs, kMinManualMs, kMaxManualMs)));
        width.setTarget (std::clamp (p.width, 0.0, 1.0));
        speed.setTarget (std::clamp (p.speedHz, kMinSpeedHz, kMaxSpeedHz));

        const double g = feedbackFor (p.regen);
        feedback.setTarget (g);

        double d, w;
        mixGains (p.mix, d, w);
        dryGain.setTarget (d);
        wetGain.setTarget (w);
        comp.setTarget (levelCompensation (g, w));

        outGain.setTarget (std::pow (10.0, p.outputDb / 20.0));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
    }

    void snapToTargets()
    {
        for (auto* s : allSmoothers())
            s->snap();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int nch = std::min (numChannels, channels);

        for (int n = 0; n < numSamples; ++n)
        {
            const double man = std::exp2 (logManual.next());
            const double wd  = width.next();
            const double sp  = speed.next();
            const double g   = feedback.next();
            const double dg  = dryGain.next();
            const double wg  = wetGain.next();
            const double cp  = comp.next();
            const double og  = outGain.next();
            const double on  = active.next();

            const double ms    = delayMs (triangle (phase), man, wd);
            const double delay = std::max (3.0, ms * 0.001 * fs);
            delayNow           = ms;

            for (int ch = 0; ch < nch; ++ch)
            {
                const float x = data[ch][n];
                auto&       line = delays[(size_t) ch];

                // delayed signal through the wet-path low-pass (TPT one-pole)
                auto&        s  = lpState[(size_t) ch];
                const double v  = lpCoeff * ((double) line.read (delay) - s);
                const double wet = v + s;
                s = wet + v;

                // feedback with a gentle soft clip, like an overdriven delay chip; keeps high Regen bounded
                line.push ((float) (x + g * std::tanh (wet)));

                const double effect = (dg * x + wg * wet) * cp * og;
                data[ch][n] = (float) (x + on * (effect - x)); // bypass crossfade
            }

            phase += sp / fs;
            if (phase >= 1.0)
                phase -= 1.0;
        }
    }

    double lfoPhase() const { return phase; }
    double currentDelayMs() const { return delayNow; }

private:
    std::vector<Smoother*> allSmoothers()
    {
        return { &logManual, &width, &speed, &feedback, &dryGain, &wetGain, &comp, &outGain, &active };
    }

    double fs = 48000.0, lpCoeff = 0.0, phase = 0.0, delayNow = 0.0;
    int channels = 2;
    DelayLine delays[kMaxChannels];
    double lpState[kMaxChannels] {};
    Smoother logManual, width, speed, feedback, dryGain, wetGain, comp, outGain, active;
};

} // namespace flanger
