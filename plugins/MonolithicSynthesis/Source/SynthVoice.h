// Melon Monolithic Synthesis - monophonic synth voice
#pragma once

#include "Filters.h"
#include "Patches.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace melon::mono
{
// ---------------------------------------------------------------------------
class Envelope
{
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double s) { sr = s; }

    void set (float a, float d, float s, float r) noexcept
    {
        attackInc = 1.0f / (float) std::max (1.0, a * sr);
        decayCoef = coefFor (d);
        sustain = std::clamp (s, 0.0f, 1.0f);
        releaseCoef = coefFor (r);
    }

    void trigger() noexcept { stage = Attack; }          // retrigger from current level (no click)
    void release() noexcept { if (stage != Idle) stage = Release; }
    void reset() noexcept { stage = Idle; value = 0.0f; }
    bool isActive() const noexcept { return stage != Idle; }
    float getValue() const noexcept { return value; }

    float next() noexcept
    {
        switch (stage)
        {
            case Attack:
                value += attackInc;
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay:
                value = sustain + (value - sustain) * decayCoef;
                if (value - sustain < 1.0e-4f) { value = sustain; stage = Sustain; }
                break;
            case Sustain:
                value = sustain;
                break;
            case Release:
                value *= releaseCoef;
                if (value < 1.0e-5f) { value = 0.0f; stage = Idle; }
                break;
            case Idle:
            default:
                break;
        }
        return value;
    }

private:
    float coefFor (float seconds) const noexcept
    {
        // time to fall to ~-60 dB
        return (float) std::exp (-6.9 / std::max (1.0, seconds * sr));
    }

    double sr = 44100.0;
    Stage stage = Idle;
    float value = 0.0f, attackInc = 0.01f, decayCoef = 0.99f, sustain = 1.0f, releaseCoef = 0.99f;
};

// ---------------------------------------------------------------------------
class Lfo
{
public:
    void setSampleRate (double s) { sr = s; }
    void reset() noexcept { phase = 0.0; held = 0.0f; delayCounter = 0.0f; }

    float next (int wave, float rate, float delaySeconds) noexcept
    {
        const double prev = phase;
        phase += rate / sr;
        if (phase >= 1.0) phase -= 1.0;

        float v = 0.0f;
        switch (wave)
        {
            case LfoTriangle:   v = (float) (1.0 - 4.0 * std::fabs (phase - 0.5)); break;
            case LfoSquare:     v = phase < 0.5 ? 1.0f : -1.0f; break;
            case LfoSampleHold:
                if (phase < prev) { rng = rng * 1664525u + 1013904223u; held = (float) ((rng >> 8) & 0xFFFF) / 32767.5f - 1.0f; }
                v = held; break;
            case LfoSine:
            default:            v = (float) std::sin (2.0 * kPi * phase); break;
        }

        // fade-in after note-on
        float fade = 1.0f;
        if (delaySeconds > 0.001f)
        {
            delayCounter += (float) (1.0 / sr);
            fade = std::clamp ((delayCounter - delaySeconds * 0.5f) / (delaySeconds * 0.5f), 0.0f, 1.0f);
        }
        return v * fade;
    }

    void noteOn() noexcept { delayCounter = 0.0f; }

private:
    double sr = 44100.0, phase = 0.0;
    float held = 0.0f, delayCounter = 0.0f;
    uint32_t rng = 12345u;
};

// ---------------------------------------------------------------------------
class StereoChorus
{
public:
    void prepare (double s)
    {
        sr = s;
        size = (int) (sr * 0.05) + 4;
        buffer.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); write = 0; phaseL = 0.0; phaseR = 0.37; }

    void process (float in, float mix, float& outL, float& outR) noexcept
    {
        buffer[(size_t) write] = in;
        phaseL += 0.43 / sr; if (phaseL >= 1.0) phaseL -= 1.0;
        phaseR += 0.51 / sr; if (phaseR >= 1.0) phaseR -= 1.0;

        const float wetL = readAt (0.012 + 0.0035 * std::sin (2.0 * kPi * phaseL));
        const float wetR = readAt (0.014 + 0.0035 * std::sin (2.0 * kPi * phaseR));
        write = (write + 1) % size;

        outL = in * (1.0f - 0.5f * mix) + wetL * mix * 0.8f;
        outR = in * (1.0f - 0.5f * mix) + wetR * mix * 0.8f;
    }

private:
    float readAt (double seconds) const noexcept
    {
        double pos = write - seconds * sr;
        while (pos < 0.0) pos += size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float frac = (float) (pos - i0);
        return buffer[(size_t) i0] + (buffer[(size_t) i1] - buffer[(size_t) i0]) * frac;
    }

    double sr = 44100.0, phaseL = 0.0, phaseR = 0.37;
    int size = 2048, write = 0;
    std::vector<float> buffer;
};

