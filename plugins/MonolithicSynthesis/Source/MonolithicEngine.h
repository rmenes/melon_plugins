// Melon Monolithic Synthesis - main engine
//
// Guitar in -> envelope/onset detection + pitch tracker -> note logic ->
// mono synth voice (+ SEQ/ARPEGGIO clock) -> stereo chorus -> dry/synth mix.
// Optional MIDI input bypasses the tracker (last-note priority).
#pragma once

#include "PitchTracker.h"
#include "SynthVoice.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace melon::mono
{
struct EngineParams
{
    int   mode = Lead;
    int   variation = 0;
    float param1 = 0.5f, param2 = 0.5f, param3 = 0.5f;
    float effectLevelDb = 0.0f;     // synth level
    float mix = 1.0f;               // 0 = dry guitar only, 1 = synth only (equal-power blend)
    float sensitivity = 0.6f;       // 0..1 -> trigger threshold -66..-30 dBFS
    int   octaveShift = 0;          // -2..+2
    bool  pitchSnap = false;        // quantise tracked pitch to semitones
    bool  midiInput = false;        // play from MIDI notes instead of the guitar
};

class MonolithicEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        tracker.prepare (sr);
        voice.prepare (sr);
        chorus.prepare (sr);

        envAttack  = coef (0.0005);
        envRelease = coef (0.030);
        dynRelease = coef (0.120);
        slowCoef   = coef (0.025);
        envHistoryStep = std::max (1, (int) std::lround (sr * 0.001));
        levelCoef  = coef (0.020);
        voice.setPitchSmoothing ((float) coef (0.004));

        rebuildPatch();
        reset();
    }

    void reset()
    {
        tracker.reset();
        voice.reset();
        chorus.reset();
        fastEnv = slowEnv = dynEnv = 0.0f;
        gateOpen = noteActive = false;
        waitingForPitch = false;
        closeCounter = 0;
        samplesSinceOnset = 1 << 30;
        candidateCount = 0;
        midiNoteCount = 0;
    }

    void setParams (const EngineParams& p)
    {
        const bool soundChanged = p.mode != params.mode || p.variation != params.variation
                               || p.param1 != params.param1 || p.param2 != params.param2
                               || p.param3 != params.param3 || p.octaveShift != params.octaveShift;
        if (p.midiInput != params.midiInput)
        {
            midiNoteCount = 0;
            releaseNote();
        }
        params = p;
        if (soundChanged)
            rebuildPatch();
    }

    const EngineParams& getParams() const noexcept { return params; }
    const Patch& getCurrentPatch() const noexcept { return patch; }

    // Host transport, call once per block
    void setTransport (double bpmIn, double ppqPosition, bool isPlaying)
    {
        bpm = bpmIn > 20.0 ? bpmIn : 120.0;
        hostPlaying = isPlaying;
        if (isPlaying) hostPpq = ppqPosition;
    }

    // MIDI (only used when params.midiInput is on)
    void midiNoteOn (int note, float velocity)
    {
        if (! params.midiInput) return;
        if (midiNoteCount < 16) midiNotes[midiNoteCount++] = note;
        const bool wasActive = noteActive;
        noteVelocity = std::clamp (velocity, 0.05f, 1.0f);
        startNote ((float) note, ! (wasActive && patch.legato > 0.5f), ! wasActive);
    }

    void midiNoteOff (int note)
    {
        if (! params.midiInput) return;
        int w = 0;
        for (int i = 0; i < midiNoteCount; ++i)
            if (midiNotes[i] != note) midiNotes[w++] = midiNotes[i];
        midiNoteCount = w;
        if (midiNoteCount > 0) changePitch ((float) midiNotes[midiNoteCount - 1], false);
        else releaseNote();
    }

    // Mono in, stereo out. inputs and outputs may alias.
    void process (const float* in, float* outL, float* outR, int numSamples)
    {
        const float effectTarget = dbToGain (params.effectLevelDb);
        const float mixValue = std::clamp (params.mix, 0.0f, 1.0f);
        const float dryTarget = (float) std::cos (0.5 * kPi * mixValue);
        const float wetTarget = effectTarget * (float) std::sin (0.5 * kPi * mixValue);
        const float threshold = dbToGain (-66.0f + 36.0f * (1.0f - std::clamp (params.sensitivity, 0.0f, 1.0f)));

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = in[i];

            if (! params.midiInput)
                trackSample (x, threshold);

            // pattern clock
            float patternPitch = 0.0f, patternFilter = 0.0f;
            if (patch.pattern != PatternNone && noteActive)
                advancePattern (patternPitch, patternFilter);

            // dynamics: follow the guitar's own envelope
            float gain = 1.0f;
            if (! params.midiInput)
            {
                const float follow = notePeak > 1.0e-6f ? std::clamp (dynEnv / notePeak, 0.0f, 1.0f) : 1.0f;
                gain = (1.0f - patch.dynamics) + patch.dynamics * std::sqrt (follow);
                gain *= 1.0f - 0.4f * patch.dynamics * (1.0f - noteVelocity);
            }
            else
            {
                gain = (1.0f - 0.6f * patch.dynamics) + 0.6f * patch.dynamics * noteVelocity;
            }

            const float synth = voice.render (patternPitch, patternFilter, gain);
            float l, r;
            chorus.process (synth, patch.chorusMix, l, r);

            effectGain += (wetTarget - effectGain) * (1.0f - (float) levelCoef);
            directGain += (dryTarget - directGain) * (1.0f - (float) levelCoef);

            outL[i] = l * effectGain + x * directGain;
            outR[i] = r * effectGain + x * directGain;
        }
    }

    // Diagnostics (tests / UI tuner readout)
    bool isNoteActive() const noexcept { return noteActive; }
    float getTrackedPitch() const noexcept { return currentMidi; }
    float getInputLevel() const noexcept { return fastEnv; }
    long long getNoteOnCount() const noexcept { return noteOnCount; }
    // Current SEQ/ARP step (0-based) while a pattern sound plays, otherwise -1
    int getPatternStep() const noexcept
    {
        if (patch.pattern == PatternNone || ! noteActive || lastStep < 0) return -1;
        const int length = patch.pattern == PatternArp
            ? kArpPatterns[(size_t) std::clamp (patch.patternIndex, 0, (int) kArpPatterns.size() - 1)].length : 16;
        return (int) (lastStep % length);
    }

