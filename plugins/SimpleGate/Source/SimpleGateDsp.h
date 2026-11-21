// SimpleGateDsp.h - Melon Simple Gate DSP core, plain C++ (no JUCE) so it can be tested on its own.
//
// A pedal-style noise gate for guitar and bass, after the Fortin Zuul+: three knobs
// (Threshold, Hold, Release) and a Guitar / Bass voicing for the detector. Everything else
// (attack, range, hysteresis, detector filters) is fixed and tuned for instruments.
//
//   in ─┬───────────────────────────────────────────────► × gain ─► Output ─► out
//       │                                                    ▲
//       └─► detector HP/LP ─► peak hold ─► open/close (hysteresis) ─► attack / hold / release
//           (Guitar / Bass voicing)
//
// Both channels share one detector (stereo-linked). No lookahead, so no latency.
#pragma once

#include "MelonDsp.h"
#include <array>
#include <atomic>

namespace sgate
{
enum class Instrument { guitar, bass };

constexpr int    kMaxChannels = 2;
constexpr double kRangeDb     = 90.0; // closed = fully silent, like the pedal
constexpr double kHysteresisDb = 4.0; // closes 4 dB below the threshold, so decaying notes don't chatter
constexpr double kAttackMs    = 0.3;  // time to open fully: fast enough to keep the pick attack
constexpr double kSmoothingMs = 20.0;
constexpr double kFollowerDecayMs = 5.0;    // after the peak hold, the detector falls quickly
constexpr double kRefresh         = 0.7079; // -3 dB: peaks this close to the held one re-arm the hold

// Detector voicing per instrument
struct Voicing
{
    double highPassHz; // ignore hum / rumble below this
    double lowPassHz;  // ignore hiss / fizz above this
    double peakHoldMs; // the detector holds each peak this long: covers one cycle of the lowest note
};
inline Voicing voicing (Instrument i)
{
    return i == Instrument::bass ? Voicing { 25.0, 3000.0, 33.0 }  // down to low B (31 Hz) on a 5-string
                                 : Voicing { 70.0, 6000.0, 17.0 }; // skips 50/60 Hz hum; down to low B (62 Hz) on a 7-string
}

struct Params
{
    double     thresholdDb = -55.0;
    double     holdMs      = 30.0;
    double     releaseMs   = 80.0;
    double     outputDb    = 0.0;
    Instrument instrument  = Instrument::guitar;
    bool       bypassed    = false;
};

// Values for the editor (written by the audio thread, read by the UI)
struct Meters
{
    std::atomic<float> inputDb { -120.0f }; // detector level (peak since the last read)
    std::atomic<float> grDb { 0.0f };       // current gain reduction (<= 0)
    std::atomic<bool>  open { false };

    float takeInput() { return inputDb.exchange (-120.0f); }
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        for (auto* s : { &outGain, &active })
            s->prepare (fs, kSmoothingMs);
        updateVoicing();
        reset();
    }

    void reset()
    {
        for (auto& c : det) { c.hp.reset(); c.lp.reset(); }
        follower = 0.0; grDb = -kRangeDb; holdLeft = 0; peakLeft = 0; open = false;
    }

    void setParams (const Params& p)
    {
        const bool voicingChanged = p.instrument != params.instrument;
        params = p;
        if (voicingChanged)
            updateVoicing();
        outGain.setTarget (melon::dbToGain (p.outputDb));
        active.setTarget (p.bypassed ? 0.0 : 1.0);
    }

    void snapToTargets()
    {
        outGain.snap();
        active.snap();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int    nch      = std::min (numChannels, channels);
        const double openThr  = melon::dbToGain (params.thresholdDb);
        const double closeThr = melon::dbToGain (params.thresholdDb - kHysteresisDb);
        const double atkStep  = kRangeDb / std::max (1.0, kAttackMs * 0.001 * fs);
        const double relStep  = kRangeDb / std::max (1.0, std::max (5.0, params.releaseMs) * 0.001 * fs);
        const int    holdN    = (int) (std::max (0.0, params.holdMs) * 0.001 * fs);
        double       blockPeak = 0.0;

        for (int n = 0; n < numSamples; ++n)
        {
            // --- detector: filtered, rectified, stereo-linked peak follower ---
            double pk = 0.0;
            for (int c = 0; c < nch; ++c)
                pk = std::max (pk, std::abs (det[(size_t) c].lp.process (det[(size_t) c].hp.process (data[c][n]))));
            if (pk > follower)
            {
                follower = pk;
                peakLeft = peakHoldN;
            }
            else if (pk > follower * kRefresh) // a decaying note's next peak keeps the hold going
                peakLeft = peakHoldN;
            else if (peakLeft > 0)
                --peakLeft;
            else
                follower *= followerDecay;
            blockPeak = std::max (blockPeak, follower);

            // --- open / close with hysteresis and hold ---
            if (follower > (open ? closeThr : openThr))
            {
                open     = true;
                holdLeft = holdN;
            }
            else if (holdLeft > 0)
                --holdLeft;
            else
                open = false;

            // --- attack / release (linear in dB) ---
            grDb = open ? std::min (0.0, grDb + atkStep) : std::max (-kRangeDb, grDb - relStep);
            const double gain = grDb <= -kRangeDb + 1e-9 ? 0.0 : melon::dbToGain (grDb);

            const double og = outGain.next(), on = active.next();
            for (int c = 0; c < nch; ++c)
            {
                const double x = data[c][n];
                const double y = x * gain * og;
                data[c][n]     = (float) (x + on * (y - x)); // bypass crossfades to the dry signal
            }
        }

        const float inDb = (float) melon::gainToDb (blockPeak);
        if (inDb > meters.inputDb.load (std::memory_order_relaxed))
            meters.inputDb.store (inDb, std::memory_order_relaxed);
        meters.grDb.store ((float) grDb, std::memory_order_relaxed);
        meters.open.store (open, std::memory_order_relaxed);
    }

    Meters& meterValues() { return meters; }
    double  currentGainReductionDb() const { return grDb; }
    bool    isOpen() const { return open; }

private:
    struct Detector
    {
        melon::Biquad hp, lp;
    };

    void updateVoicing()
    {
        const auto v = voicing (params.instrument);
        for (auto& c : det)
        {
            c.hp.set (melon::Biquad::Type::highpass, fs, v.highPassHz, 0.707);
            c.lp.set (melon::Biquad::Type::lowpass, fs, std::min (v.lowPassHz, 0.45 * fs), 0.707);
        }
        peakHoldN     = (int) (v.peakHoldMs * 0.001 * fs);
        followerDecay = std::exp (-1.0 / (kFollowerDecayMs * 0.001 * fs));
    }

    double fs = 48000.0, follower = 0.0, followerDecay = 0.999, grDb = -kRangeDb;
    int    channels = 2, holdLeft = 0, peakLeft = 0, peakHoldN = 800;
    bool   open = false;
    Params params;
    std::array<Detector, kMaxChannels> det;
    melon::Smoother outGain, active;
    Meters meters;
};

} // namespace sgate
