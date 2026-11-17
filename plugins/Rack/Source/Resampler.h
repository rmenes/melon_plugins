// Resampler.h - streaming sample-rate conversion for Melon Rack (plain C++17, no JUCE).
//
// Neural models are trained at a fixed sample rate (almost always 48 kHz). When the host
// runs at another rate, each model slot converts host -> model rate, runs the model, and
// converts back. NeuralRack does the same with zita-resampler; here a windowed-sinc
// interpolator is used so the plugin has no extra dependency.
//
//   Resampler          one direction, any ratio, constant group delay
//   ResampledProcessor up -> process at the model rate -> down, with a small FIFO so the
//                      host always gets exactly the number of samples it gave.
//                      Reports its latency in host samples (constant once prepared).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace rack
{
//==============================================================================
class Resampler
{
public:
    // maxInput: the largest number of input samples passed to one process() call
    void prepare (double inRate, double outRate, int maxInput)
    {
        step = inRate / outRate;
        // Low-pass at 92 % of the lower Nyquist: anti-imaging when going up, anti-aliasing when going down
        cutoff = 0.92 * std::min (1.0, outRate / inRate);
        half   = (int) std::ceil (kTaps / cutoff);

        table.resize ((size_t) (half * kTableRes + 2));
        for (size_t i = 0; i < table.size(); ++i)
        {
            const double x = (double) i / kTableRes;          // distance from the centre, in input samples
            table[i]       = (float) (x < (double) half ? cutoff * sinc (cutoff * x) * blackman (x / half) : 0.0);
        }

        hist.assign ((size_t) (maxInput + 2 * half + 8), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (hist.begin(), hist.end(), 0.0f);
        len = half;          // `half` zeros of history before the first input sample
        pos = (double) half; // the first output lines up with input sample 0
    }

    // Upper bound of the outputs one call with `numIn` inputs can produce
    int maxOutputFor (int numIn) const { return (int) std::ceil ((double) (numIn + 2 * half + 2) / step) + 2; }

    // Group delay of the filter in OUTPUT samples (outputs are held back until enough future input arrives)
    double delayInOutputSamples() const { return (double) half / step; }

    // Consumes all `numIn` inputs, writes the produced outputs to `out` and returns how many.
    int process (const float* in, int numIn, float* out)
    {
        std::memcpy (hist.data() + len, in, sizeof (float) * (size_t) numIn);
        len += numIn;

        int produced = 0;
        while ((int) pos + half < len)
        {
            out[produced++] = interpolate (pos);
            pos += step;
        }

        const int drop = std::max (0, (int) pos - half + 1);
        if (drop > 0)
        {
            std::memmove (hist.data(), hist.data() + drop, sizeof (float) * (size_t) (len - drop));
            len -= drop;
            pos -= drop;
        }
        return produced;
    }

private:
    static constexpr int    kTaps     = 16;  // half-length of the kernel at full bandwidth
    static constexpr int    kTableRes = 512; // kernel table points per input sample
    static constexpr double kPi       = 3.14159265358979323846;

    static double sinc (double x) { return std::abs (x) < 1e-9 ? 1.0 : std::sin (kPi * x) / (kPi * x); }
    static double blackman (double u) { return 0.42 + 0.5 * std::cos (kPi * u) + 0.08 * std::cos (2.0 * kPi * u); }

    float kernel (double x) const
    {
        x = std::abs (x) * kTableRes;
        const size_t i = (size_t) x;
        if (i + 1 >= table.size())
            return 0.0f;
        const float t = (float) (x - (double) i);
        return table[i] + t * (table[i + 1] - table[i]);
    }

    float interpolate (double p) const
    {
        const int    i0   = (int) p;
        const double frac = p - i0;
        float sum = 0.0f;
        for (int k = -half + 1; k <= half; ++k)
            sum += hist[(size_t) (i0 + k)] * kernel ((double) k - frac);
        return sum;
    }

    double step = 1.0, cutoff = 1.0, pos = 0.0;
    int    half = kTaps, len = 0;
    std::vector<float> table, hist;
};

//==============================================================================
// Runs a mono processor at `innerRate` inside a host running at `outerRate`.
// When the rates match there is no conversion and no latency.
class ResampledProcessor
{
public:
    // innerChunk: the largest block the inner processor accepts at once
    void prepare (double outerRate, double innerRate, int maxBlock, int innerChunkSize)
    {
        maxIn      = std::max (1, maxBlock);
        innerChunk = std::max (1, innerChunkSize);
        bypassRate = std::abs (outerRate - innerRate) < 0.5;

        if (bypassRate)
        {
            latency = 0;
            scratch.assign ((size_t) maxIn, 0.0f);
            return;
        }

        up.prepare (outerRate, innerRate, maxIn);
        const int maxInner = up.maxOutputFor (maxIn);
        down.prepare (innerRate, outerRate, maxInner);
        const int maxBack = down.maxOutputFor (maxInner);

        innerIn.assign ((size_t) maxInner, 0.0f);
        innerOut.assign ((size_t) maxInner, 0.0f);
        back.assign ((size_t) maxBack, 0.0f);

        // Held-back samples of both filters (in host samples) + a margin for the per-block jitter
        latency = (int) std::ceil ((double) up.delayInOutputSamples() * (outerRate / innerRate)
                                   + down.delayInOutputSamples()) + 4;

        fifo.assign ((size_t) (latency + maxBack + maxIn + 8), 0.0f);
        reset();
    }

    void reset()
    {
        if (bypassRate)
            return;
        up.reset();
        down.reset();
        std::fill (fifo.begin(), fifo.end(), 0.0f);
        fifoLen = latency; // primed with silence = the constant latency
    }

    int latencySamples() const { return latency; }
    bool isResampling() const { return ! bypassRate; }

    // fn (const float* in, float* out, int n) runs the inner processor (n <= innerChunk).
    // `io` is processed in place; numSamples must be <= maxBlock.
    template <class Fn>
    void process (float* io, int numSamples, Fn&& fn)
    {
        if (bypassRate)
        {
            for (int done = 0; done < numSamples;)
            {
                const int n = std::min (innerChunk, numSamples - done);
                std::memcpy (scratch.data(), io + done, sizeof (float) * (size_t) n);
                fn (scratch.data(), io + done, n);
                done += n;
            }
            return;
        }

        const int m = up.process (io, numSamples, innerIn.data());
        for (int done = 0; done < m;)
        {
            const int n = std::min (innerChunk, m - done);
            fn (innerIn.data() + done, innerOut.data() + done, n);
            done += n;
        }
        const int k = down.process (innerOut.data(), m, back.data());

        const int room = (int) fifo.size() - fifoLen;
        const int add  = std::min (k, room);
        std::memcpy (fifo.data() + fifoLen, back.data(), sizeof (float) * (size_t) add);
        fifoLen += add;

        const int take = std::min (numSamples, fifoLen);
        std::memcpy (io, fifo.data(), sizeof (float) * (size_t) take);
        for (int i = take; i < numSamples; ++i) // only if the FIFO ever ran dry (should not happen)
            io[i] = 0.0f;
        std::memmove (fifo.data(), fifo.data() + take, sizeof (float) * (size_t) (fifoLen - take));
        fifoLen -= take;
    }

private:
    Resampler up, down;
    std::vector<float> innerIn, innerOut, back, fifo, scratch;
    int  maxIn = 1, innerChunk = 128, latency = 0, fifoLen = 0;
    bool bypassRate = true;
};

} // namespace rack
