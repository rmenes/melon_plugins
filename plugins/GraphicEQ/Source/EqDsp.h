// EqDsp.h - graphic equalizer DSP core (plain C++17, no JUCE dependency)
//
// - ISO octave bands (10 bands) or third-octave bands (31 bands)
// - Peaking biquad filters (RBJ / Audio EQ Cookbook formulas)
// - "Precise" mode: compensates for the interaction between neighbouring bands so the
//   resulting curve passes through the slider values (interaction-matrix method,
//   inspired by Liski & Valimaki, "The Quest for the Best Graphic Equalizer", 2017)
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>

namespace geq
{
constexpr int    kMaxBands          = 31;
constexpr double kPi                = 3.14159265358979323846;
constexpr double kSliderRangeDb     = 12.0;  // slider range: +/-12 dB
constexpr double kMaxFilterGainDb   = 24.0;  // internal filter gain limit in precise mode
constexpr double kPrototypeGainDb   = 12.0;  // gain used to measure band interaction

inline const std::vector<double>& bandFrequencies (int numBands)
{
    static const std::vector<double> f10 { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    static const std::vector<double> f31 { 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630,
                                           800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000,
                                           12500, 16000, 20000 };
    return numBands == 31 ? f31 : f10;
}

inline std::string frequencyLabel (double f)
{
    char buf[16];
    if (f < 1000.0)
        std::snprintf (buf, sizeof (buf), "%g", f);
    else
        std::snprintf (buf, sizeof (buf), "%gk", f / 1000.0);
    return buf;
}

// Q of a peaking filter with a bandwidth of "bw" octaves
inline double qFromBandwidth (double bwOctaves)
{
    const double p = std::pow (2.0, bwOctaves);
    return std::sqrt (p) / (p - 1.0);
}

inline double bandQ (int numBands) { return qFromBandwidth (numBands == 31 ? 1.0 / 3.0 : 1.0); }

struct Coeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

inline Coeffs peaking (double freq, double q, double gainDb, double fs)
{
    freq = std::min (freq, 0.45 * fs);
    const double A     = std::pow (10.0, gainDb / 40.0);
    const double w0    = 2.0 * kPi * freq / fs;
    const double cw    = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0    = 1.0 + alpha / A;

    Coeffs c;
    c.b0 = (1.0 + alpha * A) / a0;
    c.b1 = (-2.0 * cw) / a0;
    c.b2 = (1.0 - alpha * A) / a0;
    c.a1 = (-2.0 * cw) / a0;
    c.a2 = (1.0 - alpha / A) / a0;
    return c;
}

inline double magnitudeDb (const Coeffs& c, double freq, double fs)
{
    const double w  = 2.0 * kPi * freq / fs;
    const auto   z1 = std::polar (1.0, -w);
    const auto   z2 = z1 * z1;
    const auto   h  = (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
    return 20.0 * std::log10 (std::max (std::abs (h), 1.0e-12));
}

//==============================================================================
// Filter-bank design for a given number of bands and sample rate.
// Does not allocate after configure(), so it is safe to use on the audio thread.
class Design
{
public:
    void configure (int numBands, double sampleRate)
    {
        n     = numBands == 31 ? 31 : 10;
        fs    = sampleRate;
        q     = bandQ (n);
        freqs = bandFrequencies (n);
        buildInverseInteractionMatrix();
    }

    int    numBands() const { return n; }
    double sampleRate() const { return fs; }
    double frequency (int i) const { return freqs[(size_t) i]; }
    double getQ() const { return q; }

    // Converts slider values (dB) into the actual gain of each filter (dB)
    void filterGains (const double* sliderDb, bool precise, double* out) const
    {
        for (int i = 0; i < n; ++i)
        {
            if (! precise)
            {
                out[i] = sliderDb[i];
                continue;
            }

            double g = 0.0;
            for (int j = 0; j < n; ++j)
                g += inv[(size_t) i][(size_t) j] * sliderDb[j];

            out[i] = std::clamp (g, -kMaxFilterGainDb, kMaxFilterGainDb);
        }
    }

    // Total response (dB) of the filter bank at a given frequency
    double responseDb (const double* filterGainsDb, double freq) const
    {
        double sum = 0.0;
        for (int m = 0; m < n; ++m)
            if (std::abs (filterGainsDb[m]) > 1.0e-6)
                sum += magnitudeDb (peaking (freqs[(size_t) m], q, filterGainsDb[m], fs), freq, fs);
        return sum;
    }

private:
    void buildInverseInteractionMatrix()
    {
        // B[k][m] = normalised response of filter m (at the prototype gain) at centre frequency k
        std::array<std::array<double, 2 * kMaxBands>, kMaxBands> a {};
        for (int k = 0; k < n; ++k)
        {
            for (int m = 0; m < n; ++m)
            {
                const auto c = peaking (freqs[(size_t) m], q, kPrototypeGainDb, fs);
                a[(size_t) k][(size_t) m] = magnitudeDb (c, freqs[(size_t) k], fs) / kPrototypeGainDb;
            }
            for (int m = 0; m < n; ++m)
                a[(size_t) k][(size_t) (n + m)] = (k == m) ? 1.0 : 0.0;
        }

        // Gauss-Jordan elimination with partial pivoting
        for (int col = 0; col < n; ++col)
        {
            int pivot = col;
            for (int r = col + 1; r < n; ++r)
                if (std::abs (a[(size_t) r][(size_t) col]) > std::abs (a[(size_t) pivot][(size_t) col]))
                    pivot = r;
            std::swap (a[(size_t) col], a[(size_t) pivot]);

            const double d = a[(size_t) col][(size_t) col];
            for (int c = 0; c < 2 * n; ++c)
                a[(size_t) col][(size_t) c] /= d;

            for (int r = 0; r < n; ++r)
            {
                if (r == col)
                    continue;
                const double f = a[(size_t) r][(size_t) col];
                if (f != 0.0)
                    for (int c = 0; c < 2 * n; ++c)
                        a[(size_t) r][(size_t) c] -= f * a[(size_t) col][(size_t) c];
            }
        }

        for (int r = 0; r < n; ++r)
            for (int c = 0; c < n; ++c)
                inv[(size_t) r][(size_t) c] = a[(size_t) r][(size_t) (n + c)];
    }

    int                 n  = 10;
    double              fs = 44100.0;
    double              q  = 1.414;
    std::vector<double> freqs;
    std::array<std::array<double, kMaxBands>, kMaxBands> inv {};
};

//==============================================================================
// Processing engine: cascade of biquads with gain smoothing
// and click-free mode switching (10 <-> 31 bands).
class Engine
{
public:
    void prepare (double sampleRate, int numChannels, int initialBands)
    {
        fs = sampleRate;
        designs[0].configure (10, fs);
        designs[1].configure (31, fs);
        states.assign ((size_t) std::max (1, numChannels), {});
        active = pending = (initialBands == 31 ? 31 : 10);
        smoothK = 1.0 - std::exp (-(double) kSubBlock / (0.015 * fs));        // ~15 ms
        outK    = 1.0 - std::exp (-1.0 / (0.015 * fs));
        cur.fill (0.0);
        tgt.fill (0.0);
        for (int m = 0; m < kMaxBands; ++m)
            coeffs[(size_t) m] = Coeffs {};
        outGain = outTarget = 1.0;
    }

    void reset()
    {
        for (auto& ch : states)
            for (auto& s : ch)
                s = {};
    }

    // Called every block with the current slider values
    void setTargets (int numBands, const double* sliderDb, bool precise)
    {
        pending = (numBands == 31 ? 31 : 10);

        if (pending != active)
        {
            // first fade the current bank to flat; the switch happens in process()
            tgt.fill (0.0);
            return;
        }

        design (active).filterGains (sliderDb, precise, tgt.data());
    }

    void setOutputGainDb (double db) { outTarget = std::pow (10.0, db / 20.0); }

    // Skips smoothing (used right after prepare)
    void snapToTargets()
    {
        cur = tgt;
        outGain = outTarget;
        for (int m = 0; m < active; ++m)
            coeffs[(size_t) m] = peaking (design (active).frequency (m), design (active).getQ(), cur[(size_t) m], fs);
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        numChannels = std::min (numChannels, (int) states.size());

        for (int start = 0; start < numSamples; start += kSubBlock)
        {
            const int len = std::min (kSubBlock, numSamples - start);

            if (pending != active && isFlat())
            {
                active = pending;
                cur.fill (0.0);
                tgt.fill (0.0);
                for (auto& c : coeffs)
                    c = Coeffs {};
                reset();
            }

            updateSmoothing();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* x  = data[ch] + start;
                auto&  st = states[(size_t) ch];

                for (int m = 0; m < active; ++m)
                {
                    if (cur[(size_t) m] == 0.0)
                        continue; // a filter at 0 dB is a pass-through

                    const Coeffs& c = coeffs[(size_t) m];
                    State&        s = st[(size_t) m];
                    double z1 = s.z1, z2 = s.z2;

                    for (int i = 0; i < len; ++i)
                    {
                        const double in  = x[i];
                        const double out = c.b0 * in + z1;
                        z1 = c.b1 * in - c.a1 * out + z2;
                        z2 = c.b2 * in - c.a2 * out;
                        x[i] = (float) out;
                    }

                    s.z1 = z1;
                    s.z2 = z2;
                }
            }

            // output gain (per-sample ramp)
            for (int i = 0; i < len; ++i)
            {
                outGain += (outTarget - outGain) * outK;
                for (int ch = 0; ch < numChannels; ++ch)
                    data[ch][start + i] *= (float) outGain;
            }
        }
    }

    int activeBands() const { return active; }
    const Design& design (int numBands) const { return designs[numBands == 31 ? 1 : 0]; }

private:
    struct State
    {
        double z1 = 0, z2 = 0;
    };

    static constexpr int kSubBlock = 32;

    bool isFlat() const
    {
        for (int m = 0; m < active; ++m)
            if (cur[(size_t) m] != 0.0)
                return false;
        return true;
    }

    void updateSmoothing()
    {
        const Design& d = design (active);
        for (int m = 0; m < active; ++m)
        {
            double& c = cur[(size_t) m];
            const double t = tgt[(size_t) m];
            if (c == t)
                continue;

            c += (t - c) * smoothK;
            if (std::abs (t - c) < 0.02)
                c = t;

            coeffs[(size_t) m] = peaking (d.frequency (m), d.getQ(), c, fs);

            if (c == 0.0) // filter is back at 0 dB: switch it off and clear its state
                for (auto& ch : states)
                    ch[(size_t) m] = {};
        }
    }

    double fs      = 44100.0;
    int    active  = 10;
    int    pending = 10;
    double smoothK = 0.1, outK = 0.001;
    double outGain = 1.0, outTarget = 1.0;

    std::array<Design, 2>                   designs;
    std::array<double, kMaxBands>           cur {}, tgt {};
    std::array<Coeffs, kMaxBands>           coeffs {};
    std::vector<std::array<State, kMaxBands>> states;
};

} // namespace geq