// ---------------------------------------------------------------------------
class SynthVoice
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ampEnv.setSampleRate (sr);
        cutoffCoef = (float) std::exp (-1.0 / (0.01 * sr));   // 10 ms smoothing for knob moves
        fltEnv.setSampleRate (sr);
        lfo.setSampleRate (sr);
        reset();
    }

    void reset()
    {
        ampEnv.reset(); fltEnv.reset(); lfo.reset();
        ladder.reset(); svf.reset();
        phase1 = phase2 = phaseSub = 0.0;
        pitchEnv = 0.0f;
        currentPitch = targetPitch = 60.0f;
        glideStarted = false;
    }

    void setPatch (const Patch& p) noexcept
    {
        patch = p;
        ampEnv.set (p.ampA, p.ampD, p.ampS, p.ampR);
        fltEnv.set (p.fltA, p.fltD, p.fltS, p.fltR);
        if (! cutoffInitialised) { cutoffSmoothed = p.cutoff; cutoffInitialised = true; }
        glideCoef = p.glide > 0.0005f ? (float) std::exp (-1.0 / (p.glide * 0.33 * sr)) : 0.0f;
        pitchEnvCoef = (float) std::exp (-1.0 / (std::max (0.005f, p.pitchEnvTime) * 0.33 * sr));
    }

    // Start (or retrigger) a note. jump = true sets the pitch immediately.
    void noteOn (float midiPitch, bool retrigger, bool jump) noexcept
    {
        targetPitch = midiPitch;
        if (jump || ! glideStarted) currentPitch = midiPitch;
        glideStarted = true;
        if (retrigger)
        {
            ampEnv.trigger();
            fltEnv.trigger();
            pitchEnv = 1.0f;
            lfo.noteOn();
        }
    }

    void setTargetPitch (float midiPitch) noexcept { targetPitch = midiPitch; }
    void setPitchSmoothing (float coef) noexcept { bendCoef = coef; }

    void retriggerEnvelopes() noexcept { ampEnv.trigger(); fltEnv.trigger(); }
    void releaseAmp() noexcept { ampEnv.release(); fltEnv.release(); }
    void noteOff() noexcept { ampEnv.release(); fltEnv.release(); }
    bool isActive() const noexcept { return ampEnv.isActive(); }
    float getCurrentPitch() const noexcept { return currentPitch; }

    // patternPitch: semitone offset from arp/seq, patternFilterOct: filter offset in octaves
    float render (float patternPitch, float patternFilterOct, float gain) noexcept
    {
        if (! ampEnv.isActive())
            return 0.0f;

        // pitch glide: patch glide when enabled, otherwise a tiny smoothing for bends
        const float coef = glideCoef > 0.0f ? glideCoef : bendCoef;
        currentPitch = targetPitch + (currentPitch - targetPitch) * coef;

        const float aEnv = ampEnv.next();
        const float fEnv = fltEnv.next();
        const float lfoValue = lfo.next (patch.lfoWave, patch.lfoRate, patch.lfoDelay);
        pitchEnv *= pitchEnvCoef;

        const float pitch = currentPitch + patch.octave * 12.0f + patternPitch
                          + patch.lfoPitch * lfoValue + patch.pitchEnvAmt * pitchEnv;
        const double f1 = 440.0 * std::pow (2.0, (pitch - 69.0) / 12.0);
        const double f2 = f1 * std::pow (2.0, (patch.osc2Semi + patch.osc2EnvSemi * fEnv + patch.osc2Cents * 0.01f) / 12.0);

        const double nyq = sr * 0.45;
        const double dt1 = std::min (f1, nyq) / sr;
        const double dt2 = std::min (f2, nyq) / sr;
        const double dtSub = dt1 * 0.5;

        const float pw = std::clamp (patch.pulseWidth + patch.lfoPWM * lfoValue * 0.45f, 0.05f, 0.95f);

        // oscillator 1 (master)
        bool wrapped = false;
        phase1 += dt1;
        if (phase1 >= 1.0) { phase1 -= 1.0; wrapped = true; }

        phase2 += dt2;
        if (phase2 >= 1.0) phase2 -= 1.0;
        if (patch.sync > 0.5f && wrapped)
            phase2 = phase1 * (dt2 / dt1);   // hard sync with fractional reset

        phaseSub += dtSub;
        if (phaseSub >= 1.0) phaseSub -= 1.0;

        float o2 = osc (patch.osc2Wave, phase2, dt2, pw);
        float o1;
        if (patch.fmAmount > 0.0f && patch.osc1Wave == Sine)
        {
            const float index = patch.fmAmount * ((1.0f - patch.fmEnv) + patch.fmEnv * fEnv);
            o1 = (float) std::sin (2.0 * kPi * phase1 + index * std::sin (2.0 * kPi * phase2));
            o2 = 0.0f;   // modulator is not heard directly
        }
        else
        {
            o1 = osc (patch.osc1Wave, phase1, dt1, pw);
        }

        float mix;
        if (patch.ringMod > 0.5f)
            mix = o1 * osc (Sine, phase2, dt2, 0.5f) * patch.osc1Level + o2 * patch.osc2Level * 0.3f;
        else
            mix = o1 * patch.osc1Level + o2 * patch.osc2Level;

        if (patch.subLevel > 0.0f)
            mix += patch.subLevel * osc (Square, phaseSub, dtSub, 0.5f) * 0.8f;

        if (patch.noiseLevel > 0.0f)
        {
            noiseState = noiseState * 1664525u + 1013904223u;
            mix += patch.noiseLevel * ((float) ((noiseState >> 9) & 0x7FFFFF) / 4194304.0f - 1.0f);
        }

        mix *= 0.5f;

        // filter
        const float keyOct = patch.keyTrack * (pitch - 60.0f) / 12.0f;
        const float octaves = keyOct + patch.filterEnvAmt * fEnv + patch.lfoFilter * lfoValue + patternFilterOct;
        cutoffSmoothed = patch.cutoff + (cutoffSmoothed - patch.cutoff) * cutoffCoef;
        float fc = cutoffSmoothed * std::pow (2.0f, octaves);
        fc = std::clamp (fc, 20.0f, (float) (sr * 0.45));
        const float g = (float) std::tan (kPi * fc / sr);

        float y;
        if (patch.filterType == LP24)
        {
            y = ladder.process (mix, g, patch.resonance * 3.9f, patch.drive);
        }
        else
        {
            const float k = 2.0f - 1.96f * std::clamp (patch.resonance, 0.0f, 1.0f);
            const float driven = patch.drive > 1.0f ? std::tanh (mix * patch.drive) / patch.drive : mix;
            const auto out = svf.process (driven, g, k);
            // band-pass normalised to unity peak gain; low/high-pass get partial resonance compensation
            const float comp = std::sqrt (k * 0.5f);
            y = patch.filterType == LP12 ? out.low * comp
              : patch.filterType == BP12 ? out.band * k * (1.0f + 2.0f * patch.resonance)
              : out.high * comp;
        }

        float amp = aEnv * (1.0f - patch.lfoAmp * 0.5f * (1.0f + lfoValue));
        return flushDenormal (softLimit (y * amp * gain * patch.level));
    }

