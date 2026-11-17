// GateDsp.h - Melon Gates of Doom DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// A gate / expander with the controls of a modern mastering-grade gate (FabFilter Pro-G style):
// Threshold, Range, Ratio, Knee, Hysteresis, Attack, Hold, Release, Lookahead, side-chain
// filters, four detection modes and Audition. Signal flow:
//
//   in ─┬──────────────────────────► lookahead delay ─────────────────► × gain ─► Mix ─► out
//       │                                                                  ▲
//       └─► side-chain HP/LP ─► level detector ─► gain computer ─► attack / hold / release
//                                                 (threshold, ratio, knee, range, hysteresis)
//
// Both channels share one detector (stereo-linked), so the stereo image never shifts.
// Gain changes are smoothed in decibels, so fades sound natural.
#pragma once

#include "MelonDsp.h"
#include <array>
#include <atomic>

namespace gate
{
enum class Mode { classic, clean, guitar, ducking };

constexpr double kMaxLookaheadMs = 10.0;
constexpr double kGuitarExtraHyst = 4.0; // Guitar mode closes 4 dB lower than it opens (no chatter on decaying notes)
constexpr double kSmoothingMs     = 20.0;
constexpr int    kMaxChannels     = 2;
constexpr int    kMeterRate       = 100; // meter points per second (for the scrolling display)

struct Params
{
    double thresholdDb = -40.0;
    double rangeDb     = 60.0;   // maximum attenuation
    double ratio       = 100.0;  // 1 = no effect, 100 = hard gate
    double kneeDb      = 3.0;
    double hysteresisDb = 3.0;
    double attackMs    = 0.5;
    double holdMs      = 50.0;
    double releaseMs   = 150.0;
    double lookaheadMs = 0.0;
    double scHighPassHz = 20.0;   // 20 = off
    double scLowPassHz  = 20000.0; // 20000 = off
    double mix         = 1.0;
    double outputDb    = 0.0;
    Mode   mode        = Mode::classic;
    bool   audition    = false;
    bool   bypassed    = false;
};

//==============================================================================
// Static gain curve: attenuation in dB (<= 0) for a detector level in dB.
inline double gainComputerDb (double levelDb, double thrDb, double ratio, double kneeDb, double rangeDb, bool ducking)
{
    ratio = std::max (1.0, ratio);
    const double slope = ratio - 1.0;
    const double over  = ducking ? thrDb - levelDb : levelDb - thrDb; // < 0 = in the "gated" region
    const double w     = std::max (0.0, kneeDb);
    double gr;
    if (over >= w * 0.5)
        gr = 0.0;
    else if (w > 0.0 && over > -w * 0.5)
    {
        const double t = over - w * 0.5;
        gr = -slope * t * t / (2.0 * w);
    }
    else
        gr = slope * over;
    return std::max (-std::abs (rangeDb), gr);
}

//==============================================================================
// Lock-free single-producer / single-consumer queue of meter points for the display.
struct MeterPoint
{
    float inDb, outDb, grDb;
};

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
    // Copies points written since `readIndex` (at most kSize) and advances it
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
        channels = std::clamp (numChannels, 1, kMaxChannels);
        for (auto& d : delay)
            d.prepare ((int) std::ceil (kMaxLookaheadMs * 0.001 * fs) + 4);
        for (auto* s : smoothers())
            s->prepare (fs, kSmoothingMs);
        meterEvery = std::max (1, (int) (fs / kMeterRate));
        updateFilters();
        reset();
    }

    void reset()
    {
        for (auto& d : delay) d.reset();
        for (auto& c : sc) { c.hp.reset(); c.lp.reset(); }
        env = -120.0; grDb = 0.0; holdLeft = 0; open = false;
        rmsState = 0.0;
        meterCount = 0; mIn = mOut = -120.0f; mGr = 0.0f;
    }

    void setParams (const Params& p)
    {
        const bool filtersChanged = std::abs (p.scHighPassHz - params.scHighPassHz) > 1e-6
                                    || std::abs (p.scLowPassHz - params.scLowPassHz) > 1e-6;
        params = p;
        if (filtersChanged)
            updateFilters();
        mix.setTarget (std::clamp (p.mix, 0.0, 1.0));
        outGain.setTarget (melon::dbToGain (p.outputDb));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
        lookahead = (int) std::lround (std::clamp (p.lookaheadMs, 0.0, kMaxLookaheadMs) * 0.001 * fs);
    }

    void snapToTargets()
    {
        for (auto* s : smoothers())
            s->snap();
    }

    // Latency in samples (the lookahead), for the host
    int latencySamples() const { return lookahead; }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int  nch      = std::min (numChannels, channels);
        const auto mode     = params.mode;
        const bool ducking  = mode == Mode::ducking;
        const double hyst   = std::max (0.0, params.hysteresisDb) + (mode == Mode::guitar ? kGuitarExtraHyst : 0.0);

        // gain ballistics: Attack / Release = time to travel the whole Range (linear in dB, like most gates)
        const double span   = std::max (1.0, std::abs (params.rangeDb));
        const double atk    = span / std::max (1.0, std::max (0.01, params.attackMs) * (mode == Mode::clean ? 2.0 : 1.0) * 0.001 * fs);
        const double rel    = span / std::max (1.0, std::max (1.0, params.releaseMs) * (mode == Mode::clean ? 1.5 : 1.0) * 0.001 * fs);
        const double envRel = coeff (mode == Mode::guitar ? 5.0 : 10.0);
        const double rmsCo  = coeff (5.0);
        const int    holdN  = (int) (std::max (0.0, params.holdMs) * 0.001 * fs);

        for (int n = 0; n < numSamples; ++n)
        {
            // --- side-chain level (stereo-linked) ---
            double scPeak = 0.0, scSample[kMaxChannels] {};
            for (int c = 0; c < nch; ++c)
            {
                const double s = sc[(size_t) c].lp.process (sc[(size_t) c].hp.process (data[c][n]));
                scSample[c]    = s;
                scPeak         = std::max (scPeak, std::abs (s));
            }

            double levelDb;
            if (mode == Mode::clean)
            {
                rmsState += rmsCo * (scPeak * scPeak - rmsState);
                levelDb = melon::gainToDb (std::sqrt (2.0 * rmsState)); // sqrt(2): a sine reads its peak level
            }
            else
            {
                const double pk = melon::gainToDb (scPeak);
                env = pk > env ? pk : env + envRel * (pk - env); // instant attack, short release
                levelDb = env;
            }

            // --- gain computer with hysteresis ---
            // once triggered, the threshold drops by the hysteresis until the gate (or ducker) releases
            const bool   triggered = ducking ? ! open : open;
            const double thr       = params.thresholdDb - (triggered ? hyst : 0.0);
            const double target = gainComputerDb (levelDb, thr, params.ratio, params.kneeDb, params.rangeDb, ducking);
            open = target > -0.01;

            // --- attack / hold / release ---
            // (gate: opening = attack; ducker: turning down = attack. Hold delays the other direction.)
            const bool attackPhase = ducking ? target < grDb : target > grDb;
            if (attackPhase)
            {
                grDb     = moveTowards (grDb, target, atk);
                holdLeft = holdN;
            }
            else if (holdLeft > 0)
                --holdLeft;
            else
                grDb = moveTowards (grDb, target, rel);
            const double gain = melon::dbToGain (grDb);

            const double mx = mix.next(), og = outGain.next(), on = active.next();
            double outPeak = 0.0, inPeak = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                auto& d = delay[(size_t) c];
                d.push (data[c][n]);
                const double x   = d.readInt (lookahead); // audio, delayed by the lookahead
                const double wet = params.audition ? scSample[c] : x * gain;
                const double y   = ((1.0 - mx) * x + mx * wet) * og;
                // bypass: crossfade to the delayed dry signal so the latency stays constant
                const double out = x + on * (y - x);
                data[c][n] = (float) out;
                inPeak  = std::max (inPeak, std::abs (x));
                outPeak = std::max (outPeak, std::abs (out));
            }

            // --- meter points ---
            mIn  = std::max (mIn, (float) melon::gainToDb (inPeak));
            mOut = std::max (mOut, (float) melon::gainToDb (outPeak));
            mGr  = std::min (mGr, (float) grDb);
            if (++meterCount >= meterEvery)
            {
                meters.push ({ mIn, mOut, mGr });
                meterCount = 0; mIn = mOut = -120.0f; mGr = 0.0f;
            }
        }
        lastGr = grDb;
    }

    double currentGainReductionDb() const { return lastGr; }
    const MeterQueue& meterQueue() const { return meters; }

