// ProEqDsp.h - Melon Pro EQ DSP core, plain C++17 (no JUCE) so it can be tested on its own.
//
// A parametric equalizer with the feature set of a modern mixing/mastering EQ (FabFilter Pro-Q style):
//
//   - 24 bands, each with: type, frequency, gain, Q, slope, stereo placement and dynamic EQ
//   - 8 filter types: Bell, Low Shelf, Low Cut, High Shelf, High Cut, Notch, Band Pass, Tilt Shelf
//   - Cut slopes from 6 to 96 dB/oct (Butterworth, with adjustable resonance through Q)
//   - Stereo placement per band: Stereo, Left, Right, Mid, Side
//   - Dynamic EQ per band: the band's gain moves by up to "Dyn Range" dB when the input around the
//     band's frequency goes above "Threshold"
//   - Two processing modes:
//       Zero Latency : cascade of minimum-phase IIR filters (no latency, analog-like phase)
//       Linear Phase : the whole static curve becomes one linear-phase FIR (no phase shift, latency =
//                      kernel size). The kernels are designed on a background thread and crossfaded.
//   - Gain Scale (scales every band's gain), Auto Gain, Solo band, Bypass and Output gain
//
// Signal flow (Zero Latency):
//
//   in ─► Stereo/Left/Right bands (L/R domain) ─► Mid/Side bands (M/S domain) ─► solo ─► output gain ─► out
//
// In Linear Phase mode the same curve (L/R bands first, then M/S bands) is folded into one 2x2 FIR
// matrix, so the result is identical in magnitude; the dynamic part of dynamic bands runs as IIR
// filters after the FIR.
#pragma once

#include "MelonDsp.h"
#include <array>
#include <atomic>
#include <chrono>
#include <complex>
#include <cstdint>
#include <thread>
#include <vector>

namespace proeq
{
using cd = std::complex<double>;

constexpr int    kNumBands       = 24;
constexpr int    kMaxSections    = 16;   // biquads per band (a 96 dB/oct band pass needs 16)
constexpr int    kNumSlopes      = 9;
constexpr int    kSlopes[kNumSlopes] = { 6, 12, 18, 24, 30, 36, 48, 72, 96 };
constexpr double kMinFreq        = 10.0;
constexpr double kMaxFreq        = 30000.0;
constexpr double kMinQ           = 0.025;
constexpr double kMaxQ           = 40.0;
constexpr double kMaxGainDb      = 30.0;
constexpr double kDynRatio       = 4.0;   // dynamic EQ: band gain moves (1 - 1/ratio) dB per dB over the threshold
constexpr int    kSubBlock       = 32;    // parameters and coefficients are updated every 32 samples
constexpr int    kNumResolutions = 3;
constexpr int    kKernelSizes[kNumResolutions] = { 2048, 4096, 8192 }; // Linear Phase: Low / Medium / High

// Kernel length scales with the sample rate, so the frequency resolution and the latency in
// milliseconds stay the same at every rate.
inline int rateMultiplier (double fs) { return fs > 120000.0 ? 4 : (fs > 60000.0 ? 2 : 1); }
inline int kernelSize (int resolution, double fs) { return kKernelSizes[std::clamp (resolution, 0, kNumResolutions - 1)] * rateMultiplier (fs); }
constexpr int    kAutoGainEvery  = 2048;  // samples between Auto Gain updates

enum class Type { bell, lowShelf, lowCut, highShelf, highCut, notch, bandPass, tiltShelf };
constexpr int kNumTypes = 8;
enum class Placement { stereo, left, right, mid, side };
enum class Phase { zeroLatency, linear };

inline bool hasGain (Type t) { return t == Type::bell || t == Type::lowShelf || t == Type::highShelf || t == Type::tiltShelf; }
inline bool usesSlope (Type t) { return t != Type::bell && t != Type::notch; }
inline bool isMidSide (Placement p) { return p == Placement::mid || p == Placement::side; }

struct Band
{
    bool      used = false;      // the band exists (shown in the display)
    bool      on   = true;       // enabled (a used band can be switched off)
    Type      type = Type::bell;
    double    freq = 1000.0;
    double    gainDb = 0.0;
    double    q    = 1.0;
    int       slope = 1;         // index into kSlopes
    Placement placement = Placement::stereo;
    double    dynRangeDb = 0.0;  // 0 = static band
    double    dynThresholdDb = -30.0;

    bool active() const { return used && on; }
    bool dynamic() const { return hasGain (type) && std::abs (dynRangeDb) > 0.05; }
};

struct Params
{
    std::array<Band, kNumBands> bands {};
    Phase  phase      = Phase::zeroLatency;
    int    resolution = 1;
    double gainScale  = 1.0;    // 1 = 100 %
    bool   autoGain   = false;
    double attackMs   = 5.0;    // dynamic EQ
    double releaseMs  = 100.0;
    double outputDb   = 0.0;
    bool   bypassed   = false;
    int    solo       = -1;     // band being soloed, -1 = none
};

//==============================================================================
// One biquad (or first-order section when b2 = a2 = 0), a0 normalised to 1.
struct Section
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    // |H|^2 at the angle w, given cos(w) and cos(2w)
    double magSq (double cw, double c2w) const
    {
        const double num = b0 * b0 + b1 * b1 + b2 * b2 + 2.0 * (b0 * b1 + b1 * b2) * cw + 2.0 * b0 * b2 * c2w;
        const double den = 1.0 + a1 * a1 + a2 * a2 + 2.0 * (a1 + a1 * a2) * cw + 2.0 * a2 * c2w;
        return std::max (0.0, num) / std::max (den, 1.0e-300);
    }
};

struct Cascade
{
    std::array<Section, kMaxSections> s {};
    int n = 0;

