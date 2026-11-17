// MelonDsp.h - small DSP building blocks shared by the Melon plugins.
// Plain C++17, no JUCE, so every plugin's DSP can be tested on its own.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace melon
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g, double floorDb = -120.0)
{
    return g > 0.0 ? std::max (floorDb, 20.0 * std::log10 (g)) : floorDb;
}

// Dry and wet gains for a 0..1 Mix value: equal-power crossfade.
inline void equalPowerMix (double mix, double& dry, double& wet)
{
    mix = std::clamp (mix, 0.0, 1.0);
    dry = std::cos (0.5 * kPi * mix);
    wet = std::sin (0.5 * kPi * mix);
}

//==============================================================================
// One-pole parameter smoother (reaches ~63 % of a change in `ms`).
class Smoother
{
public:
    void prepare (double sampleRate, double ms)
    {
        coeff = 1.0 - std::exp (-1.0 / std::max (1.0, ms * 0.001 * sampleRate));
    }
    void   setTarget (double t) { target = t; }
    double next() { return current += coeff * (target - current); }
    void   snap() { current = target; }
    double value() const { return current; }
    double getTarget() const { return target; }

private:
    double coeff = 1.0, current = 0.0, target = 0.0;
};

//==============================================================================
// Biquad (RBJ cookbook), transposed direct form II, double precision.
class Biquad
{
public:
    enum class Type { lowpass, highpass, bandpass, peak, lowShelf, highShelf, allpass };

    void set (Type type, double fs, double freq, double q, double gainDb = 0.0)
    {
        freq = std::clamp (freq, 5.0, 0.495 * fs);
        q    = std::max (0.05, q);
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = kTwoPi * freq / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case Type::lowpass:
                b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
            case Type::highpass:
                b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
            case Type::bandpass: // constant 0 dB peak gain
                b0 = alpha; b1 = 0; b2 = -alpha;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
            case Type::peak:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
                break;
            case Type::lowShelf:
            {
                const double s = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + s);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - s);
                a0 = (A + 1) + (A - 1) * cw + s;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - s;
                break;
            }
            case Type::highShelf:
            {
                const double s = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + s);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - s);
                a0 = (A + 1) - (A - 1) * cw + s;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - s;
                break;
            }
            case Type::allpass:
                b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
                break;
        }
        c0 = b0 / a0; c1 = b1 / a0; c2 = b2 / a0; d1 = a1 / a0; d2 = a2 / a0;
    }

    void setIdentity() { c0 = 1; c1 = c2 = d1 = d2 = 0; }

    double process (double x)
    {
        const double y = c0 * x + z1;
        z1 = c1 * x - d1 * y + z2;
        z2 = c2 * x - d2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0.0; }

    // Magnitude response in dB at `freq` (for the editors' displays).
    double responseDb (double freq, double fs) const
    {
        const std::complex<double> z = std::polar (1.0, -kTwoPi * freq / fs);
        const auto num = c0 + c1 * z + c2 * z * z;
        const auto den = 1.0 + d1 * z + d2 * z * z;
        return gainToDb (std::abs (num / den), -200.0);
    }

private:
    double c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0;
    double z1 = 0, z2 = 0;
};

//==============================================================================
// One-pole TPT low-pass / high-pass.
class OnePole
{
public:
    void setCutoff (double fs, double freq)
    {
        const double g = std::tan (kPi * std::clamp (freq, 1.0, 0.49 * fs) / fs);
        a = g / (1.0 + g);
    }
    double lowpass (double x)
    {
        const double v = a * (x - s);
        const double y = v + s;
        s = y + v;
        return y;
    }
    double highpass (double x) { return x - lowpass (x); }
    void   reset() { s = 0.0; }

private:
    double a = 1.0, s = 0.0;
};

//==============================================================================
// Circular delay line with integer and 4-point Hermite reads.
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        size_t n = 1;
        while (n < (size_t) std::max (8, maxSamples + 4))
            n <<= 1;
        buf.assign (n, 0.0f);
        mask = n - 1;
        pos  = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x)
    {
        pos       = (pos + 1) & mask;
        buf[pos]  = x;
    }
    // delay 0 = the sample just pushed
    float readInt (int delay) const { return buf[(pos - (size_t) delay) & mask]; }
    float read (double delay) const
    {
        delay = std::clamp (delay, 1.0, (double) (buf.size() - 4));
        const int    d = (int) delay;
        const double t = delay - d;
        const double xm1 = readInt (d - 1), x0 = readInt (d), x1 = readInt (d + 1), x2 = readInt (d + 2);
        const double c1 = 0.5 * (x1 - xm1);
        const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return (float) (((c3 * t + c2) * t + c1) * t + x0);
    }
    int capacity() const { return (int) buf.size() - 4; }

private:
    std::vector<float> buf { std::vector<float> (8, 0.0f) };
    size_t mask = 7, pos = 0;
};

//==============================================================================
// 4x oversampler for distortion stages: zero-stuffing up, 6th-order Butterworth
// anti-imaging / anti-aliasing low-pass at the oversampled rate on both sides.
class Oversampler4x
{
public:
    static constexpr int kFactor = 4;

    void prepare (double baseRate)
    {
        const double fsHi = baseRate * kFactor;
        const double fc   = std::min (20000.0, 0.42 * baseRate);
        const double qs[3] = { 0.51763809, 0.70710678, 1.93185165 };
        for (int i = 0; i < 3; ++i)
        {
            up[i].set (Biquad::Type::lowpass, fsHi, fc, qs[i]);
            down[i].set (Biquad::Type::lowpass, fsHi, fc, qs[i]);
        }
        reset();
    }
    void reset()
    {
        for (auto& b : up)   b.reset();
        for (auto& b : down) b.reset();
    }
    // Upsamples x into out[4]
    void upsample (double x, double* out)
    {
        for (int k = 0; k < kFactor; ++k)
        {
            double v = (k == 0 ? x * kFactor : 0.0);
            for (auto& b : up)
                v = b.process (v);
            out[k] = v;
        }
    }
    // Filters 4 high-rate samples and returns one base-rate sample
    double downsample (const double* in)
    {
        double y = 0.0;
        for (int k = 0; k < kFactor; ++k)
        {
            double v = in[k];
            for (auto& b : down)
                v = b.process (v);
            if (k == 0)
                y = v;
        }
        return y;
    }

private:
    Biquad up[3], down[3];
};

// Simple xorshift noise (deterministic, real-time safe)
class Noise
{
public:
    double next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (double) s / 2147483648.0 - 1.0;
    }

private:
    uint32_t s = 0x9e3779b9u;
};

} // namespace melon