private:
    // transparent below 0.7, smoothly saturates toward 1.0
    static float softLimit (float x) noexcept
    {
        const float a = std::fabs (x);
        if (a <= 0.7f) return x;
        const float s = 0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f);
        return x < 0.0f ? -s : s;
    }

    static float polyBlep (double t, double dt) noexcept
    {
        if (t < dt) { const double x = t / dt; return (float) (x + x - x * x - 1.0); }
        if (t > 1.0 - dt) { const double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
        return 0.0f;
    }

    static float osc (int wave, double ph, double dt, float pw) noexcept
    {
        switch (wave)
        {
            case Saw:
                return (float) (2.0 * ph - 1.0) - polyBlep (ph, dt);
            case Square:
            {
                float v = ph < pw ? 1.0f : -1.0f;
                v += polyBlep (ph, dt);
                double t2 = ph + 1.0 - pw; if (t2 >= 1.0) t2 -= 1.0;
                v -= polyBlep (t2, dt);
                return v;
            }
            case Triangle:
                return (float) (1.0 - 4.0 * std::fabs (ph - 0.5));
            case Sine:
            default:
                return (float) std::sin (2.0 * kPi * ph);
        }
    }

    double sr = 44100.0;
    Patch patch;
    Envelope ampEnv, fltEnv;
    Lfo lfo;
    Ladder ladder;
    SVF svf;

    double phase1 = 0.0, phase2 = 0.0, phaseSub = 0.0;
    float currentPitch = 60.0f, targetPitch = 60.0f;
    float glideCoef = 0.0f, bendCoef = 0.0f, pitchEnv = 0.0f, pitchEnvCoef = 0.99f;
    bool glideStarted = false;
    float cutoffSmoothed = 2500.0f, cutoffCoef = 0.999f;
    bool cutoffInitialised = false;
    uint32_t noiseState = 22222u;
};

} // namespace melon::mono
