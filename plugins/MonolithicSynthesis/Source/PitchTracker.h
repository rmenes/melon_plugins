// Melon Monolithic Synthesis - low-latency monophonic pitch tracker
//
// McLeod Pitch Method (normalised square difference function) running on a
// pre-filtered, decimated (~11-12 kHz) copy of the guitar signal.
// A new estimate is produced every ~2.5 ms. Because leading silence adds
// nothing to either side of the NSDF ratio, a note is usually detected as soon
// as about two periods of it have been played.
#pragma once

#include "Filters.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace melon::mono
{
class PitchTracker
{
public:
    void prepare (double sampleRate, float lowestHz = 70.0f, float highestHz = 1400.0f)
    {
        sr = sampleRate;
        decimation = std::max (1, (int) std::lround (sr / 11025.0));
        dsr = sr / decimation;

        hp.setHighpass (sr, 55.0, 0.707);
        lp1.setLowpass (sr, 1250.0, 0.707);
        lp2.setLowpass (sr, 1250.0, 0.707);

        minLag = std::max (2, (int) std::floor (dsr / highestHz));
        maxLag = (int) std::ceil (dsr / lowestHz);
        window = 2 * maxLag + 8;
        hop = std::max (8, (int) std::lround (dsr * 0.0025));

        ring.assign ((size_t) window, 0.0f);
        frame.assign ((size_t) window, 0.0f);
        nsdf.assign ((size_t) maxLag + 2, 0.0f);
        reset();
    }

    void reset()
    {
        hp.reset(); lp1.reset(); lp2.reset();
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = decCount = hopCount = 0;
        frequency = 0.0f;
        clarity = 0.0f;
    }

    // Feed one input sample. Returns true when a new estimate is available.
    bool push (float x) noexcept
    {
        const float y = lp2.process (lp1.process (hp.process (x)));
        if (++decCount < decimation)
            return false;
        decCount = 0;

        ring[(size_t) writePos] = y;
        writePos = (writePos + 1) % window;

        if (++hopCount < hop)
            return false;
        hopCount = 0;

        analyse();
        return true;
    }

    float getFrequency() const noexcept { return frequency; } // Hz, 0 if none
    float getClarity() const noexcept   { return clarity; }   // 0..1
    double getAnalysisRate() const noexcept { return dsr; }

private:
    void analyse() noexcept
    {
        for (int i = 0; i < window; ++i)
            frame[(size_t) i] = ring[(size_t) ((writePos + i) % window)];

        // m(tau) computed incrementally
        double m = 0.0;
        for (int i = 0; i < window; ++i)
            m += 2.0 * (double) frame[(size_t) i] * frame[(size_t) i];

        if (m < 1.0e-9)
        {
            frequency = 0.0f; clarity = 0.0f;
            return;
        }

        for (int tau = 0; tau <= maxLag; ++tau)
        {
            if (tau > 0)
            {
                const double a = frame[(size_t) (window - tau)];
                const double b = frame[(size_t) (tau - 1)];
                m -= a * a + b * b;
            }

            double acf = 0.0;
            const float* x = frame.data();
            const int n = window - tau;
            for (int i = 0; i < n; ++i)
                acf += (double) x[i] * x[i + tau];

            nsdf[(size_t) tau] = m > 1.0e-12 ? (float) (2.0 * acf / m) : 0.0f;
        }

        // Key maxima: highest point of each positive lobe after the first zero crossing
        constexpr int maxKeys = 16;
        int keyTau[maxKeys];
        float keyVal[maxKeys];
        int numKeys = 0;

        int pos = 1;
        while (pos < maxLag && nsdf[(size_t) pos] > 0.0f) ++pos;

        while (pos < maxLag && numKeys < maxKeys)
        {
            while (pos < maxLag && nsdf[(size_t) pos] <= 0.0f) ++pos;
            if (pos >= maxLag) break;

            int best = pos;
            while (pos < maxLag && nsdf[(size_t) pos] > 0.0f)
            {
                if (nsdf[(size_t) pos] > nsdf[(size_t) best]) best = pos;
                ++pos;
            }
            if (best >= minLag && best < maxLag)
            {
                keyTau[numKeys] = best;
                keyVal[numKeys] = nsdf[(size_t) best];
                ++numKeys;
            }
        }

        if (numKeys == 0)
        {
            frequency = 0.0f; clarity = 0.0f;
            return;
        }

        float highest = 0.0f;
        for (int i = 0; i < numKeys; ++i) highest = std::max (highest, keyVal[i]);

        const float threshold = 0.88f * highest;
        int chosen = 0;
        for (int i = 0; i < numKeys; ++i)
            if (keyVal[i] >= threshold) { chosen = i; break; }

        // Octave check: when the 2nd harmonic dominates, the half period looks periodic too.
        // If the key at twice the lag is clearly more periodic (less than half the
        // aperiodicity) and the current pick is noticeably imperfect, take the longer period.
        // Values are measured exactly at the fractional period, so short (high-note) lags
        // aren't penalised by integer-lag quantisation.
        for (int pass = 0; pass < 2; ++pass)
        {
            const double tauHere = refinedLag (keyTau[chosen]);
            const float aper = 1.0f - nsdfAt (tauHere);
            if (aper <= 0.01f) break;
            const int target = 2 * keyTau[chosen];
            int found = -1;
            for (int i = chosen + 1; i < numKeys; ++i)
                if (std::abs (keyTau[i] - target) <= std::max (2, target / 40)) { found = i; break; }
            if (found < 0) break;
            const float aperDouble = 1.0f - nsdfAt (refinedLag (keyTau[found]));
            if (aperDouble >= 0.4f * aper) break;
            chosen = found;
        }

        const int t = keyTau[chosen];
        const float a = nsdf[(size_t) t - 1], b = nsdf[(size_t) t], c = nsdf[(size_t) t + 1];
        const float denom = a - 2.0f * b + c;
        float delta = 0.0f, peak = b;
        if (std::fabs (denom) > 1.0e-9f)
        {
            delta = std::clamp (0.5f * (a - c) / denom, -0.5f, 0.5f);
            peak = b - 0.25f * (a - c) * delta;
        }

        const double period = (double) t + (double) delta;

        frequency = (float) (dsr / period);
        clarity = std::clamp (peak, 0.0f, 1.0f);
    }

    double refinedLag (int t) const noexcept
    {
        const float a = nsdf[(size_t) t - 1], b = nsdf[(size_t) t], c = nsdf[(size_t) t + 1];
        const float denom = a - 2.0f * b + c;
        return std::fabs (denom) > 1.0e-9f ? (double) t + (double) std::clamp (0.5f * (a - c) / denom, -0.5f, 0.5f) : (double) t;
    }

    // NSDF at a fractional lag, using 4-point Hermite interpolation of the lagged signal
    float nsdfAt (double tau) const noexcept
    {
        const int ti = (int) std::floor (tau);
        const float fr = (float) (tau - ti);
        const int n = window - ti - 3;
        if (n <= 8) return 0.0f;
        double acf = 0.0, m = 0.0;
        const float* x = frame.data();
        for (int i = 1; i < n; ++i)
        {
            const float y0 = x[i + ti - 1], y1 = x[i + ti], y2 = x[i + ti + 1], y3 = x[i + ti + 2];
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            const float yl = ((c3 * fr + c2) * fr + c1) * fr + y1;
            acf += (double) x[i] * yl;
            m += (double) x[i] * x[i] + (double) yl * yl;
        }
        return m > 1.0e-12 ? (float) (2.0 * acf / m) : 0.0f;
    }

    double sr = 44100.0, dsr = 11025.0;
    int decimation = 4, minLag = 8, maxLag = 160, window = 328, hop = 28;
    int writePos = 0, decCount = 0, hopCount = 0;

    Biquad hp, lp1, lp2;
    std::vector<float> ring, frame, nsdf;

    float frequency = 0.0f, clarity = 0.0f;
};

} // namespace melon::mono
