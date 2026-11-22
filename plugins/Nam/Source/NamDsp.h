// NamDsp.h - the plain-C++ stages of Melon NAM (no JUCE, no neural engine):
// noise gate, the Bass / Middle / Treble tone stack and the latency-compensated bypass delay.
// The model itself lives in NeuralModelSlot (NeuralAudio engine).
#pragma once

#include <MelonDsp.h>
#include <array>

namespace mnam
{
//==============================================================================
// Simple guitar noise gate: peak follower, 4 dB hysteresis, 30 ms hold, smooth open/close.
class Gate
{
public:
    void prepare (double fs)
    {
        envRelease = std::exp (-1.0 / (0.010 * fs));
        gainOpen   = 1.0 - std::exp (-1.0 / (0.001 * fs));
        gainClose  = 1.0 - std::exp (-1.0 / (0.030 * fs));
        hold       = (int) (0.030 * fs);
        reset();
    }
    void reset() { env = 0.0; gain = 0.0; open = false; holdCount = 0; }

    void setThresholdDb (double db)
    {
        threshold  = melon::dbToGain (db);
        hysteresis = melon::dbToGain (db - 4.0);
    }

    void process (float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const double a = std::abs ((double) x[i]);
            env = a > env ? a : env * envRelease;

            if (env >= threshold)
            {
                open      = true;
                holdCount = hold;
            }
            else if (env < hysteresis)
            {
                if (holdCount > 0)
                    --holdCount;
                else
                    open = false;
            }

            gain += ((open ? 1.0 : 0.0) - gain) * (open ? gainOpen : gainClose);
            x[i] = (float) (x[i] * gain);
        }
    }

    bool isOpen() const { return open; }

private:
    double threshold = 0.0001, hysteresis = 0.00006;
    double env = 0.0, envRelease = 0.999, gain = 0.0, gainOpen = 0.02, gainClose = 0.001;
    int    hold = 1440, holdCount = 0;
    bool   open = false;
};

//==============================================================================
// The NAM plugin's tone stack: Bass, Middle and Treble knobs from 0 to 10, 5 = flat.
//   Bass   low shelf  150 Hz, 4 dB per step (+/-20 dB)
//   Middle peak       425 Hz, 3 dB per step (+/-15 dB), narrower when cutting
//   Treble high shelf 1.8 kHz, 2 dB per step (+/-10 dB)
class ToneStack
{
public:
    static constexpr double kBassHz = 150.0, kMidHz = 425.0, kTrebleHz = 1800.0;

    static double bassDb (double knob)   { return 4.0 * (knob - 5.0); }
    static double middleDb (double knob) { return 3.0 * (knob - 5.0); }
    static double trebleDb (double knob) { return 2.0 * (knob - 5.0); }

    void prepare (double fs)
    {
        sampleRate = fs;
        knobs      = { -1.0, -1.0, -1.0 }; // force a recalculation
        set (5.0, 5.0, 5.0);
        reset();
    }
    void reset()
    {
        for (auto& f : filters)
            f.reset();
    }

    void set (double bass, double middle, double treble)
    {
        using T = melon::Biquad::Type;
        if (bass != knobs[0])
        {
            knobs[0] = bass;
            filters[0].set (T::lowShelf, sampleRate, kBassHz, 0.707, bassDb (bass));
        }
        if (middle != knobs[1])
        {
            knobs[1] = middle;
            const double db = middleDb (middle);
            filters[1].set (T::peak, sampleRate, kMidHz, db < 0.0 ? 1.5 : 0.7, db);
        }
        if (treble != knobs[2])
        {
            knobs[2] = treble;
            filters[2].set (T::highShelf, sampleRate, kTrebleHz, 0.707, trebleDb (treble));
        }
    }

    void process (float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            double v = x[i];
            for (auto& f : filters)
                v = f.process (v);
            x[i] = (float) v;
        }
    }

    double responseDb (double freq) const
    {
        double db = 0.0;
        for (auto& f : filters)
            db += f.responseDb (freq, sampleRate);
        return db;
    }

private:
    double                          sampleRate = 48000.0;
    std::array<double, 3>           knobs { -1.0, -1.0, -1.0 };
    std::array<melon::Biquad, 3>    filters;
};

//==============================================================================
// Integer delay that keeps the dry signal in time with the model's latency (for the bypass crossfade).
class CompensationDelay
{
public:
    void prepare (int maxDelay) { line.prepare (std::max (1, maxDelay)); }
    void reset() { line.reset(); }
    void setDelay (int samples) { delay = std::max (0, std::min (samples, line.capacity())); }
    int  getDelay() const { return delay; }

    void process (float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            line.push (x[i]);
            x[i] = line.readInt (delay);
        }
    }

private:
    melon::DelayLine line;
    int delay = 0;
};

} // namespace mnam
