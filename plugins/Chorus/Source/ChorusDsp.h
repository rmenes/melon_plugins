// ChorusDsp.h - Melon Chorus DSP core, in plain C++ (no JUCE) so it can be tested on its own.
//
// Signal flow, per channel:
//
//   in ──┬──────────────────────────────────────── × dry ──┐
//        │                                                  (+) ── × output ── out
//        └─► modulated delay ─► Tone (shelf) ───── × wet ──┘
//                 ▲
//          sine LFO (Rate, Depth); the right channel's LFO is offset by Width × 180°
//
// The delay sweeps around kBaseDelayMs. The continuously changing delay detunes the
// copy slightly, and mixing it with the dry signal gives the classic chorus shimmer.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace chorus
{
constexpr double kPi = 3.14159265358979323846;

// --- Tuning ------------------------------------------------------------------
constexpr double kBaseDelayMs  = 7.0;  // centre of the sweep (typical of analog BBD chorus pedals)
constexpr double kMaxSweepMs   = 3.0;  // ± sweep at 100 % depth  → delay moves between 4 and 10 ms
constexpr double kMinRateHz    = 0.1;
constexpr double kMaxRateHz    = 8.0;
constexpr double kToneSplitHz  = 1500.0; // Tone shelves the chorus signal above this frequency
constexpr double kToneRangeDb  = 10.0;   // highs at -10 dB (full Dark) .. +10 dB (full Bright)
constexpr double kSmoothingMs  = 30.0;  // parameter smoothing (no zipper noise or clicks)
constexpr int    kMaxChannels  = 2;

struct Params
{
    double rateHz   = 0.8;  // LFO speed
    double depth    = 0.5;  // 0..1, amount of delay sweep
    double width    = 0.7;  // 0..1, L/R LFO phase offset (0 = mono, 1 = 180°)
    double mix      = 0.5;  // 0..1, 0 = dry only, 0.5 = dry and wet both at full level, 1 = wet only
    double tone     = 0.0;  // -1..1, -1 = darkest, 0 = flat, +1 = brightest (applies to the chorus signal)
    double outputDb = 0.0;
    bool   bypassed = false;
};

// Dry and wet gains for a Mix value. At 50 % both are at full level, like the
// effect-level knob of a chorus pedal; past that the dry signal fades out.
inline void mixGains (double mix, double& dry, double& wet)
{
    mix = std::clamp (mix, 0.0, 1.0);
    dry = std::min (1.0, 2.0 * (1.0 - mix));
    wet = std::min (1.0, 2.0 * mix);
}

// Gain applied to the highs of the chorus signal for a Tone value (the lows stay as they are,
// like the tone knob on a guitar or pedal).
inline double toneHighGain (double tone)
{
    return std::pow (10.0, std::clamp (tone, -1.0, 1.0) * kToneRangeDb / 20.0);
}

// Delay in milliseconds for an LFO value in -1..1.
inline double delayMs (double lfo, double depth)
{
    return kBaseDelayMs + kMaxSweepMs * std::clamp (depth, 0.0, 1.0) * lfo;
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
        const int needed = (int) std::ceil (maxDelayMs * 0.001 * fs) + 4;
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

    // Reads the signal 'delay' samples ago (1 = the sample just pushed).
    // 4-point Hermite interpolation: smooth enough for a moving read position without dulling the highs.
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
        const double g = std::tan (kPi * std::min (kToneSplitHz, 0.45 * fs) / fs);
        toneCoeff = g / (1.0 + g); // bilinear (TPT) one-pole, so the shelf reaches its full depth

        for (auto& d : delays)
            d.prepare (fs, kBaseDelayMs + kMaxSweepMs + 1.0);
        for (auto* s : allSmoothers())
            s->prepare (fs, kSmoothingMs);
        reset();
    }

    void reset()
    {
        for (auto& d : delays)
            d.reset();
        for (auto& z : toneState)
            z = 0.0;
        phase = 0.0;
    }

    void setParams (const Params& p)
    {
        rate.setTarget (std::clamp (p.rateHz, kMinRateHz, kMaxRateHz));
        depth.setTarget (std::clamp (p.depth, 0.0, 1.0));
        width.setTarget (std::clamp (p.width, 0.0, 1.0));

        double d, w;
        mixGains (p.mix, d, w);
        dryGain.setTarget (d);
        wetGain.setTarget (w);

        toneHigh.setTarget (toneHighGain (p.tone));

        outGain.setTarget (std::pow (10.0, p.outputDb / 20.0));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
    }

    // Jumps straight to the targets (call after prepare(), before the first block).
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
            const double r  = rate.next();
            const double dp = depth.next();
            const double wd = width.next();
            const double dg = dryGain.next();
            const double wg = wetGain.next();
            const double hi = toneHigh.next();
            const double og = outGain.next();
            const double on = active.next();

            for (int ch = 0; ch < nch; ++ch)
            {
                const float x = data[ch][n];
                delays[(size_t) ch].push (x);

                const double lfo   = std::sin (2.0 * kPi * (phase + (ch == 1 ? 0.5 * wd : 0.0)));
                const double delay = delayMs (lfo, dp) * 0.001 * fs;
                double wet         = delays[(size_t) ch].read (delay);

                // Tone: first-order high shelf (one-pole split into lows and highs; only the highs change)
                auto&        s  = toneState[(size_t) ch];
                const double v  = toneCoeff * (wet - s);
                const double lp = v + s;
                s   = lp + v;
                wet = lp + hi * (wet - lp);

                const double effect = (dg * x + wg * wet) * og;
                data[ch][n] = (float) (x + on * (effect - x)); // bypass crossfade
            }

            phase += r / fs;
            if (phase >= 1.0)
                phase -= 1.0;
        }
    }

    double lfoPhase() const { return phase; }

private:
    std::vector<Smoother*> allSmoothers()
    {
        return { &rate, &depth, &width, &dryGain, &wetGain, &toneHigh, &outGain, &active };
    }

    double fs = 48000.0, toneCoeff = 0.0, phase = 0.0;
    int channels = 2;
    DelayLine delays[kMaxChannels];
    double toneState[kMaxChannels] {};
    Smoother rate, depth, width, dryGain, wetGain, toneHigh, outGain, active;
};

} // namespace chorus
