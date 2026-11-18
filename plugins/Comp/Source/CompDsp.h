// CompDsp.h - Melon Comp DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// Emulates a Keeley Compressor Plus style pedal: an OTA feedback compressor in the
// Ross / Dyna Comp lineage with a parallel Blend, an emphasis-style Tone control and a
// Single Coil / Humbucker attack-release switch.
//
//         ┌──────────────────────────── dry (in phase) ───────────────────────────┐
//   in ───┤                                                                       Blend ─► Level ─► out
//         └─► OTA gain cell ─► Sustain gain ─┬─► Tone (emphasis shelf) ─► ────────┘
//                ▲                           │
//                └──── bias ◄── cap ◄── rectifier (attack / release set by the switch)
//
// The detector listens to the cell's OUTPUT (feedback), like the original circuit. Sustain is
// the gain after the cell, inside the loop: more Sustain both squeezes the peaks harder and lifts
// quiet, decaying notes ("expander / sustainer" behaviour). The OTA input has a soft tanh curve,
// and the whole cell runs 4x oversampled so the fast attack does not alias.
#pragma once

#include "MelonDsp.h"
#include <array>
#include <atomic>

namespace comp
{
enum class Pickup { singleCoil, humbucker };

constexpr int    kMaxChannels   = 2;
constexpr int    kMeterRate     = 100;   // meter points per second (for the scrolling display)
constexpr double kSmoothingMs   = 20.0;
constexpr double kMaxSustainDb  = 40.0;  // loop gain at Sustain = 100 %
constexpr double kThreshold     = 0.1;   // cell output level (about -20 dBFS) where compression sets in
constexpr double kOtaHeadroom   = 0.7;   // OTA input soft-clip level
constexpr double kRail          = 1.0;   // the gain stage after the cell runs into its supply rails here
constexpr double kToneFreq      = 2200.0;
constexpr double kToneRangeDb   = 9.0;   // Tone knob: -9 dB (dark) ... 0 (noon) ... +9 dB (bright)
constexpr double kBandwidthHz   = 12000.0; // gentle pedal bandwidth on the compressed path
constexpr double kWetTrim       = 0.5;   // compressed path sits ~6 dB under the dry peaks, so Blend lets the attack poke through

struct Ballistics
{
    double attackMs, releaseMs;
};

// Switch positions. Humbuckers have more average energy, so they get a quicker release
// (otherwise the compressor stays clamped down and the notes lose their definition).
inline Ballistics ballisticsFor (Pickup p)
{
    return p == Pickup::humbucker ? Ballistics { 1.0, 120.0 } : Ballistics { 2.0, 380.0 };
}

struct Params
{
    double sustain  = 0.5; // 0..1
    double level    = 0.6; // 0..1, audio taper, 0.5 = unity (0.6 = +3 dB)
    double blend    = 0.75; // 0 = dry, 1 = compressed
    double tone     = 0.5; // 0..1, 0.5 = flat
    Pickup pickup   = Pickup::singleCoil;
    double outputDb = 0.0;
    bool   bypassed = false;
};

inline double sustainGain (double sustain) { return melon::dbToGain (std::clamp (sustain, 0.0, 1.0) * kMaxSustainDb); }
inline double levelGain (double level)     { const double l = std::clamp (level, 0.0, 1.0); return 4.0 * l * l; } // 0.5 -> 1.0, 1.0 -> +12 dB

// Gain of the OTA cell for a given detector voltage: smooth, knee-less curve like the transistor
// bias stage of the original. With the feedback loop this gives roughly 4:1 well above the threshold.
inline double cellGain (double env)
{
    const double r = env / kThreshold;
    return 1.0 / (1.0 + r * r * r);
}

//==============================================================================
struct MeterPoint
{
    float inDb, outDb, grDb;
};

// Lock-free single-producer / single-consumer queue of meter points for the display.
class MeterQueue
{
public:
    static constexpr int kSize = 1024;
    void push (const MeterPoint& p)
    {
        const int w = write.load (std::memory_order_relaxed);
        points[(size_t) (w % kSize)] = p;
        write.store (w + 1, std::memory_order_release);
    }
    template <class Fn>
    void drain (int& readIndex, Fn&& fn) const
    {
        const int w = write.load (std::memory_order_acquire);
        if (w - readIndex > kSize) readIndex = w - kSize;
        for (; readIndex < w; ++readIndex)
            fn (points[(size_t) (readIndex % kSize)]);
    }

private:
    std::array<MeterPoint, kSize> points {};
    std::atomic<int> write { 0 };
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        fsHi     = fs * melon::Oversampler4x::kFactor;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        for (auto* s : smoothers())
            s->prepare (fs, kSmoothingMs);
        for (auto& c : ch)
        {
            c.os.prepare (fs);
            c.band.set (melon::Biquad::Type::lowpass, fs, kBandwidthHz, 0.707);
        }
        meterEvery = std::max (1, (int) (fs / kMeterRate));
        lastTone   = -1.0;
        updateTone (params.tone);
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.os.reset(); c.shelf.reset(); c.band.reset();
        }
        env = 0.0; lastGrDb = 0.0;
        meterCount = 0; mIn = mOut = -120.0f; mGr = 0.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        loopGain.setTarget (sustainGain (p.sustain));
        blend.setTarget (std::clamp (p.blend, 0.0, 1.0));
        level.setTarget (levelGain (p.level));
        outGain.setTarget (melon::dbToGain (p.outputDb));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
        updateTone (p.tone);
    }

    void snapToTargets()
    {
        for (auto* s : smoothers())
            s->snap();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int  nch = std::min (numChannels, channels);
        const auto b   = ballisticsFor (params.pickup);
        // capacitor charge / discharge, at the oversampled rate
        const double atk = 1.0 - std::exp (-1.0 / (b.attackMs * 0.001 * fsHi));
        const double rel = 1.0 - std::exp (-1.0 / (b.releaseMs * 0.001 * fsHi));
        constexpr int K  = melon::Oversampler4x::kFactor;

        for (int n = 0; n < numSamples; ++n)
        {
            const double L = loopGain.next(), bl = blend.next(), lv = level.next(), og = outGain.next(), on = active.next();

            // upsample every channel
            double up[kMaxChannels][K];
            for (int c = 0; c < nch; ++c)
                ch[(size_t) c].os.upsample (data[c][n], up[c]);

            // OTA cell + feedback detector (stereo-linked: one capacitor for both channels)
            double cell[kMaxChannels][K];
            double grMin = 1.0;
            for (int k = 0; k < K; ++k)
            {
                const double g = cellGain (env);
                grMin = std::min (grMin, g);
                double rect = 0.0;
                for (int c = 0; c < nch; ++c)
                {
                    const double v = L * g * kOtaHeadroom * std::tanh (up[c][k] / kOtaHeadroom);
                    const double y = kRail * std::tanh (v / kRail); // tames the attack spike, like the pedal's op-amp
                    cell[c][k] = y;
                    rect = std::max (rect, std::abs (y));
                }
                env += (rect > env ? atk : rel) * (rect - env);
            }

            double inPeak = 0.0, outPeak = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                auto& st        = ch[(size_t) c];
                const double x  = data[c][n];
                const double cmp = kWetTrim * st.band.process (st.shelf.process (st.os.downsample (cell[c])));
                const double y   = ((1.0 - bl) * x + bl * cmp) * lv * og;
                const double out = x + on * (y - x);
                data[c][n] = (float) out;
                inPeak  = std::max (inPeak, std::abs (x));
                outPeak = std::max (outPeak, std::abs (out));
            }

            const double grDb = melon::gainToDb (grMin);
            mIn  = std::max (mIn, (float) melon::gainToDb (inPeak));
            mOut = std::max (mOut, (float) melon::gainToDb (outPeak));
            mGr  = std::min (mGr, (float) grDb);
            lastGrDb = grDb;
            if (++meterCount >= meterEvery)
            {
                meters.push ({ mIn, mOut, mGr });
                meterCount = 0; mIn = mOut = -120.0f; mGr = 0.0f;
            }
        }
    }

    double currentGainReductionDb() const { return lastGrDb; }
    const MeterQueue& meterQueue() const { return meters; }

private:
    struct Channel
    {
        melon::Oversampler4x os;
        melon::Biquad        shelf, band;
    };

    void updateTone (double tone)
    {
        if (std::abs (tone - lastTone) < 1e-4 || fs <= 0.0)
            return;
        lastTone = tone;
        const double db = (std::clamp (tone, 0.0, 1.0) * 2.0 - 1.0) * kToneRangeDb;
        for (auto& c : ch)
            c.shelf.set (melon::Biquad::Type::highShelf, fs, kToneFreq, 0.6, db);
    }

    std::array<melon::Smoother*, 5> smoothers() { return { &loopGain, &blend, &level, &outGain, &active }; }

    Params params;
    double fs = 48000.0, fsHi = 192000.0, env = 0.0, lastTone = -1.0, lastGrDb = 0.0;
    int    channels = 2;
    std::array<Channel, kMaxChannels> ch;
    melon::Smoother loopGain, blend, level, outGain, active;

    MeterQueue meters;
    int   meterEvery = 480, meterCount = 0;
    float mIn = -120.0f, mOut = -120.0f, mGr = 0.0f;
};

} // namespace comp