private:
    static float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
    double coef (double seconds) const { return std::exp (-1.0 / (seconds * sr)); }

    void rebuildPatch()
    {
        patch = buildPatch (params.mode, params.variation, params.param1, params.param2, params.param3);
        patch.octave += (float) params.octaveShift;
        voice.setPatch (patch);
    }

    // ---- guitar tracking --------------------------------------------------
    void trackSample (float x, float threshold)
    {
        const float a = std::fabs (x);
        fastEnv = a > fastEnv ? a + (fastEnv - a) * (float) envAttack : fastEnv * (float) envRelease;
        dynEnv  = a > dynEnv  ? a + (dynEnv - a) * (float) envAttack  : dynEnv * (float) dynRelease;
        slowEnv += (fastEnv - slowEnv) * (1.0f - (float) slowCoef);

        ++samplesSinceOnset;
        ++samplesSinceNoteStart;

        // envelope history, one value per millisecond
        if (++envHistoryCounter >= envHistoryStep)
        {
            envHistoryCounter = 0;
            envHistory[(size_t) envHistoryPos] = fastEnv;
            envHistoryPos = (envHistoryPos + 1) % kEnvHistory;
        }
        ++samplesSinceSoftOnset;

        // gate with hysteresis
        if (! gateOpen)
        {
            if (fastEnv > threshold)
            {
                gateOpen = true;
                waitingForPitch = true;
                closeCounter = 0;
                markOnset();
            }
        }
        else
        {
            // new pick attack: fast envelope jumps well above the slow one
            if (fastEnv > 1.8f * slowEnv && fastEnv > 2.0f * threshold && samplesSinceOnset > (int) (0.06 * sr))
                markOnset();
            // softer bump: pull-off / hammer-on (allows a legato octave change). The level must
            // rise above the lowest point of the last ~20 ms; a decaying note never does.
            if (fastEnv > 1.3f * recentMinEnv() && fastEnv > 2.0f * threshold
                && samplesSinceSoftOnset > (int) (0.04 * sr) && samplesSinceNoteStart > (int) (0.03 * sr))
                samplesSinceSoftOnset = 0;

            if (fastEnv < threshold * 0.5f)
            {
                if (++closeCounter > (int) (0.03 * sr))
                {
                    gateOpen = false;
                    waitingForPitch = false;
                    releaseNote();
                }
            }
            else
            {
                closeCounter = 0;
            }
        }

        if (noteActive || waitingForPitch)
            notePeak = std::max (notePeak, dynEnv);

        if (tracker.push (x))
            handleEstimate (tracker.getFrequency(), tracker.getClarity());
    }

    float recentMinEnv() const noexcept
    {
        float m = envHistory[0];
        for (float v : envHistory) m = std::min (m, v);
        return m;
    }

    void markOnset()
    {
        samplesSinceOnset = 0;
        onsetPending = true;
        onsetPeakReset = true;
    }

    void handleEstimate (float freq, float clarity)
    {
        if (! gateOpen || freq < 60.0f || freq > 1500.0f || clarity < 0.80f)
            return;

        float midi = 69.0f + 12.0f * std::log2 (freq / 440.0f);
        if (params.pitchSnap)
            midi = std::round (midi);

        const bool recentOnset = samplesSinceOnset < (int) (0.08 * sr);

        if (! noteActive)
        {
            // First note after silence: two estimates in a row must agree. The first few
            // milliseconds of a pick (one or two cycles heard) are where octave slips happen.
            if (clarity >= 0.85f && candidateCount > 0 && std::fabs (midi - candidateMidi) < 0.5f)
            {
                beginPickedNote (midi);
            }
            else
            {
                candidateMidi = midi;
                candidateCount = 1;
            }
            return;
        }

        // Octave-related readings (the tracker locking onto a harmonic, or a fading fundamental)
        // are folded back onto the current note, so bends and vibrato keep working and the
        // synth never jumps octave while a note rings.
        // Settled = past the attack (the first ~45 ms are where real octave corrections happen).
        // A legato note (pull-off / hammer-on) makes a small level bump, a fading fundamental doesn't.
        const bool settled = samplesSinceNoteStart > (int) (0.045 * sr);
        const bool legatoEvent = samplesSinceSoftOnset < (int) (0.06 * sr);
        {
            const float d = midi - currentMidi;
            for (const float interval : { 12.0f, -12.0f, 24.0f, -24.0f, 19.02f, -19.02f, 7.02f, -7.02f })
            {
                if (std::fabs (d - interval) < 0.8f)
                {
                    if (settled && ! legatoEvent && ! (onsetPending && recentOnset))
                    {
                        changePitch (currentMidi + (d - interval), true);
                        candidateCount = 0;
                        return;
                    }
                    break;   // still settling or a new pick: handled below with confirmation
                }
            }
        }

        const float diff = midi - currentMidi;

        if (std::fabs (diff) < 0.8f)
        {
            // bends, vibrato, or the same note picked again
            candidateCount = 0;
            if (onsetPending && recentOnset)
            {
                onsetPending = false;
                notePeak = dynEnv;
                noteVelocity = velocityFromPeak (dynEnv);
                voice.retriggerEnvelopes();
                if (patch.pattern != PatternNone) restartPattern();
            }
            changePitch (midi, true);
            return;
        }

        // a jump in the quiet tail of a note (> 22 dB below its peak) without a new pick is noise
        if (! recentOnset && dynEnv < 0.08f * notePeak)
            return;

        // a jump: confirm it so octave errors and pick noise don't glitch the synth
        if (candidateCount > 0 && std::fabs (midi - candidateMidi) < 0.5f)
            ++candidateCount;
        else
        {
            candidateMidi = midi;
            candidateCount = 1;
        }

        const float absDiff = std::fabs (diff);
        const bool octaveLike = std::fabs (absDiff - 12.0f) < 0.6f || std::fabs (absDiff - 19.0f) < 0.6f
                             || std::fabs (absDiff - 24.0f) < 0.6f;
        const int required = (recentOnset && onsetPending) ? (octaveLike ? 2 : 1) : (octaveLike ? 4 : 2);

        if (candidateCount >= required)
        {
            candidateCount = 0;
            if (onsetPending && recentOnset)
            {
                onsetPending = false;
                beginPickedNote (midi);
            }
            else
            {
                changePitch (midi, false);   // hammer-on / pull-off / slide: legato
            }
        }
    }

    float velocityFromPeak (float peak) const noexcept
    {
        // typical DI guitar peaks around 0.1..0.5
        return std::clamp (std::sqrt (peak / 0.4f), 0.2f, 1.0f);
    }

    void beginPickedNote (float midi)
    {
        waitingForPitch = false;
        onsetPending = false;
        candidateCount = 0;
        notePeak = std::max (dynEnv, 1.0e-4f);
        noteVelocity = velocityFromPeak (notePeak);
        const bool wasActive = noteActive;
        startNote (midi, ! (wasActive && patch.legato > 0.5f), ! wasActive);
    }

    // ---- note handling -----------------------------------------------------
    void startNote (float midi, bool retrigger, bool jump)
    {
        currentMidi = midi;
        noteActive = true;
        samplesSinceNoteStart = 0;
        ++noteOnCount;
        voice.noteOn (midi, retrigger, jump);
        if (retrigger && patch.pattern != PatternNone) restartPattern();
    }

    void changePitch (float midi, bool smallMove)
    {
        (void) smallMove;
        currentMidi = midi;
        voice.setTargetPitch (midi);
    }

    void releaseNote()
    {
        if (noteActive) voice.noteOff();
        noteActive = false;
        candidateCount = 0;
    }

    // ---- SEQ / ARPEGGIO ------------------------------------------------------
    void restartPattern()
    {
        freePhase = 0.0;
        lastStep = -1;
        noteStartPpq = hostPpq;
    }

    void advancePattern (float& pitchOut, float& filterOut)
    {
        const int divIdx = std::clamp ((int) std::lround (patch.rateIdx), 0, (int) kDivisions.size() - 1);
        const double beatsPerStep = kDivisions[(size_t) divIdx];
        const double stepsPerSample = (bpm / 60.0) / beatsPerStep / sr;

        double stepPos;
        if (hostPlaying)
        {
            hostPpq += (bpm / 60.0) / sr;
            stepPos = hostPpq / beatsPerStep;
        }
        else
        {
            freePhase += stepsPerSample;
            stepPos = freePhase;
        }

        const long long stepIndex = (long long) std::floor (stepPos);
        const double stepFrac = stepPos - (double) stepIndex;

        int length;
        int pitch = 0;
        float filterLevel = 1.0f;
        bool on = true;

        if (patch.pattern == PatternArp)
        {
            const auto& ap = kArpPatterns[(size_t) std::clamp (patch.patternIndex, 0, (int) kArpPatterns.size() - 1)];
            length = ap.length;
            pitch = ap.steps[(size_t) (stepIndex % length)];
        }
        else
        {
            const auto& sp = kSeqPatterns[(size_t) std::clamp (patch.patternIndex, 0, (int) kSeqPatterns.size() - 1)];
            length = 16;
            const auto& st = sp.steps[(size_t) (stepIndex % length)];
            pitch = st.pitch;
            filterLevel = st.filter;
            on = st.on;
        }

        if (stepIndex != lastStep)
        {
            lastStep = stepIndex;
            if (on) voice.retriggerEnvelopes();
            stepGateOpen = on;
        }

        if (stepGateOpen && stepFrac >= patch.gateLen)
        {
            voice.releaseAmp();
            stepGateOpen = false;
        }

        pitchOut = (float) pitch;
        filterOut = patch.pattern == PatternSeq ? patch.seqDepth * 3.0f * (filterLevel - 1.0f) : 0.0f;
    }

    // ---- state --------------------------------------------------------------
    double sr = 44100.0;
    EngineParams params;
    Patch patch;

    PitchTracker tracker;
    SynthVoice voice;
    StereoChorus chorus;

    double envAttack = 0, envRelease = 0, dynRelease = 0, slowCoef = 0, levelCoef = 0;
    float fastEnv = 0, slowEnv = 0, dynEnv = 0, notePeak = 1.0e-4f, noteVelocity = 1.0f;
    bool gateOpen = false, noteActive = false, waitingForPitch = false;
    bool onsetPending = false, onsetPeakReset = false;
    int closeCounter = 0;
    int samplesSinceOnset = 1 << 30;
    int samplesSinceNoteStart = 0;
    int samplesSinceSoftOnset = 1 << 30;
    static constexpr int kEnvHistory = 20;   // ms
    std::array<float, kEnvHistory> envHistory {};
    int envHistoryPos = 0, envHistoryCounter = 0, envHistoryStep = 48;

    float currentMidi = 60.0f, candidateMidi = 0.0f;
    int candidateCount = 0;
    long long noteOnCount = 0;

    int midiNotes[16] {};
    int midiNoteCount = 0;

    double bpm = 120.0, hostPpq = 0.0, noteStartPpq = 0.0, freePhase = 0.0;
    bool hostPlaying = false, stepGateOpen = false;
    long long lastStep = -1;

    float effectGain = 1.0f, directGain = 0.0f;
};

} // namespace melon::mono