    void add (const Section& x)
    {
        if (n < kMaxSections)
            s[(size_t) n++] = x;
    }
    double magSq (double cw, double c2w) const
    {
        double m = 1.0;
        for (int i = 0; i < n; ++i)
            m *= s[(size_t) i].magSq (cw, c2w);
        return m;
    }
    double magnitudeDb (double freq, double fs) const
    {
        const double w = melon::kTwoPi * std::min (freq, 0.4999 * fs) / fs;
        return 10.0 * std::log10 (std::max (1.0e-30, magSq (std::cos (w), std::cos (2.0 * w))));
    }
};

//==============================================================================
// Filter sections (RBJ Audio EQ Cookbook for the second-order ones, bilinear transform with
// pre-warping for the first-order ones).
namespace design
{
inline double omega (double fs, double f) { return melon::kTwoPi * std::clamp (f, 1.0, 0.4999 * fs) / fs; }

inline Section normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

inline Section peak (double fs, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = omega (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
}
inline Section lowShelf2 (double fs, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = omega (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    const double s = 2.0 * std::sqrt (A) * al;
    return normalise (A * ((A + 1) - (A - 1) * cw + s), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - s),
                      (A + 1) + (A - 1) * cw + s, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - s);
}
inline Section highShelf2 (double fs, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = omega (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    const double s = 2.0 * std::sqrt (A) * al;
    return normalise (A * ((A + 1) + (A - 1) * cw + s), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - s),
                      (A + 1) - (A - 1) * cw + s, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - s);
}
inline Section lowpass2 (double fs, double f, double q)
{
    const double w = omega (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
}
inline Section highpass2 (double fs, double f, double q)
{
    const double w = omega (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
}
inline Section bandpass2 (double fs, double f, double q) // 0 dB at the centre
{
    const double w = omega (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise (al, 0, -al, 1 + al, -2 * cw, 1 - al);
}
inline Section notch2 (double fs, double f, double q)
{
    const double w = omega (fs, f), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise (1, -2 * cw, 1, 1 + al, -2 * cw, 1 - al);
}

inline double prewarp (double fs, double f) { return std::tan (0.5 * omega (fs, f)); }

inline Section lowpass1 (double fs, double f)
{
    const double K = prewarp (fs, f);
    return normalise (K, K, 0, 1 + K, K - 1, 0);
}
inline Section highpass1 (double fs, double f)
{
    const double K = prewarp (fs, f);
    return normalise (1, -1, 0, 1 + K, K - 1, 0);
}
// First-order shelves: H(s) = (s + w*g) / (s + w/g) (low), (g*s + w) / (s/g + w) (high), g = sqrt(gain)
inline Section lowShelf1 (double fs, double f, double gainDb)
{
    const double K = prewarp (fs, f), g = std::pow (10.0, gainDb / 40.0);
    return normalise (1 + K * g, K * g - 1, 0, 1 + K / g, K / g - 1, 0);
}
inline Section highShelf1 (double fs, double f, double gainDb)
{
    const double K = prewarp (fs, f), g = std::pow (10.0, gainDb / 40.0);
    return normalise (g + K, K - g, 0, 1.0 / g + K, K - 1.0 / g, 0);
}

// Butterworth high-pass / low-pass of any order. `resonance` multiplies the Q of the
// sharpest section (1 = plain Butterworth), which is how the Q control shapes a cut.
inline void butterworth (Cascade& c, bool highpass, double fs, double f, int order, double resonance)
{
    if (order % 2 == 1)
        c.add (highpass ? highpass1 (fs, f) : lowpass1 (fs, f));

    const int pairs = order / 2;
    for (int k = 1; k <= pairs; ++k)
    {
        const double phi = (order % 2 == 1) ? k * melon::kPi / order : (2 * k - 1) * melon::kPi / (2.0 * order);
        double q = 1.0 / (2.0 * std::cos (phi));
        if (k == pairs)
            q *= resonance;
        c.add (highpass ? highpass2 (fs, f, q) : lowpass2 (fs, f, q));
    }
}

inline double bandwidthOctaves (double q) { return 2.0 / std::log (2.0) * std::asinh (1.0 / (2.0 * q)); }
} // namespace design

// Builds the filter sections of a band, with `gainDb` as its (effective) gain.
inline void designBand (const Band& b, double gainDb, double fs, Cascade& c)
{
    using namespace design;
    c.n = 0;
    const double f     = std::clamp (b.freq, kMinFreq, 0.49 * fs);
    const double q     = std::clamp (b.q, kMinQ, kMaxQ);
    const int    order = kSlopes[std::clamp (b.slope, 0, kNumSlopes - 1)] / 6;
    const double g     = std::clamp (gainDb, -2.0 * kMaxGainDb, 2.0 * kMaxGainDb);

    switch (b.type)
    {
        case Type::bell:      c.add (peak (fs, f, q, g)); break;
        case Type::lowShelf:  c.add (order == 1 ? lowShelf1 (fs, f, g) : lowShelf2 (fs, f, q, g)); break;
        case Type::highShelf: c.add (order == 1 ? highShelf1 (fs, f, g) : highShelf2 (fs, f, q, g)); break;
        case Type::tiltShelf: // lows go down by half the gain, highs up by half: total tilt = gain
            if (order == 1)
            {
                c.add (lowShelf1 (fs, f, -0.5 * g));
                c.add (highShelf1 (fs, f, 0.5 * g));
            }
            else
            {
                c.add (lowShelf2 (fs, f, q, -0.5 * g));
                c.add (highShelf2 (fs, f, q, 0.5 * g));
            }
            break;
        case Type::lowCut:  butterworth (c, true, fs, f, order, q * std::sqrt (2.0)); break; // Q 0.71 = Butterworth
        case Type::highCut: butterworth (c, false, fs, f, order, q * std::sqrt (2.0)); break;
        case Type::notch:   c.add (notch2 (fs, f, q)); break;
        case Type::bandPass:
            if (order == 1)
            {
                c.add (bandpass2 (fs, f, q));
            }
            else
            {
                // high-pass and low-pass skirts of the chosen slope around the band, normalised to 0 dB at f
                const double k  = std::pow (2.0, 0.5 * std::max (0.05, bandwidthOctaves (q)));
                butterworth (c, true, fs, f / k, order, 1.0);
                butterworth (c, false, fs, std::min (f * k, 0.49 * fs), order, 1.0);
                const double w  = omega (fs, f);
                const double m  = std::sqrt (c.magSq (std::cos (w), std::cos (2.0 * w)));
                if (m > 1.0e-9)
                {
                    c.s[0].b0 /= m;
                    c.s[0].b1 /= m;
                    c.s[0].b2 /= m;
                }
            }
            break;
    }
}

// Response of a band at its current settings (static gain x gainScale), for the display.
inline void designBandStatic (const Band& b, double gainScale, double fs, Cascade& c)
{
    designBand (b, b.gainDb * gainScale, fs, c);
}

// The filter used by dynamic EQ to listen to the band's region of the input.
inline Section detectorFor (const Band& b, double fs)
{
    const double f = std::clamp (b.freq, kMinFreq, 0.49 * fs);
    switch (b.type)
    {
        case Type::lowShelf:  return design::lowpass2 (fs, f, 0.7071);
        case Type::highShelf: return design::highpass2 (fs, f, 0.7071);
        case Type::bell:
        case Type::tiltShelf:
        case Type::lowCut:
        case Type::highCut:
        case Type::notch:
        case Type::bandPass:  break;
    }
    return design::bandpass2 (fs, f, std::clamp (b.q, 0.3, 12.0));
}

// Gain change of a dynamic band (dB) for a detector level (dB).
inline double dynamicOffsetDb (double levelDb, double thresholdDb, double rangeDb)
{
    const double over = levelDb - thresholdDb;
    if (over <= 0.0 || rangeDb == 0.0)
        return 0.0;
    const double amount = std::min (std::abs (rangeDb), over * (1.0 - 1.0 / kDynRatio));
    return rangeDb > 0.0 ? amount : -amount;
}

// Auto Gain: minus the average level change of the gain bands between 40 Hz and 16 kHz.
// Left/Right bands count half (they change one channel), Side bands are ignored.
inline double autoGainDb (const Params& p, double fs)
{
    constexpr int kPoints = 48;
    double sum = 0.0;
    Cascade c;
    for (const auto& b : p.bands)
    {
        if (! b.active() || ! hasGain (b.type) || b.placement == Placement::side)
            continue;
        const double weight = (b.placement == Placement::left || b.placement == Placement::right) ? 0.5 : 1.0;
        designBandStatic (b, p.gainScale, fs, c);
        for (int i = 0; i < kPoints; ++i)
            sum += weight * c.magnitudeDb (40.0 * std::pow (400.0, i / (double) (kPoints - 1)), fs);
    }
    return std::clamp (-sum / kPoints, -24.0, 24.0);
}

//==============================================================================
// Radix-2 complex FFT. Read-only after init(), so it can be shared between threads.
class Fft
{
public:
    void init (int size)
    {
        n = size;
        tw.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
            tw[(size_t) k] = std::polar (1.0, -melon::kTwoPi * k / n);
        rev.resize ((size_t) n);
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    int size() const { return n; }

    void transform (cd* d, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i])
                std::swap (d[i], d[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const cd w = inverse ? std::conj (tw[(size_t) (j * step)]) : tw[(size_t) (j * step)];
                    const cd u = d[i + j], v = d[i + j + half] * w;
                    d[i + j]        = u + v;
                    d[i + j + half] = u - v;
                }
        }
        if (inverse)
        {
            const double s = 1.0 / n;
            for (int i = 0; i < n; ++i)
                d[i] *= s;
        }
    }

private:
    int n = 0;
    std::vector<cd>  tw;
    std::vector<int> rev;
};

//==============================================================================
// Linear Phase: the 2x2 FIR matrix (out_i = sum_j A_ij * in_j), as spectra ready for the
// convolver. Without Mid/Side bands only the diagonal is used.
struct KernelSet
{
    int  size  = 0;     // kernel length N (0 = not designed yet)
    bool cross = false; // uses the off-diagonal terms (Mid/Side bands present)
    std::array<std::vector<cd>, 4> spec; // A00, A01, A10, A11, each 2N bins

    void allocate (int maxKernel)
    {
        for (auto& s : spec)
            s.assign ((size_t) (2 * maxKernel), cd {});
    }
};

// Tables for one Linear Phase resolution (read-only after prepare)
struct Resolution
{
    int n = 0;
    Fft fftN, fft2N;
    std::vector<double> window, cosW, cos2W; // window: N points; cos tables: N/2 + 1 bins
    KernelSet identity;                        // pure delay of N/2 (used until the first kernel is ready)

    void prepare (int size);
};

// Designs the FIR spectra for the static curve of `p`.
class KernelDesigner
{
public:
    void prepare (int maxKernel)
    {
        for (auto& m : mags) m.assign ((size_t) maxKernel / 2 + 1, 1.0);
        bufN.assign ((size_t) maxKernel, cd {});
    }

    void design (const Params& p, double fs, const Resolution& r, KernelSet& out)
    {
        const int n = r.n, bins = n / 2 + 1;
        auto &hl = mags[0], &hr = mags[1], &hm = mags[2], &hs = mags[3];
        for (auto& m : mags) std::fill (m.begin(), m.begin() + bins, 1.0);

        bool cross = false;
        Cascade c;
        for (const auto& b : p.bands)
        {
            if (! b.active())
                continue;
            designBandStatic (b, p.gainScale, fs, c);
            const bool toL = b.placement == Placement::stereo || b.placement == Placement::left;
            const bool toR = b.placement == Placement::stereo || b.placement == Placement::right;
            cross = cross || isMidSide (b.placement);
            for (int k = 0; k < bins; ++k)
            {
                const double m = std::sqrt (c.magSq (r.cosW[(size_t) k], r.cos2W[(size_t) k]));
                if (toL) hl[(size_t) k] *= m;
                if (toR) hr[(size_t) k] *= m;
                if (b.placement == Placement::mid)  hm[(size_t) k] *= m;
                if (b.placement == Placement::side) hs[(size_t) k] *= m;
            }
        }

        out.size  = n;
        out.cross = cross;
        if (! cross)
        {
            toKernel (hl.data(), r, out.spec[0]);
            toKernel (hr.data(), r, out.spec[3]);
            return;
        }
        // M = (L+R)/2, S = (L-R)/2; L = M+S, R = M-S  =>  A = 1/2 [[(HM+HS)HL, (HM-HS)HR], [(HM-HS)HL, (HM+HS)HR]]
        std::vector<double>& t = tmp;
        t.resize ((size_t) bins);
        auto make = [&] (int idx, double sgn, const std::vector<double>& h) {
            for (int k = 0; k < bins; ++k)
                t[(size_t) k] = 0.5 * (hm[(size_t) k] + sgn * hs[(size_t) k]) * h[(size_t) k];
            toKernel (t.data(), r, out.spec[(size_t) idx]);
        };
        make (0, +1.0, hl);
        make (1, -1.0, hr);
        make (2, -1.0, hl);
        make (3, +1.0, hr);
    }

    // Real zero-phase response (N/2+1 bins) -> windowed linear-phase kernel of N taps centred at N/2
    // -> spectrum of 2N bins for overlap-save.
    void toKernel (const double* response, const Resolution& r, std::vector<cd>& out)
    {
        const int n = r.n;
        for (int k = 0; k <= n / 2; ++k)
            bufN[(size_t) k] = response[k];
        for (int k = 1; k < n / 2; ++k)
            bufN[(size_t) (n - k)] = response[k];
        r.fftN.transform (bufN.data(), true);

        std::fill (out.begin(), out.begin() + 2 * n, cd {});
        for (int m = 0; m < n; ++m)
            out[(size_t) m] = bufN[(size_t) ((m - n / 2 + n) % n)].real() * r.window[(size_t) m];
        r.fft2N.transform (out.data(), false);
    }

private:
    std::array<std::vector<double>, 4> mags;
    std::vector<double> tmp;
    std::vector<cd> bufN;
};

inline void Resolution::prepare (int size)
{
    n = size;
    fftN.init (n);
    fft2N.init (2 * n);
    window.resize ((size_t) n);
    for (int m = 0; m < n; ++m) // periodic Blackman, peak at N/2
        window[(size_t) m] = 0.42 - 0.5 * std::cos (melon::kTwoPi * m / n) + 0.08 * std::cos (2.0 * melon::kTwoPi * m / n);
    cosW.resize ((size_t) n / 2 + 1);
    cos2W.resize ((size_t) n / 2 + 1);
    for (int k = 0; k <= n / 2; ++k)
    {
        cosW[(size_t) k]  = std::cos (melon::kTwoPi * k / n);
        cos2W[(size_t) k] = std::cos (2.0 * melon::kTwoPi * k / n);
    }
    identity.allocate (n);
    KernelDesigner d;
    d.prepare (n);
    std::vector<double> flat ((size_t) n / 2 + 1, 1.0);
    d.toKernel (flat.data(), *this, identity.spec[0]);
    identity.spec[3] = identity.spec[0];
    identity.size  = n;
    identity.cross = false;
}

// Triple buffer: the designer thread writes kernels, the audio thread reads the newest one.
class KernelExchange
{
public:
    void allocate (int maxKernel)
    {
        for (auto& s : sets)
        {
            s.allocate (maxKernel);
            s.size = 0;
        }
        middle.store (1);
        front = 0;
        back  = 2;
    }
    // writer side
    KernelSet& writeBuffer() { return sets[(size_t) back]; }
    void       publish() { back = middle.exchange (back | kNew) & 3; }
    // reader side
    bool             hasNew() const { return (middle.load() & kNew) != 0; }
    void             acquire() { front = middle.exchange (front) & 3; }
    const KernelSet& current() const { return sets[(size_t) front]; }

private:
    static constexpr int kNew = 4;
    std::array<KernelSet, 3> sets;
    std::atomic<int> middle { 1 };
    int front = 0, back = 2;
};

//==============================================================================
// Uniform overlap-save convolution with a 2x2 kernel matrix. FFT size 2N, hop N/2,
// latency N (N/2 for the kernel centre + N/2 of buffering).
class Convolver
{
public:
    void prepare (int maxKernel)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            hist[(size_t) ch].assign ((size_t) (2 * maxKernel), 0.0);
            inFifo[(size_t) ch].assign ((size_t) maxKernel, 0.0f);
            outFifo[(size_t) ch].assign ((size_t) maxKernel, 0.0f);
            X[(size_t) ch].assign ((size_t) (2 * maxKernel), cd {});
        }
        Y.assign ((size_t) (2 * maxKernel), cd {});
        prev.assign ((size_t) maxKernel, 0.0);
    }

    void reset (const Resolution& r)
    {
        res = &r;
        hop = r.n / 2;
        pos = 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (hist[(size_t) ch].begin(), hist[(size_t) ch].end(), 0.0);
            std::fill (inFifo[(size_t) ch].begin(), inFifo[(size_t) ch].end(), 0.0f);
            std::fill (outFifo[(size_t) ch].begin(), outFifo[(size_t) ch].end(), 0.0f);
        }
    }

