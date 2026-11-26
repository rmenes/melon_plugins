// Melon Monolithic Synthesis - basic filter building blocks
// Framework-free C++17, header-only.
#pragma once

#include <cmath>

namespace melon::mono
{
constexpr double kPi = 3.14159265358979323846;

// RBJ biquad (transposed direct form II)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void setLowpass (double sr, double fc, double q)
    {
        const double w = 2.0 * kPi * fc / sr, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (float) ((1.0 - c) * 0.5 / a0);
        b1 = (float) ((1.0 - c) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * c / a0);
        a2 = (float) ((1.0 - al) / a0);
    }

    void setHighpass (double sr, double fc, double q)
    {
        const double w = 2.0 * kPi * fc / sr, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (float) ((1.0 + c) * 0.5 / a0);
        b1 = (float) (-(1.0 + c) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * c / a0);
        a2 = (float) ((1.0 - al) / a0);
    }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0f; }
};

// Topology-preserving state variable filter (Zavalishin / Simper)
struct SVF
{
    float ic1 = 0, ic2 = 0;

    struct Out { float low, band, high; };

    Out process (float v0, float g, float k) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

// Zero-delay-feedback 4-pole ladder with input saturation
struct Ladder
{
    float s[4] {};

    // g = tan(pi fc / sr), k = 0..~3.9 resonance feedback, drive >= 1
    float process (float x, float g, float k, float drive) noexcept
    {
        const float G = g / (1.0f + g);
        const float oneMinusG = 1.0f - G;
        const float b1 = oneMinusG * s[0], b2 = oneMinusG * s[1], b3 = oneMinusG * s[2], b4 = oneMinusG * s[3];
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = G3 * b1 + G2 * b2 + G * b3 + b4;

        float u = (x * drive - k * S) / (1.0f + k * G4);
        u = std::tanh (u);
        // passband gain compensation for resonance
        u *= (1.0f + 0.5f * k) / drive;

        float in = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (in - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            in = y;
        }
        return in;
    }

    void reset() noexcept { for (auto& v : s) v = 0.0f; }
};

inline float flushDenormal (float x) noexcept { return std::fabs (x) < 1.0e-15f ? 0.0f : x; }

} // namespace melon::mono