private:
    struct SideChain
    {
        melon::Biquad hp, lp;
    };

    static double moveTowards (double v, double target, double step)
    {
        return v < target ? std::min (target, v + step) : std::max (target, v - step);
    }

    double coeff (double ms) const { return 1.0 - std::exp (-1.0 / std::max (1.0, ms * 0.001 * fs)); }

    void updateFilters()
    {
        for (auto& c : sc)
        {
            if (params.scHighPassHz > 20.5) c.hp.set (melon::Biquad::Type::highpass, fs, params.scHighPassHz, 0.707);
            else                            c.hp.setIdentity();
            if (params.scLowPassHz < 19999.0) c.lp.set (melon::Biquad::Type::lowpass, fs, params.scLowPassHz, 0.707);
            else                              c.lp.setIdentity();
        }
    }

    std::vector<melon::Smoother*> smoothers() { return { &mix, &outGain, &active }; }

    double fs = 48000.0, env = -120.0, grDb = 0.0, rmsState = 0.0, lastGr = 0.0;
    int    channels = 2, lookahead = 0, holdLeft = 0, meterEvery = 480, meterCount = 0;
    bool   open = false;
    float  mIn = -120.0f, mOut = -120.0f, mGr = 0.0f;
    Params params;
    std::array<melon::DelayLine, kMaxChannels> delay;
    std::array<SideChain, kMaxChannels> sc;
    melon::Smoother mix, outGain, active;
    MeterQueue meters;
};

} // namespace gate