    void process (float* const* data, int numChannels, int numSamples, KernelExchange& kx)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                inFifo[(size_t) ch][(size_t) pos] = data[ch][i];
                data[ch][i] = outFifo[(size_t) ch][(size_t) pos];
            }
            if (++pos == hop)
            {
                computeHop (numChannels, kx);
                pos = 0;
            }
        }
    }

private:
    const KernelSet& usable (const KernelSet& k) const { return k.size == res->n ? k : res->identity; }

    void computeHop (int numChannels, KernelExchange& kx)
    {
        const int n2 = 2 * res->n;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& h = hist[(size_t) ch];
            std::copy (h.begin() + hop, h.begin() + n2, h.begin());
            for (int i = 0; i < hop; ++i)
                h[(size_t) (n2 - hop + i)] = inFifo[(size_t) ch][(size_t) i];
            auto& x = X[(size_t) ch];
            for (int i = 0; i < n2; ++i)
                x[(size_t) i] = h[(size_t) i];
            res->fft2N.transform (x.data(), false);
        }

        const bool fade = kx.hasNew();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            apply (usable (kx.current()), ch, numChannels);
            if (fade)
                for (int i = 0; i < hop; ++i)
                    prev[(size_t) (ch * hop + i)] = Y[(size_t) (n2 - hop + i)].real();
            else
                for (int i = 0; i < hop; ++i)
                    outFifo[(size_t) ch][(size_t) i] = (float) Y[(size_t) (n2 - hop + i)].real();
        }
        if (! fade)
            return;

        kx.acquire(); // the old kernel is released only now that it is no longer used
        for (int ch = 0; ch < numChannels; ++ch)
        {
            apply (usable (kx.current()), ch, numChannels);
            for (int i = 0; i < hop; ++i)
            {
                const double t = (i + 0.5) / hop;
                outFifo[(size_t) ch][(size_t) i] = (float) ((1.0 - t) * prev[(size_t) (ch * hop + i)] + t * Y[(size_t) (n2 - hop + i)].real());
            }
        }
    }

    // Y = IFFT (sum_j A[ch][j] X[j]) for output channel ch
    void apply (const KernelSet& k, int ch, int numChannels)
    {
        const int n2 = 2 * res->n;
        if (numChannels == 1 || ! k.cross)
        {
            const auto& a = k.spec[ch == 0 ? 0 : 3];
            const auto& x = X[(size_t) ch];
            for (int i = 0; i < n2; ++i)
                Y[(size_t) i] = a[(size_t) i] * x[(size_t) i];
        }
        else
        {
            const auto& a0 = k.spec[ch == 0 ? 0 : 2];
            const auto& a1 = k.spec[ch == 0 ? 1 : 3];
            for (int i = 0; i < n2; ++i)
                Y[(size_t) i] = a0[(size_t) i] * X[0][(size_t) i] + a1[(size_t) i] * X[1][(size_t) i];
        }
        res->fft2N.transform (Y.data(), true);
    }

    const Resolution* res = nullptr;
    int hop = 1, pos = 0;
    std::array<std::vector<double>, 2> hist;
    std::array<std::vector<float>, 2>  inFifo, outFifo;
    std::array<std::vector<cd>, 2>     X;
    std::vector<cd>     Y;
    std::vector<double> prev;
};

