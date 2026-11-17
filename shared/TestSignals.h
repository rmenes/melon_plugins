// TestSignals.h - test signals and measurements shared by the Melon DSP tests (plain C++).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace testsig
{
constexpr double kPi = 3.14159265358979323846;

inline std::vector<float> sine (double fs, double freq, double amp, double seconds)
{
    std::vector<float> v ((size_t) (fs * seconds));
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = (float) (amp * std::sin (2.0 * kPi * freq * (double) i / fs));
    return v;
}

inline std::vector<float> silence (double fs, double seconds) { return std::vector<float> ((size_t) (fs * seconds), 0.0f); }

// Karplus-Strong plucked strings: a few strummed chords, peaks around 0.3 like a DI guitar.
inline std::vector<float> pluckedChords (double fs, double seconds)
{
    std::vector<float> out ((size_t) (fs * seconds), 0.0f);
    const double chords[3][4] = { { 82.4, 123.5, 164.8, 207.7 }, { 110.0, 164.8, 220.0, 277.2 }, { 98.0, 146.8, 196.0, 246.9 } };
    unsigned     seed = 12345;
    const size_t chordLen = out.size() / 3;
    for (int c = 0; c < 3; ++c)
        for (int s = 0; s < 4; ++s)
        {
            const int    len = std::max (2, (int) (fs / chords[c][s]));
            std::vector<double> line ((size_t) len);
            for (auto& v : line)
            {
                seed = seed * 1664525u + 1013904223u;
                v    = ((double) (seed >> 9) / 4194304.0 - 1.0) * 0.09;
            }
            const size_t start = (size_t) c * chordLen + (size_t) (s * 0.012 * fs);
            size_t idx = 0;
            for (size_t i = start; i < std::min (out.size(), (size_t) (c + 1) * chordLen); ++i)
            {
                const size_t j    = idx % (size_t) len;
                const size_t k    = (idx + 1) % (size_t) len;
                const double next = 0.996 * 0.5 * (line[j] + line[k]);
                out[i] += (float) line[j];
                line[j] = next;
                ++idx;
            }
        }
    return out;
}

inline double rms (const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > v.size()) to = v.size();
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return to > from ? std::sqrt (s / (double) (to - from)) : 0.0;
}
inline double rmsDb (const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    const double r = rms (v, from, to);
    return r > 0.0 ? 20.0 * std::log10 (r) : -200.0;
}
inline double peak (const std::vector<float>& v)
{
    double m = 0.0;
    for (float x : v) m = std::max (m, (double) std::abs (x));
    return m;
}
inline double crestDb (const std::vector<float>& v) { return 20.0 * std::log10 (peak (v) / std::max (1e-12, rms (v))); }
inline double maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double m = 0.0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i) m = std::max (m, (double) std::abs (a[i] - b[i]));
    return m;
}
inline bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}
inline std::vector<float> scaled (std::vector<float> v, float g)
{
    for (auto& x : v) x *= g;
    return v;
}
// Ratio of the energy of the first difference to the signal energy (higher = brighter)
inline double brightness (const std::vector<float>& v)
{
    double d = 0.0, s = 0.0;
    for (size_t i = 1; i < v.size(); ++i) { const double e = v[i] - v[i - 1]; d += e * e; s += (double) v[i] * v[i]; }
    return s > 0.0 ? d / s : 0.0;
}
} // namespace testsig
