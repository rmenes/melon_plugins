// RackDsp.h - the plain-C++ stages of Melon Rack (no JUCE, no neural engine):
// noise gate, 6-band EQ and the latency-compensated bypass delay.
// The neural model slots and the cabinet live in the processor.
#pragma once

#include <MelonDsp.h>
#include <array>

namespace rack
{
//==============================================================================
// Simple guitar noise gate: peak follower, 4 dB hysteresis, smooth open/close.
class Gate
{
public:
    void prepare (double fs)
    {
        sampleRate = fs;
        envRelease = std::exp (-1.0 / (0.010 * fs));
        gainOpen   = 1.0 - std::exp (-1.0 / (0.001 * fs));
        gainClose  = 1.0 - std::exp (-1.0 / (0.030 * fs));
        hold       = (int) (0.030 * fs);
        reset();
    }
    void reset() { env = 0.0; gain = 0.0; open = false; holdCount = 0; }

    void setThresholdDb (double db) { threshold = melon::dbToGain (db); hysteresis = melon::dbToGain (db - 4.0); }

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

            const double target = open ? 1.0 : 0.0;
            gain += (target - gain) * (open ? gainOpen : gainClose);
            x[i] = (float) (x[i] * gain);
        }
    }

    bool isOpen() const { return open; }

private:
    double sampleRate = 48000.0, threshold = 0.001, hysteresis = 0.0006;
    double env = 0.0, envRelease = 0.999, gain = 0.0, gainOpen = 0.02, gainClose = 0.001;
    int    hold = 1440, holdCount = 0;
    bool   open = false;
};

//==============================================================================
// Six peaking bands tuned for guitar, +/-12 dB each.
class Eq6
{
public:
    static constexpr int kBands = 6;
    static constexpr std::array<double, kBands> kFreqs { 100.0, 220.0, 500.0, 1200.0, 2700.0, 6000.0 };
    static constexpr double kQ = 1.1;

    void prepare (double fs)
    {
        sampleRate = fs;
        for (int b = 0; b < kBands; ++b)
            setBand (b, gainDb[(size_t) b], true);
        reset();
    }
    void reset()
    {
        for (auto& f : filters)
            f.reset();
    }

    void setBand (int b, double db, bool force = false)
    {
        auto& g = gainDb[(size_t) b];
        if (! force && std::abs (g - db) < 1e-4)
            return;
        g = db;
        if (std::abs (db) < 0.01)
            filters[(size_t) b].setIdentity();
        else
            filters[(size_t) b].set (melon::Biquad::Type::peak, sampleRate, kFreqs[(size_t) b], kQ, db);
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
    double sampleRate = 48000.0;
    std::array<double, kBands>       gainDb {};
    std::array<melon::Biquad, kBands> filters;
};

//==============================================================================
// Integer delay used to keep the dry signal in time with a stage that has latency
// (switched-off model slots and the global bypass).
class CompensationDelay
{
public:
    void prepare (int maxDelay) { line.prepare (std::max (1, maxDelay)); }
    void reset() { line.reset(); }
    void setDelay (int samples) { delay = std::max (0, samples); }
    int  getDelay() const { return delay; }

    void process (float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            line.push (x[i]);
            x[i] = line.readInt (delay);
        }
    }
    float processSample (float v)
    {
        line.push (v);
        return line.readInt (delay);
    }

private:
    melon::DelayLine line;
    int delay = 0;
};

} // namespace rack