//==============================================================================
// Lock-free ring of the latest samples, for the spectrum analyzer (one writer, one reader).
class AnalyzerFifo
{
public:
    static constexpr int kSize = 32768;
    void push (float x)
    {
        const int w = writePos.load (std::memory_order_relaxed);
        buf[(size_t) (w & (kSize - 1))].store (x, std::memory_order_relaxed);
        writePos.store (w + 1, std::memory_order_release);
    }
    // Copies the latest `count` samples (count <= kSize); returns the write counter
    int copyLatest (float* dest, int count) const
    {
        const int end = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
            dest[i] = buf[(size_t) ((end - count + i) & (kSize - 1))].load (std::memory_order_relaxed);
        return end;
    }

private:
    std::array<std::atomic<float>, kSize> buf {};
    std::atomic<int> writePos { 0 };
};

//==============================================================================
// One band in the IIR path: smoothing, click-free type/placement changes and dynamics.
class BandRuntime
{
public:
    void prepare (double sampleRate)
    {
        fs        = sampleRate;
        smoothK   = 1.0 - std::exp (-(double) kSubBlock / (0.02 * fs)); // ~20 ms
        mixStep   = (double) kSubBlock / (0.01 * fs);                     // 10 ms fades
        reset();
    }

    void reset()
    {
        mix = mixTarget = 0.0;
        configured = false;
        env[0] = env[1] = 0.0;
        offsetDb = 0.0;
        clearStates();
    }

