// TunerDsp.h - Melon Tuner DSP: pitch detection, note maths and tunings (plain C++17, no JUCE).
//
// Pitch detection:
//   input (mono sum) ─► 4th-order low-pass ─┬─► full-rate ring ───────────────┐
//                                           └─► decimate to ~11 kHz ─► YIN ───┴─► refine at full rate ─► tracker
//
// - YIN (de Cheveigné & Kawahara) runs on a decimated copy so it stays cheap enough for a
//   Raspberry Pi, then the period is refined on the full-rate signal around YIN's estimate
//   (parabolic interpolation), which gives well under 1 cent of error on a steady note.
// - Range 25 Hz – 1.4 kHz: low B of a 5/6-string bass (30.9 Hz) up to the 24th fret of the
//   high E string (1318.5 Hz).
// - The tracker rejects single-frame octave jumps and holds the last note briefly when the
//   signal drops out, so the display doesn't flicker.
// - The audio itself is only passed through (Mute, Output gain, Bypass).
#pragma once

#include <MelonDsp.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace tuner
{
constexpr double kMinFreq        = 25.0;
constexpr double kMaxFreq        = 1400.0;
constexpr double kDecimatedRate  = 11025.0; // target analysis rate
constexpr double kLowpassHz      = 1800.0;
constexpr double kAnalysisRate   = 50.0;  // analyses per second (20 ms hop)
constexpr double kYinThreshold   = 0.15;  // first dip below this is taken as the period
constexpr double kMaxAperiodic   = 0.40;  // above this the frame has no clear pitch
constexpr int    kHoldFrames     = 4;     // frames a note is held after the signal goes (80 ms)
constexpr int    kMaxChannels    = 2;
constexpr double kSmoothingMs    = 15.0;

//==============================================================================
// Note maths
inline double midiFromFreq (double freq, double refA4) { return 69.0 + 12.0 * std::log2 (freq / refA4); }
inline double freqFromMidi (double midi, double refA4) { return refA4 * std::pow (2.0, (midi - 69.0) / 12.0); }
inline double centsBetween (double freq, double target) { return 1200.0 * std::log2 (freq / target); }

inline std::string noteName (int midi, bool flats = false)
{
    static const char* sharps[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatN[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const int pc = ((midi % 12) + 12) % 12;
    return flats ? flatN[pc] : sharps[pc];
}
inline int octaveOf (int midi) { return (int) std::floor (midi / 12.0) - 1; } // MIDI 60 = C4

//==============================================================================
// Tunings (strings listed from the lowest to the highest, as MIDI notes)
struct Tuning
{
    const char*      name;
    std::vector<int> strings; // empty = chromatic
    bool             flats = false;
};

inline const std::vector<Tuning>& tunings()
{
    static const std::vector<Tuning> list = {
        { "Chromatic", {} },
        { "Guitar Standard", { 40, 45, 50, 55, 59, 64 } },
        { "Guitar Eb Standard", { 39, 44, 49, 54, 58, 63 }, true },
        { "Guitar D Standard", { 38, 43, 48, 53, 57, 62 } },
        { "Guitar Drop D", { 38, 45, 50, 55, 59, 64 } },
        { "Guitar Drop C", { 36, 43, 48, 53, 57, 62 } },
        { "Guitar DADGAD", { 38, 45, 50, 55, 57, 62 } },
        { "Guitar Open G", { 38, 43, 50, 55, 59, 62 } },
        { "Guitar Open D", { 38, 45, 50, 54, 57, 62 } },
        { "Guitar 7-String", { 35, 40, 45, 50, 55, 59, 64 } },
        { "Bass Standard", { 28, 33, 38, 43 } },
        { "Bass 5-String", { 23, 28, 33, 38, 43 } },
        { "Bass 6-String", { 23, 28, 33, 38, 43, 48 } },
        { "Bass Drop D", { 26, 33, 38, 43 } },
        { "Bass Eb Standard", { 27, 32, 37, 42 }, true },
    };
    return list;
}

// What the display shows for a detected frequency.
struct Target
{
    int    midi   = 69;   // note to tune to
    int    string = -1;   // index in the tuning (-1 = chromatic)
    double cents  = 0.0;  // detected pitch relative to the target note
    double targetFreq = 440.0;
};

inline Target findTarget (double freq, double refA4, int tuningIndex)
{
    Target t;
    const auto& list = tunings();
    const auto& tun  = list[(size_t) std::clamp (tuningIndex, 0, (int) list.size() - 1)];
    const double m   = midiFromFreq (freq, refA4);
    if (tun.strings.empty())
    {
        t.midi = (int) std::lround (m);
    }
    else
    {
        double best = 1e9;
        for (size_t i = 0; i < tun.strings.size(); ++i)
            if (std::abs (m - tun.strings[i]) < best)
            {
                best     = std::abs (m - tun.strings[i]);
                t.midi   = tun.strings[i];
                t.string = (int) i;
            }
    }
    t.targetFreq = freqFromMidi (t.midi, refA4);
    t.cents      = centsBetween (freq, t.targetFreq);
    return t;
}

//==============================================================================
// Stabilises the raw per-frame estimates.
class Tracker
{
public:
    void reset() { out = pending = 0.0; pendingCount = misses = 0; }

    // raw = 0 when the frame has no pitch. Returns the frequency to show (0 = none).
    double push (double raw)
    {
        if (raw <= 0.0)
        {
            pendingCount = 0;
            if (++misses > kHoldFrames)
                out = 0.0;
            return out;
        }
        misses = 0;
        if (out <= 0.0 || std::abs (centsBetween (raw, out)) < 60.0)
        {
            out          = raw; // same note (or first note): follow it
            pendingCount = 0;
        }
        else if (pendingCount > 0 && std::abs (centsBetween (raw, pending)) < 60.0)
        {
            pending = raw;
            if (++pendingCount >= 2) // a new note needs two frames that agree
            {
                out          = raw;
                pendingCount = 0;
            }
        }
        else
        {
            pending      = raw;
            pendingCount = 1;
        }
        return out;
    }

private:
    double out = 0.0, pending = 0.0;
    int    pendingCount = 0, misses = 0;
};

//==============================================================================
// Pitch detector. feed() one sample at a time; a new estimate is ready every 20 ms.
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        fs     = sampleRate;
        factor = std::max (1, (int) std::lround (fs / kDecimatedRate));
        fsd    = fs / factor;
        maxLag = (int) std::ceil (fsd / kMinFreq) + 2;
        minLag = std::max (2, (int) std::floor (fsd / kMaxFreq));
        window = maxLag;
        frameLen = window + maxLag + 2;
        hop      = std::max (1, (int) std::lround (fsd / kAnalysisRate));

        const double lp = std::min (kLowpassHz, 0.4 * fsd);
        lp1.set (melon::Biquad::Type::lowpass, fs, lp, 0.5412);  // 4th-order Butterworth
        lp2.set (melon::Biquad::Type::lowpass, fs, lp, 1.3066);

        decim.assign ((size_t) frameLen, 0.0f);
        frame.assign ((size_t) frameLen, 0.0f);
        diff.assign ((size_t) maxLag + 2, 0.0);

        const int fullNeeded = (frameLen + 4) * factor;
        fullSize             = 1;
        while (fullSize < fullNeeded) fullSize <<= 1;
        full.assign ((size_t) fullSize, 0.0f);
        fullFrame.assign ((size_t) fullNeeded, 0.0f);
        reset();
    }

    void reset()
    {
        lp1.reset();
        lp2.reset();
        std::fill (decim.begin(), decim.end(), 0.0f);
        std::fill (full.begin(), full.end(), 0.0f);
        decimPos = fullPos = phase = hopCount = filled = 0;
        lastFreq = 0.0;
        lastLevelDb = -120.0;
        lastAperiodicity = 1.0;
    }

    void setThresholdDb (double db) { thresholdDb = db; }

    // Returns true when a new estimate is ready (see frequency()).
    bool feed (float x)
    {
        const float y = (float) lp2.process (lp1.process (x));
        full[(size_t) fullPos] = y;
        fullPos = (fullPos + 1) & (fullSize - 1);

        if (++phase < factor)
            return false;
        phase = 0;
        decim[(size_t) decimPos] = y;
        decimPos = (decimPos + 1) % frameLen;
        filled   = std::min (filled + 1, frameLen);

        if (++hopCount < hop || filled < frameLen)
            return false;
        hopCount = 0;
        analyse();
        return true;
    }

    double frequency() const { return lastFreq; }            // Hz, 0 = no pitch
    double levelDb() const { return lastLevelDb; }           // RMS of the analysis frame
    double aperiodicity() const { return lastAperiodicity; } // 0 = perfectly periodic
    double analysisRate() const { return fsd / hop; }

private:
    void analyse()
    {
        // newest frameLen decimated samples, oldest first
        for (int i = 0; i < frameLen; ++i)
            frame[(size_t) i] = decim[(size_t) ((decimPos + i) % frameLen)];

        double energy = 0.0;
        for (int i = frameLen - window; i < frameLen; ++i)
            energy += (double) frame[(size_t) i] * frame[(size_t) i];
        lastLevelDb = melon::gainToDb (std::sqrt (energy / window));
        lastFreq    = 0.0;
        lastAperiodicity = 1.0;
        if (lastLevelDb < thresholdDb)
            return;

        // YIN: difference function and cumulative mean normalised difference
        const int start = frameLen - window - maxLag; // window = the newest samples, compared with older ones
        diff[0] = 1.0;
        double running = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double d = 0.0;
            const float* a = frame.data() + start + maxLag;
            const float* b = a - tau;
            for (int j = 0; j < window; ++j)
            {
                const double e = (double) a[j] - (double) b[j];
                d += e * e;
            }
            running += d;
            diff[(size_t) tau] = running > 0.0 ? d * tau / running : 1.0;
        }

        int best = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
            if (diff[(size_t) tau] < kYinThreshold)
            {
                while (tau + 1 < maxLag && diff[(size_t) tau + 1] < diff[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        if (best < 0)
        {
            best = minLag;
            for (int tau = minLag; tau < maxLag; ++tau)
                if (diff[(size_t) tau] < diff[(size_t) best])
                    best = tau;
        }
        lastAperiodicity = diff[(size_t) best];
        if (lastAperiodicity > kMaxAperiodic || best <= minLag || best >= maxLag - 1)
            return;

        // Refine on the full-rate signal around best * factor
        const double coarse = (best + parabola (diff[(size_t) best - 1], diff[(size_t) best], diff[(size_t) best + 1])) * factor;
        lastFreq = refine (coarse);
    }

    double refine (double coarseLag)
    {
        const int fullWindow = window * factor;
        const int lo = std::max (2, (int) std::floor (coarseLag) - factor - 1);
        const int hi = (int) std::ceil (coarseLag) + factor + 1;
        const int needed = fullWindow + hi + 1;
        if (needed > (int) fullFrame.size())
            return fs / coarseLag;

        // newest `needed` full-rate samples, oldest first
        const int first = (fullPos - needed + fullSize) & (fullSize - 1);
        for (int i = 0; i < needed; ++i)
            fullFrame[(size_t) i] = full[(size_t) ((first + i) & (fullSize - 1))];

        auto d = [&] (int tau) {
            const float* a = fullFrame.data() + hi + 1;
            const float* b = a - tau;
            double s = 0.0;
            for (int j = 0; j < fullWindow; ++j)
            {
                const double e = (double) a[j] - (double) b[j];
                s += e * e;
            }
            return s;
        };

        int    bestTau = lo;
        double bestD = d (lo);
        std::array<double, 80> vals {};
        const int count = std::min (hi - lo + 1, (int) vals.size());
        vals[0] = bestD;
        for (int i = 1; i < count; ++i)
        {
            vals[(size_t) i] = d (lo + i);
            if (vals[(size_t) i] < bestD)
            {
                bestD   = vals[(size_t) i];
                bestTau = lo + i;
            }
        }
        const int bi = bestTau - lo;
        if (bi <= 0 || bi >= count - 1)
            return fs / coarseLag;
        return fs / (bestTau + parabola (vals[(size_t) bi - 1], bestD, vals[(size_t) bi + 1]));
    }

    // Offset of the minimum of the parabola through (-1, a), (0, b), (1, c)
    static double parabola (double a, double b, double c)
    {
        const double den = a - 2.0 * b + c;
        return den > 1e-20 ? std::clamp (0.5 * (a - c) / den, -0.5, 0.5) : 0.0;
    }

    double fs = 48000.0, fsd = 12000.0, thresholdDb = -60.0;
    int    factor = 4, maxLag = 0, minLag = 2, window = 0, frameLen = 0, hop = 240;
    int    decimPos = 0, fullPos = 0, fullSize = 1, phase = 0, hopCount = 0, filled = 0;
    melon::Biquad lp1, lp2;
    std::vector<float>  decim, frame, full, fullFrame;
    std::vector<double> diff;
    double lastFreq = 0.0, lastLevelDb = -120.0, lastAperiodicity = 1.0;
};

//==============================================================================
struct Params
{
    int    tuning      = 0;
    double reference   = 440.0;
    int    display     = 0;     // 0 needle, 1 strobe
    bool   mute        = false;
    double thresholdDb = -60.0;
    double smoothing   = 0.5;   // 0..1, used by the display
    double outputDb    = 0.0;
    bool   bypassed    = false;
};

// What the editor reads (written by the audio thread).
struct Reading
{
    std::atomic<float>    frequency { 0.0f }; // stabilised, 0 = no note
    std::atomic<float>    levelDb { -120.0f };
    std::atomic<uint32_t> frames { 0 };       // increments on every analysis
};

class Engine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs       = sampleRate;
        channels = std::clamp (numChannels, 1, kMaxChannels);
        detector.prepare (fs);
        tracker.reset();
        for (auto* s : { &outGain, &passGain })
            s->prepare (fs, kSmoothingMs);
        reading.frequency = 0.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        detector.setThresholdDb (p.thresholdDb);
        outGain.setTarget (p.bypassed ? 1.0 : melon::dbToGain (p.outputDb));
        passGain.setTarget (! p.bypassed && p.mute ? 0.0 : 1.0);
    }

    void snapToTargets()
    {
        outGain.snap();
        passGain.snap();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int nc = std::clamp (numChannels, 1, kMaxChannels);
        const float scale = 1.0f / (float) std::min (nc, channels);
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < std::min (nc, channels); ++c)
                mono += data[c][i];
            if (detector.feed (mono * scale))
                publish();

            const float g = (float) (outGain.next() * passGain.next());
            for (int c = 0; c < numChannels; ++c)
                data[c][i] *= g;
        }
    }

    const Reading& current() const { return reading; }
    double         analysisRate() const { return detector.analysisRate(); }

private:
    void publish()
    {
        reading.frequency = (float) tracker.push (detector.frequency());
        reading.levelDb   = (float) detector.levelDb();
        reading.frames.fetch_add (1, std::memory_order_release);
    }

    Params          params;
    double          fs = 48000.0;
    int             channels = 2;
    PitchDetector   detector;
    Tracker         tracker;
    melon::Smoother outGain, passGain;
    Reading         reading;
};

} // namespace tuner
