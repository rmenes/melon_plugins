// CabDsp.h - the plain-C++ part of Melon Cab (no JUCE): the Low Cut / High Cut filters
// applied to the cabinet sound. The convolution itself is juce::dsp::Convolution in the processor.
#pragma once

#include <MelonDsp.h>
#include <array>

namespace cab
{
constexpr double kLowCutOff  = 20.0;    // Low Cut at its minimum: filter off
constexpr double kHighCutOff = 20000.0; // High Cut at its maximum: filter off

// 12 dB/oct high-pass + 12 dB/oct low-pass (Butterworth), one pair per channel.
class CutFilters
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double fs)
    {
        sampleRate = fs;
        lowHz = highHz = -1.0; // force a recalculation
        set (kLowCutOff, kHighCutOff);
        reset();
    }
    void reset()
    {
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
    }

    void set (double lowCutHz, double highCutHz)
    {
        if (lowCutHz != lowHz)
        {
            lowHz = lowCutHz;
            lowOn = lowCutHz > kLowCutOff + 0.5;
            for (auto& f : hp)
                f.set (melon::Biquad::Type::highpass, sampleRate, lowCutHz, 0.7071);
        }
        if (highCutHz != highHz)
        {
            highHz = highCutHz;
            highOn = highCutHz < kHighCutOff - 1.0 && highCutHz < 0.45 * sampleRate;
            for (auto& f : lp)
                f.set (melon::Biquad::Type::lowpass, sampleRate, highCutHz, 0.7071);
        }
    }

    void process (float* x, int n, int channel)
    {
        if (! lowOn && ! highOn)
            return;
        auto& h = hp[(size_t) channel];
        auto& l = lp[(size_t) channel];
        for (int i = 0; i < n; ++i)
        {
            double v = x[i];
            if (lowOn)  v = h.process (v);
            if (highOn) v = l.process (v);
            x[i] = (float) v;
        }
    }

    double responseDb (double freq) const
    {
        double db = 0.0;
        if (lowOn)  db += hp[0].responseDb (freq, sampleRate);
        if (highOn) db += lp[0].responseDb (freq, sampleRate);
        return db;
    }

private:
    double sampleRate = 48000.0, lowHz = -1.0, highHz = -1.0;
    bool   lowOn = false, highOn = false;
    std::array<melon::Biquad, kMaxChannels> hp, lp;
};

} // namespace cab