    // `lpMode`: Linear Phase, where this filter only carries the dynamic part of the band.
    // `det`: detection signals L, R, M, S (aligned with the audio), `numDet` samples.
    void update (const Band& p, const Params& gp, bool lpMode, int numChannels, const double* const* det, int numDet)
    {
        const bool live = p.active() && (! lpMode || p.dynamic());
        const bool sameConfig = configured && p.type == cfg.type && p.slope == cfg.slope && p.placement == cfg.placement;

        if (! sameConfig && mix <= 0.0)
        {
            cfg = p; // switch type / slope / placement while silent
            configured = true;
            clearStates();
            logF = std::log (p.freq);
            gain = p.gainDb;
            logQ = std::log (p.q);
            env[0] = env[1] = 0.0;
            offsetDb = 0.0;
            designed = false;
        }
        mixTarget = (live && configured && p.type == cfg.type && p.slope == cfg.slope && p.placement == cfg.placement) ? 1.0 : 0.0;

        mixStart = mix;
        mix      = mixTarget > mix ? std::min (mixTarget, mix + mixStep) : std::max (mixTarget, mix - mixStep);
        if (mixStart <= 0.0 && mix <= 0.0)
        {
            logF = std::log (p.freq); gain = p.gainDb; logQ = std::log (p.q);
            offsetDb = 0.0;
            return;
        }

        auto smooth = [this] (double& v, double t, double eps) { v += (t - v) * smoothK; if (std::abs (t - v) < eps) v = t; };
        smooth (logF, std::log (p.freq), 1.0e-4);
        smooth (gain, p.gainDb, 1.0e-3);
        smooth (logQ, std::log (p.q), 1.0e-4);

        Band b    = cfg;
        b.freq    = std::exp (logF);
        b.gainDb  = gain;
        b.q       = std::exp (logQ);

        // dynamics
        if (p.dynamic() && hasGain (cfg.type))
        {
            const Section d = detectorFor (b, fs);
            const double att = 1.0 - std::exp (-1.0 / std::max (1.0, gp.attackMs * 0.001 * fs));
            const double rel = 1.0 - std::exp (-1.0 / std::max (1.0, gp.releaseMs * 0.001 * fs));
            int src[2] = { 0, 1 };
            int nSrc   = numChannels > 1 ? 2 : 1;
            switch (cfg.placement)
            {
                case Placement::stereo: break;
                case Placement::left:  nSrc = 1; break;
                case Placement::right: nSrc = 1; src[0] = numChannels > 1 ? 1 : 0; break;
                case Placement::mid:   nSrc = 1; src[0] = 2; break;
                case Placement::side:  nSrc = 1; src[0] = 3; break;
            }
            double level = 0.0;
            for (int k = 0; k < nSrc; ++k)
            {
                const double* x = det[src[k]];
                double& e = env[k];
                double z1 = dz[k][0], z2 = dz[k][1];
                for (int i = 0; i < numDet; ++i)
                {
                    const double y = d.b0 * x[i] + z1;
                    z1 = d.b1 * x[i] - d.a1 * y + z2;
                    z2 = d.b2 * x[i] - d.a2 * y;
                    const double a = std::abs (y);
                    e += (a - e) * (a > e ? att : rel);
                }
                dz[k][0] = z1; dz[k][1] = z2;
                level = std::max (level, e);
            }
            offsetDb = dynamicOffsetDb (melon::gainToDb (level), p.dynThresholdDb, p.dynRangeDb);
        }
        else
        {
            offsetDb = 0.0;
        }

        const double eff = lpMode ? offsetDb * gp.gainScale : (gain + offsetDb) * gp.gainScale;
        if (! designed || b.freq != dFreq || b.q != dQ || eff != dGain)
        {
            designBand (b, eff, fs, cascade);
            dFreq = b.freq; dQ = b.q; dGain = eff;
            designed = true;
        }
    }

