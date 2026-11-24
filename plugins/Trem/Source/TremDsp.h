// TremDsp.h - Melon Trem DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// A tremolo in the vein of the Boss TR-2: three knobs, Rate, Wave and Depth.
// The LFO is a triangle that the Wave knob pushes into a soft clipper, so it morphs
// from a smooth, slightly rounded triangle (Wave at 0) to a choppy square (Wave at 100),
// like the pedal's wave control. Depth sets how far the volume drops at the bottom of
// each cycle; at full depth the signal is cut to silence.
//
//   in ─► × gain(LFO) ─► Output ─► out
//
// Extras over the pedal: host Sync / Tap (set in the processor) and a Stereo mode
// (Mono: both channels together, Pan: the right channel runs in anti-phase = auto-pan).
// No delay, no filtering of the audio path, so no latency and no tail.
#pragma once

#include "MelonDsp.h"
#include <array>
#include <atomic>

namespace trem
{
constexpr int    kMaxChannels = 2;
constexpr double kMinRateHz   = 0.3;
constexpr double kMaxRateHz   = 15.0;
constexpr double kSmoothingMs = 20.0;
constexpr double kEdgeHz      = 350.0; // gain smoothing: rounds the square's edges so they don't click

enum Stereo { mono, pan, kNumStereoModes };

// Host sync values (same list as Melon MultiMod)
inline const std::array<std::pair<const char*, double>, 16>& syncValues()
{
    static const std::array<std::pair<const char*, double>, 16> v { {
        { "Off", 0.0 }, { "4 bars", 16.0 }, { "2 bars", 8.0 }, { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/2 D", 3.0 },
        { "1/2 T", 4.0 / 3.0 }, { "1/4", 1.0 }, { "1/4 D", 1.5 }, { "1/4 T", 2.0 / 3.0 }, { "1/8", 0.5 },
        { "1/8 D", 0.75 }, { "1/8 T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/16 T", 1.0 / 6.0 }, { "1/32", 0.125 },
    } };
    return v;
}
enum SyncIndex { syncOff = 0, sync1_8 = 10, sync1_8T = 12, sync1_16 = 13 };

inline double syncLengthQuarters (int syncIndex)
{
    const auto& v = syncValues();
    return (syncIndex <= 0 || syncIndex >= (int) v.size()) ? 0.0 : v[(size_t) syncIndex].second;
}

// LFO rate for a sync value at `bpm` (0 = not synced)
inline double syncedHz (int syncIndex, double bpm)
{
    const double q = syncLengthQuarters (syncIndex);
    if (q <= 0.0 || bpm <= 0.0)
        return 0.0;
    return std::clamp (bpm / 60.0 / q, 0.01, 40.0);
}

// LFO shape: 0..1 at `phase` (cycles, 0 = top of the cycle) for Wave 0..1.
// Triangle into a tanh clipper whose drive grows with Wave (0.75 -> 45).
inline double lfoShape (double phase, double wave)
{
    phase = phase - std::floor (phase);
    const double tri   = 4.0 * std::abs (phase - 0.5) - 1.0; // +1 at phase 0, -1 at phase 0.5
    const double drive = 0.75 * std::pow (60.0, std::clamp (wave, 0.0, 1.0));
    return 0.5 * (std::tanh (tri * drive) / std::tanh (drive) + 1.0);
}

// Volume (0..1) for an LFO value and Depth 0..1: 1 at the top, 1 - depth at the bottom.
inline double tremGain (double lfo, double depth) { return 1.0 - std::clamp (depth, 0.0, 1.0) * (1.0 - lfo); }

struct Params
{
    double rateHz   = 4.0;  // already resolved from Sync + host tempo
    double wave     = 0.5;  // 0 = triangle, 1 = square
    double depth    = 0.5;  // 0..1
    int    stereo   = mono;
    double outputDb = 0.0;
    bool   bypassed = false;
};

// Values for the editor (written by the audio thread, read by the UI)
struct Meters
{
    std::atomic<double> phase { 0.0 };
    std::atomic<float>  gainL { 1.0f }, gainR { 1.0f };
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        for (auto* s : { &rate, &wave, &depth, &spread, &outGain, &active })
            s->prepare (fs, kSmoothingMs);
        edgeCoeff = 1.0 - std::exp (-melon::kTwoPi * kEdgeHz / fs);
        reset();
    }

    void reset()
    {
        phase      = 0.0;
        edgePrimed = false;
    }

    void setParams (const Params& p)
    {
        params = p;
        rate.setTarget (std::clamp (p.rateHz, 0.01, 40.0));
        wave.setTarget (p.wave);
        depth.setTarget (p.depth);
        spread.setTarget (p.stereo == pan ? 0.5 : 0.0);
        outGain.setTarget (melon::dbToGain (p.outputDb));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
    }

    void snapToTargets()
    {
        for (auto* s : { &rate, &wave, &depth, &spread, &outGain, &active })
            s->snap();
    }

    // Host sync: LFO position in cycles (follows the host beat while it plays)
    void   setHostPosition (double cycles) { phase = cycles - std::floor (cycles); }
    double getPosition() const { return phase; }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int nch = std::min (numChannels, channels);
        double    gl = 1.0, gr = 1.0;

        for (int n = 0; n < numSamples; ++n)
        {
            const double w = wave.next(), d = depth.next(), sp = spread.next();
            const double og = outGain.next(), on = active.next();

            std::array<double, kMaxChannels> target {
                tremGain (lfoShape (phase, w), d),
                tremGain (lfoShape (phase + sp, w), d)
            };
            if (! edgePrimed) // start at the first value instead of fading in from silence
            {
                edge       = target;
                edgePrimed = true;
            }
            for (size_t c = 0; c < kMaxChannels; ++c)
                edge[c] += edgeCoeff * (target[c] - edge[c]);
            gl = edge[0];
            gr = edge[1];

            for (int c = 0; c < nch; ++c)
            {
                const double x = data[c][n];
                const double y = x * (c == 0 ? gl : gr) * og;
                data[c][n]     = (float) (x + on * (y - x)); // bypass crossfades to the dry signal
            }

            phase += rate.next() / fs;
            if (phase >= 1.0)
                phase -= std::floor (phase);
        }

        meters.phase.store (phase, std::memory_order_relaxed);
        meters.gainL.store ((float) gl, std::memory_order_relaxed);
        meters.gainR.store ((float) (nch > 1 ? gr : gl), std::memory_order_relaxed);
    }

    Meters& meterValues() { return meters; }

private:
    double fs = 48000.0, phase = 0.0, edgeCoeff = 1.0;
    int    channels = 2;
    bool   edgePrimed = false;
    Params params;
    std::array<double, kMaxChannels> edge {};
    melon::Smoother rate, wave, depth, spread, outGain, active;
    Meters meters;
};

} // namespace trem