    bool audible() const { return mixStart > 0.0 || mix > 0.0; }
    Placement placement() const { return cfg.placement; }
    double currentOffsetDb (double gainScale) const { return audible() ? offsetDb * gainScale : 0.0; }

    // Processes stage channel `ch` (index into the state arrays) of `len` samples in place.
    void process (int ch, float* x, int len)
    {
        auto& st = z[(size_t) ch];
        const double m0 = mixStart, dm = (mix - mixStart) / len;
        for (int i = 0; i < len; ++i)
        {
            const double in = x[i];
            double v = in;
            for (int s = 0; s < cascade.n; ++s)
            {
                const Section& c = cascade.s[(size_t) s];
                double& z1 = st[(size_t) s][0];
                double& z2 = st[(size_t) s][1];
                const double y = c.b0 * v + z1;
                z1 = c.b1 * v - c.a1 * y + z2;
                z2 = c.b2 * v - c.a2 * y;
                v = y;
            }
            const double m = m0 + dm * (i + 1);
            x[i] = (float) (in + m * (v - in));
        }
    }

private:
    void clearStates()
    {
        for (auto& ch : z)
            for (auto& s : ch)
                s = { 0.0, 0.0 };
        for (auto& d : dz)
            d[0] = d[1] = 0.0;
    }

    double fs = 48000.0, smoothK = 0.1, mixStep = 0.1;
    Band   cfg;
    bool   configured = false, designed = false;
    double mix = 0.0, mixTarget = 0.0, mixStart = 0.0;
    double logF = std::log (1000.0), gain = 0.0, logQ = 0.0;
    double dFreq = 0.0, dQ = 0.0, dGain = 0.0;
    double env[2] { 0.0, 0.0 };
    double dz[2][2] {};
    double offsetDb = 0.0;
    Cascade cascade;
    std::array<std::array<std::array<double, 2>, kMaxSections>, 2> z {};
};

//==============================================================================
class Engine
{
public:
    Engine() = default;
    ~Engine() { stopWorker(); }
    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    // Tests design the Linear Phase kernels on the audio thread, so results are deterministic.
    void setSynchronousKernels (bool s) { synchronous = s; }

    void prepare (double sampleRate, int numChannels)
    {
        stopWorker();
        fs   = sampleRate;
        nch  = std::clamp (numChannels, 1, 2);
        const int maxKernel = kernelSize (kNumResolutions - 1, fs);
        for (int r = 0; r < kNumResolutions; ++r)
            if (resolutions[(size_t) r].n != kernelSize (r, fs))
                resolutions[(size_t) r].prepare (kernelSize (r, fs));
        exchange.allocate (maxKernel);
        convolver.prepare (maxKernel);
        workerDesigner.prepare (maxKernel);
        syncDesigner.prepare (maxKernel);
        for (auto& d : delays)
            d.prepare (maxKernel + kSubBlock + 8);
        for (auto& b : bands)
            b.prepare (fs);
        for (auto& d : detBuf)
            d.assign ((size_t) kSubBlock, 0.0);
        outGain.prepare (fs, 15.0);
        wet.prepare (fs, 10.0);
        currentPhase = Phase::zeroLatency;
        currentRes   = -1;
        latency      = 0;
        autoGainCountdown = 0;
        autoGain = 0.0;
        soloIndex = -1;
        for (auto& s : soloState) s[0] = s[1] = 0.0;
        if (! synchronous)
            startWorker();
    }

    void setParams (const Params& p) { params = p; }

    void snapToTargets()
    {
        if (params.autoGain)
            autoGain = autoGainDb (params, fs);
        outGain.setTarget (melon::dbToGain (params.outputDb + (params.autoGain ? autoGain : 0.0)));
        outGain.snap();
        wet.setTarget (params.bypassed ? 0.0 : 1.0);
        wet.snap();
    }

    int latencySamples() const { return latency; }

    void process (float* const* data, int numChannels, int numSamples)
    {
        numChannels = std::min (numChannels, nch);
        for (int start = 0; start < numSamples; start += kSubBlock)
        {
            const int len = std::min (kSubBlock, numSamples - start);
            float* blk[2] = { data[0] + start, numChannels > 1 ? data[1] + start : nullptr };
            processSubBlock (blk, numChannels, len);
        }
    }

    const AnalyzerFifo& preFifo() const { return pre; }
    const AnalyzerFifo& postFifo() const { return post; }
    float dynamicOffsetDb (int band) const { return dynOffset[(size_t) band].load (std::memory_order_relaxed); }

private:
    void processSubBlock (float** x, int numChannels, int len)
    {
        // --- mode changes ------------------------------------------------------
        const int res = std::clamp (params.resolution, 0, kNumResolutions - 1);
        if (params.phase != currentPhase || (params.phase == Phase::linear && res != currentRes))
        {
            currentPhase = params.phase;
            currentRes   = res;
            for (auto& b : bands) b.reset();
            if (currentPhase == Phase::linear)
            {
                convolver.reset (resolutions[(size_t) res]);
                kernelDirty = true;
                latency = resolutions[(size_t) res].n;
            }
            else
            {
                latency = 0;
            }
        }
        const bool lp = currentPhase == Phase::linear;

        // --- dry / detection signal (input delayed by the latency) -------------
        float dry[2][kSubBlock];
        float pre0[kSubBlock];
        for (int i = 0; i < len; ++i)
        {
            double sum = 0.0;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                delays[(size_t) ch].push (x[ch][i]);
                dry[ch][i] = delays[(size_t) ch].readInt (latency);
                sum += x[ch][i];
            }
            pre0[i] = (float) (sum / numChannels);
        }
        for (int i = 0; i < len; ++i)
        {
            const double l = dry[0][i], r = numChannels > 1 ? dry[1][i] : l;
            detBuf[0][(size_t) i] = l;
            detBuf[1][(size_t) i] = r;
            detBuf[2][(size_t) i] = 0.5 * (l + r);
            detBuf[3][(size_t) i] = 0.5 * (l - r);
        }
        const double* det[4] = { detBuf[0].data(), detBuf[1].data(), detBuf[2].data(), detBuf[3].data() };

        // --- band parameters ----------------------------------------------------
        for (int b = 0; b < kNumBands; ++b)
        {
            bands[(size_t) b].update (params.bands[(size_t) b], params, lp, numChannels, det, len);
            dynOffset[(size_t) b].store ((float) bands[(size_t) b].currentOffsetDb (params.gainScale), std::memory_order_relaxed);
        }

        // --- linear-phase FIR ---------------------------------------------------
        if (lp)
        {
            if (kernelDirty || ! sameKernel (params, requested))
                requestKernel();
            convolver.process (x, numChannels, len, exchange);
        }

        // --- IIR bands (all of them in Zero Latency, the dynamic part in Linear Phase) ---
        for (auto& b : bands)
            if (b.audible() && ! isMidSide (b.placement()))
                processLR (b, x, numChannels, len);

        bool anyMS = false;
        for (auto& b : bands)
            anyMS = anyMS || (b.audible() && isMidSide (b.placement()));
        if (anyMS)
        {
            if (numChannels == 2)
            {
                for (int i = 0; i < len; ++i)
                {
                    const float l = x[0][i], r = x[1][i];
                    x[0][i] = 0.5f * (l + r);
                    x[1][i] = 0.5f * (l - r);
                }
                for (auto& b : bands)
                    if (b.audible() && isMidSide (b.placement()))
                        b.process (b.placement() == Placement::mid ? 0 : 1, x[b.placement() == Placement::mid ? 0 : 1], len);
                for (int i = 0; i < len; ++i)
                {
                    const float m = x[0][i], s = x[1][i];
                    x[0][i] = m + s;
                    x[1][i] = m - s;
                }
            }
            else
            {
                for (auto& b : bands) // mono: everything is Mid
                    if (b.audible() && b.placement() == Placement::mid)
                        b.process (0, x[0], len);
            }
        }

        // --- solo ---------------------------------------------------------------
        updateSolo();
        if (soloIndex >= 0)
            for (int ch = 0; ch < numChannels; ++ch)
            {
                double z1 = soloState[(size_t) ch][0], z2 = soloState[(size_t) ch][1];
                for (int i = 0; i < len; ++i)
                {
                    const double v = x[ch][i];
                    const double y = soloSection.b0 * v + z1;
                    z1 = soloSection.b1 * v - soloSection.a1 * y + z2;
                    z2 = soloSection.b2 * v - soloSection.a2 * y;
                    x[ch][i] = (float) y;
                }
                soloState[(size_t) ch][0] = z1;
                soloState[(size_t) ch][1] = z2;
            }

        // --- auto gain, output, bypass, analyzer ----------------------------------
        if (params.autoGain)
        {
            autoGainCountdown -= len;
            if (autoGainCountdown <= 0)
            {
                autoGain = autoGainDb (params, fs);
                autoGainCountdown = kAutoGainEvery;
            }
        }
        outGain.setTarget (melon::dbToGain (params.outputDb + (params.autoGain ? autoGain : 0.0)));
        wet.setTarget (params.bypassed ? 0.0 : 1.0);

        for (int i = 0; i < len; ++i)
        {
            const double g = outGain.next(), w = wet.next();
            double sum = 0.0;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const double y = (double) x[ch][i] * g;
                x[ch][i] = (float) (dry[ch][i] + w * (y - dry[ch][i]));
                sum += x[ch][i];
            }
            pre.push (pre0[i]);
            post.push ((float) (sum / numChannels));
        }
    }

    void processLR (BandRuntime& b, float** x, int numChannels, int len)
    {
        switch (b.placement())
        {
            case Placement::stereo:
                for (int ch = 0; ch < numChannels; ++ch)
                    b.process (ch, x[ch], len);
                break;
            case Placement::left: b.process (0, x[0], len); break;
            case Placement::right:
                if (numChannels > 1) b.process (1, x[1], len);
                break;
            case Placement::mid:
            case Placement::side: break; // processed in the M/S stage
        }
    }

    void updateSolo()
    {
        int s = params.solo;
        if (s < 0 || s >= kNumBands || ! params.bands[(size_t) s].active())
            s = -1;
        if (s != soloIndex)
            for (auto& st : soloState) st[0] = st[1] = 0.0;
        soloIndex = s;
        if (s < 0)
            return;
        const Band& b = params.bands[(size_t) s];
        const bool  useQ = hasGain (b.type) || b.type == Type::notch || b.type == Type::bandPass;
        soloSection = design::bandpass2 (fs, std::clamp (b.freq, kMinFreq, 0.49 * fs), std::clamp (useQ ? b.q : 0.7, 0.3, 10.0));
    }

    // Kernel-relevant settings are unchanged?
    static bool sameKernel (const Params& a, const Params& b)
    {
        if (a.gainScale != b.gainScale || a.resolution != b.resolution)
            return false;
        for (int i = 0; i < kNumBands; ++i)
        {
            const Band &x = a.bands[(size_t) i], &y = b.bands[(size_t) i];
            if (x.active() != y.active())
                return false;
            if (! x.active())
                continue;
            if (x.type != y.type || x.freq != y.freq || x.gainDb != y.gainDb || x.q != y.q || x.slope != y.slope
                || x.placement != y.placement)
                return false;
        }
        return true;
    }

    void requestKernel()
    {
        if (synchronous)
        {
            syncDesigner.design (params, fs, resolutions[(size_t) currentRes], exchange.writeBuffer());
            exchange.publish();
            requested   = params;
            kernelDirty = false;
            return;
        }
        if (workerLock.test_and_set (std::memory_order_acquire))
        {
            kernelDirty = true; // the worker is copying: try again next sub-block
            return;
        }
        pending    = params;
        pendingRes = currentRes;
        workerLock.clear (std::memory_order_release);
        requestSerial.fetch_add (1);
        requested   = params;
        kernelDirty = false;
    }

    void startWorker()
    {
        running = true;
        const uint32_t start = requestSerial.load(); // read here: a request may arrive before the thread runs
        worker  = std::thread ([this, start] {
            uint32_t done = start;
            Params   p;
            int      r = 1;
            while (running.load())
            {
                const uint32_t want = requestSerial.load();
                if (want == done)
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (3));
                    continue;
                }
                while (workerLock.test_and_set (std::memory_order_acquire)) {}
                p = pending;
                r = pendingRes;
                workerLock.clear (std::memory_order_release);
                workerDesigner.design (p, fs, resolutions[(size_t) std::clamp (r, 0, kNumResolutions - 1)], exchange.writeBuffer());
                exchange.publish();
                done = want;
            }
        });
    }

    void stopWorker()
    {
        running = false;
        if (worker.joinable())
            worker.join();
    }

    double fs  = 48000.0;
    int    nch = 2;
    Params params, requested, pending;
    int    pendingRes = 1;

    std::array<BandRuntime, kNumBands> bands;
    std::array<std::atomic<float>, kNumBands> dynOffset {};
    std::array<melon::DelayLine, 2> delays;
    std::array<std::vector<double>, 4> detBuf;

    std::array<Resolution, kNumResolutions> resolutions;
    KernelExchange exchange;
    Convolver      convolver;
    KernelDesigner workerDesigner, syncDesigner;
    bool  kernelDirty = true, synchronous = false;
    Phase currentPhase = Phase::zeroLatency;
    int   currentRes = -1, latency = 0;

    std::thread           worker;
    std::atomic<bool>     running { false };
    std::atomic_flag      workerLock = ATOMIC_FLAG_INIT;
    std::atomic<uint32_t> requestSerial { 0 };

    melon::Smoother outGain, wet;
    double autoGain = 0.0;
    int    autoGainCountdown = 0;

    int     soloIndex = -1;
    Section soloSection;
    std::array<std::array<double, 2>, 2> soloState {};

    AnalyzerFifo pre, post;
};

} // namespace proeq
