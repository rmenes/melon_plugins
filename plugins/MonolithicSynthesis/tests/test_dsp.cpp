// DSP core test (does not need JUCE):
//   g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
// Pass a folder to also render demo WAV files there:  ./test_dsp demo
//
// Melon Monolithic Synthesis - offline test harness
// Builds a synthetic plucked guitar (Karplus-Strong), runs it through the engine and checks:
//  1. tracking accuracy and note-on latency across the guitar range
//  2. numeric safety (no NaN/Inf, sane peaks) for all 72 sounds
//  3. CPU cost (real-time factor)
// and renders demo WAV files.
#include "MonolithicEngine.h"

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

using namespace melon::mono;

static constexpr double SR = 48000.0;

// ---------------------------------------------------------------- guitar model
struct Pluck { double start; float midi; float amp; double dur; float slideTo = -1; };

static std::vector<float> renderGuitar (const std::vector<Pluck>& notes, double totalSeconds, unsigned seed = 1)
{
    std::vector<float> out ((size_t) (totalSeconds * SR), 0.0f);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

    for (const auto& n : notes)
    {
        const double f0 = 440.0 * std::pow (2.0, (n.midi - 69.0) / 12.0);
        const int maxDelay = (int) (SR / 50.0) + 4;
        std::vector<float> line ((size_t) maxDelay, 0.0f);
        // excitation: low-passed noise burst
        const int initLen = (int) (SR / f0);
        float lp = 0.0f;
        for (int i = 0; i < initLen; ++i) { lp += 0.5f * (uni (rng) - lp); line[(size_t) i] = lp * n.amp * 2.0f; }

        int w = initLen % maxDelay;
        float prev = 0.0f;
        const size_t s0 = (size_t) (n.start * SR);
        const size_t len = (size_t) (n.dur * SR);
        for (size_t i = 0; i < len && s0 + i < out.size(); ++i)
        {
            double f = f0;
            if (n.slideTo > 0)
            {
                const double t = std::clamp ((double) i / (0.25 * SR) - 1.0, 0.0, 1.0);   // slide after 250 ms
                f = 440.0 * std::pow (2.0, ((n.midi + (n.slideTo - n.midi) * t) - 69.0) / 12.0);
            }
            const double delay = SR / f - 0.5;
            double rp = w - delay;
            while (rp < 0) rp += maxDelay;
            const int i0 = (int) rp, i1 = (i0 + 1) % maxDelay;
            const float fr = (float) (rp - i0);
            const float y = line[(size_t) i0] + (line[(size_t) i1] - line[(size_t) i0]) * fr;
            const float filtered = 0.996f * 0.5f * (y + prev);
            prev = y;
            line[(size_t) w] = filtered;
            w = (w + 1) % maxDelay;

            float fade = 1.0f;
            const size_t tail = (size_t) (0.01 * SR);
            if (i > len - tail) fade = (float) (len - i) / (float) tail;   // palm-mute style stop
            out[s0 + i] += y * fade;
        }
    }
    // small noise floor like a real DI
    for (auto& v : out) v += 0.0003f * uni (rng);
    return out;
}

// ---------------------------------------------------------------- wav writer
static void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) l.size(), dataBytes = n * 2 * 2, sr = (uint32_t) SR;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 (sr); w32 (sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround (std::clamp (l[i], -1.0f, 1.0f) * 32767.0f);
        int16_t b = (int16_t) std::lround (std::clamp (r[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

// ---------------------------------------------------------------- tests
static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static void testTracking()
{
    std::printf ("== Tracking accuracy / latency (Lead, 48 kHz, 64-sample blocks)\n");
    std::printf ("  note  freq(Hz)  latency(ms)  error(cents)\n");
    double worstLatency = 0, sumLatency = 0; int count = 0;
    double worstErr = 0;

    for (int note = 40; note <= 88; note += 3)   // E2 .. E6
    {
        auto guitar = renderGuitar ({ { 0.2, (float) note, 0.3f, 1.0 } }, 1.4, (unsigned) note);
        MonolithicEngine eng;
        eng.prepare (SR);
        EngineParams p; p.mode = Lead; eng.setParams (p);
        eng.setTransport (120, 0, false);

        std::vector<float> l (guitar.size()), r (guitar.size());
        double onTime = -1; float pitchAt = 0;
        const int block = 64;
        for (size_t i = 0; i < guitar.size(); i += block)
        {
            const int n = (int) std::min<size_t> (block, guitar.size() - i);
            eng.process (guitar.data() + i, l.data() + i, r.data() + i, n);
            const double t = (double) (i + n) / SR;
            if (onTime < 0 && eng.isNoteActive()) onTime = t;
            if (t >= 0.7 && pitchAt == 0) pitchAt = eng.getTrackedPitch();
        }
        const double latency = (onTime - 0.2) * 1000.0;
        const double err = (pitchAt - note) * 100.0;
        std::printf ("  %3d  %8.1f  %10.1f  %+11.1f\n", note, 440.0 * std::pow (2.0, (note - 69) / 12.0), latency, err);
        CHECK (onTime > 0, "note %d never detected", note);
        CHECK (std::fabs (err) < 15.0, "note %d pitch error %.1f cents", note, err);
        CHECK (latency < 35.0, "note %d latency %.1f ms", note, latency);
        worstLatency = std::max (worstLatency, latency); sumLatency += latency; ++count;
        worstErr = std::max (worstErr, std::fabs (err));
    }
    std::printf ("  average latency %.1f ms, worst %.1f ms, worst error %.1f cents\n\n", sumLatency / count, worstLatency, worstErr);
}

static void testTrackingStress()
{
    std::printf ("== Tracking stress: every semitone E2-E6, 8 different plucks each\n");

  int total=0, wrong=0, missed=0; double lat=0, worst=0;
  for(int note=40; note<=88; ++note) for(unsigned seed=100; seed<108; ++seed){
    auto g = renderGuitar({{0.2,(float)note,0.15f+0.05f*(seed%4),0.8}},1.1,seed*31+note);
    MonolithicEngine eng; eng.prepare(SR); EngineParams p; eng.setParams(p);
    std::vector<float> l(g.size()), r(g.size()); double on=-1; float pitch=0; bool glitch=false;
    for(size_t i=0;i<g.size();i+=64){ int n=(int)std::min<size_t>(64,g.size()-i); eng.process(g.data()+i,l.data()+i,r.data()+i,n);
      double t=(i+n)/SR; if(on<0&&eng.isNoteActive()) on=t;
      if(eng.isNoteActive() && t>on+0.05 && t<0.9 && std::fabs(eng.getTrackedPitch()-note)>0.5 && !glitch){ glitch=true; std::printf("    glitch note %d seed %u at %.0f ms (+%.0f after on) -> %.2f\n",note,seed,t*1000,(t-on)*1000,eng.getTrackedPitch()); }
      if(t>=0.6&&pitch==0) pitch=eng.getTrackedPitch(); }
    ++total; if(on<0){++missed;continue;}
    if(std::fabs(pitch-note)>0.5 || glitch) {++wrong; std::printf("  wrong: note %d seed %u -> %.2f%s\n",note,seed,pitch, glitch?" (glitch)":"");}
    double L=(on-0.2)*1000; lat+=L; worst=std::max(worst,L);
  }
  std::printf("  notes %d, missed %d, wrong/glitched %d, avg latency %.1f ms, worst %.1f ms\n", total, missed, wrong, lat/(total-missed), worst);
    CHECK (missed == 0 && wrong == 0, "%d missed, %d wrong or glitched notes", missed, wrong);
    std::printf ("\n");
}


// ---------------------------------------------------------------- real-string model
// Additive string: random pick and pickup positions, per-harmonic random decay (the fundamental
// can be weak and can fade before the 2nd harmonic), inharmonicity and a slightly sharp attack.
static std::vector<float> stringNote (int midi, unsigned seed, double seconds, double amp)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<double> U (0.0, 1.0);
    const double f0 = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
    const double pluck = 0.08 + 0.35 * U (rng);      // pick position (fraction of string)
    const double pickup = 0.05 + 0.25 * U (rng);     // pickup position
    const double B = 0.00005 + 0.0002 * U (rng);     // inharmonicity
    const double tau0 = 1.5 + 2.5 * U (rng) * (midi < 60 ? 1.5 : 0.8);
    const int H = 14;
    double a[H], tau[H], f[H], ph[H];
    double norm = 0;
    for (int k = 1; k <= H; ++k)
    {
        f[k-1] = k * f0 * std::sqrt (1.0 + B * k * k);
        a[k-1] = std::fabs (std::sin (kPi * k * pluck)) / k * std::fabs (std::sin (kPi * k * pickup)) * (0.5 + U (rng));
        tau[k-1] = tau0 / (1.0 + 0.25 * (k - 1)) * (0.4 + 1.4 * U (rng));   // per-harmonic random decay
        ph[k-1] = 2 * kPi * U (rng);
        if (k == 1) { a[0] *= 0.05 + 0.95 * U (rng) * U (rng); tau[0] *= 0.15 + 0.85 * U (rng); }
        if (k == 2) a[1] *= 1.0 + 1.5 * U (rng);
        if (f[k-1] > SR * 0.45) a[k-1] = 0;
        norm += a[k-1];
    }
    std::vector<float> out ((size_t) ((seconds + 0.3) * SR), 0.0f);
    const size_t s0 = (size_t) (0.2 * SR);
    for (size_t i = 0; i + s0 < out.size(); ++i)
    {
        const double t = i / SR;
        double v = 0;
        for (int k = 0; k < H; ++k)
            if (a[k] > 0) v += a[k] * std::exp (-t / tau[k]) * std::sin (2 * kPi * f[k] * (t + 0.004 * 0.08 * (1.0 - std::exp (-t / 0.08))) + ph[k]);
        const double attack = std::min (1.0, t / 0.002);
        out[s0 + i] = (float) (amp * attack * v / norm);
    }
    std::normal_distribution<float> N (0.0f, 0.0003f);
    for (auto& v : out) v += N (rng);
    return out;
}


static void testOctaveStability()
{
    std::printf ("== Octave stability: 270 sustained notes (3 s) from the real-string model\n");
    int total = 0, flips = 0, stuck = 0, missed = 0;
    double wrongMs = 0;
    for (int midi = 40; midi <= 84; ++midi)
        for (unsigned s = 0; s < 6; ++s)
        {
            const unsigned seed = (unsigned) (midi * 101 + (int) s * 7);
            auto x = stringNote (midi, seed, 3.0, 0.1 + 0.05 * (s % 4));
            MonolithicEngine e; e.prepare (SR); EngineParams p; e.setParams (p);
            std::vector<float> l (x.size()), r (x.size());
            bool wasRight = false, flipped = false, isStuck = false, on = false;
            for (size_t i = 0; i < x.size(); i += 64)
            {
                const int n = (int) std::min<size_t> (64, x.size() - i);
                e.process (x.data() + i, l.data() + i, r.data() + i, n);
                const double t = (double) (i + (size_t) n) / SR;
                if (! e.isNoteActive()) continue;
                on = true;
                const bool right = std::fabs (e.getTrackedPitch() - (float) midi) < 0.6f;
                if (! right) wrongMs += n / SR * 1000.0;
                if (right && t > 0.25) wasRight = true;
                if (wasRight && ! right && ! flipped)
                {
                    flipped = true;
                    std::printf ("  flip: midi %d seed %u -> %.2f at %.0f ms\n", midi, seed, e.getTrackedPitch(), (t - 0.2) * 1000.0);
                }
                if (t > 0.5 && t < 0.502 && ! right) isStuck = true;
            }
            ++total; if (! on) ++missed; if (flipped) ++flips; if (isStuck) ++stuck;
        }
    std::printf ("  notes %d, missed %d, octave changes while ringing %d, wrong octave for the whole note %d, avg wrong-octave time %.1f ms\n\n",
                 total, missed, flips, stuck, wrongMs / total);
    CHECK (missed == 0, "%d notes missed", missed);
    CHECK (flips == 0, "%d notes changed octave while ringing", flips);
    CHECK (stuck <= 3, "%d notes in the wrong octave (only the 3 with a 2nd harmonic 20+ dB above the fundamental may be)", stuck);
}

// Simple string segment with a pitch glide (bends) for the legato tests
static void addSegment (std::vector<float>& out, double start, double dur, float midiFrom, float midiTo, double glideTime, float amp)
{
    const double partials[] { 1.0, 0.6, 0.35, 0.25, 0.12, 0.08 };
    double phase[6] {};
    const size_t s0 = (size_t) (start * SR);
    for (size_t i = 0; i < (size_t) (dur * SR) && s0 + i < out.size(); ++i)
    {
        const double t = (double) i / SR;
        const double m = midiFrom + (midiTo - midiFrom) * std::min (1.0, glideTime > 0 ? t / glideTime : 1.0);
        const double f = 440.0 * std::pow (2.0, (m - 69.0) / 12.0);
        double v = 0.0;
        for (int k = 0; k < 6; ++k)
        {
            phase[k] += 2.0 * kPi * f * (k + 1) / SR;
            v += partials[k] * std::exp (-t * (0.6 + 0.4 * k)) * std::sin (phase[k]);
        }
        const double env = std::min (1.0, t / 0.002) * std::min (1.0, (dur - t) / 0.003);
        out[s0 + i] += (float) (amp * env * v / 2.4);
    }
}

static void testLegato()
{
    std::printf ("== Legato: octave pull-off and bend\n");
    // pull-off: E4 rings, then the 12th-fret note is pulled off to the open E3 (a small level bump)
    {
        std::vector<float> x ((size_t) (1.4 * SR), 0.0f);
        addSegment (x, 0.2, 0.5, 64, 64, 0, 0.25f);
        addSegment (x, 0.7, 0.6, 52, 52, 0, 0.30f);
        MonolithicEngine e; e.prepare (SR); EngineParams p; e.setParams (p);
        std::vector<float> l (x.size()), r (x.size());
        float before = 0, after = 0;
        for (size_t i = 0; i < x.size(); i += 64)
        {
            const int n = (int) std::min<size_t> (64, x.size() - i);
            e.process (x.data() + i, l.data() + i, r.data() + i, n);
            const double t = (double) (i + (size_t) n) / SR;
            if (t > 0.6 && before == 0) before = e.getTrackedPitch();
            if (t > 0.8 && after == 0) after = e.getTrackedPitch();
        }
        std::printf ("  pull-off 64 -> 52: before %.2f, 100 ms after %.2f\n", before, after);
        CHECK (std::fabs (before - 64) < 0.3f && std::fabs (after - 52) < 0.3f, "octave pull-off not followed (%.2f -> %.2f)", before, after);
    }
    // bend: G4 bent up a whole tone over 250 ms
    {
        std::vector<float> x ((size_t) (1.2 * SR), 0.0f);
        addSegment (x, 0.2, 0.9, 67, 69, 0.45, 0.25f);
        MonolithicEngine e; e.prepare (SR); EngineParams p; e.setParams (p);
        std::vector<float> l (x.size()), r (x.size());
        float mid = 0, end = 0;
        for (size_t i = 0; i < x.size(); i += 64)
        {
            const int n = (int) std::min<size_t> (64, x.size() - i);
            e.process (x.data() + i, l.data() + i, r.data() + i, n);
            const double t = (double) (i + (size_t) n) / SR;
            if (t > 0.425 && mid == 0) mid = e.getTrackedPitch();
            if (t > 0.9 && end == 0) end = e.getTrackedPitch();
        }
        std::printf ("  bend 67 -> 69: halfway %.2f, end %.2f\n\n", mid, end);
        CHECK (std::fabs (mid - 68) < 0.35f && std::fabs (end - 69) < 0.15f, "bend not followed (%.2f, %.2f)", mid, end);
    }
}

static void testMix()
{
    // The synth plays an unrelated MIDI note, so the dry guitar in the output can be measured
    // by correlation: expect gains 1, 0.71 and 0 (equal-power blend).
    std::printf ("== Mix (dry guitar gain in the output)\n");
    auto guitar = renderGuitar ({ { 0.1, 57, 0.3f, 0.8 } }, 1.0, 5);
    for (float mix : { 0.0f, 0.5f, 1.0f })
    {
        MonolithicEngine e; e.prepare (SR); EngineParams p; p.mix = mix; p.midiInput = true; e.setParams (p);
        e.midiNoteOn (91, 0.8f);
        std::vector<float> l (guitar.size()), r (guitar.size());
        for (size_t i = 0; i < guitar.size(); i += 128)
            e.process (guitar.data() + i, l.data() + i, r.data() + i, (int) std::min<size_t> (128, guitar.size() - i));
        double corr = 0, dryE = 0;
        const size_t from = (size_t) (0.1 * SR);   // after the 20 ms level smoothing
        for (size_t i = from; i < guitar.size(); ++i) { corr += l[i] * guitar[i]; dryE += guitar[i] * guitar[i]; }
        const double dryGain = corr / dryE;
        const double expected = std::cos (0.5 * kPi * mix);
        std::printf ("  mix %3.0f %%: dry gain %.3f (expected %.3f)\n", mix * 100, dryGain, expected);
        CHECK (std::fabs (dryGain - expected) < 0.03, "mix %.0f %%: dry gain %.3f, expected %.3f", mix * 100, dryGain, expected);
    }
    std::printf ("\n");
}

static std::vector<Pluck> demoPhrase()
{
    // E minor-ish riff with a re-pick, a slide and rests
    return {
        { 0.10, 52, 0.30f, 0.45 }, { 0.60, 55, 0.25f, 0.45 }, { 1.10, 57, 0.30f, 0.45 },
        { 1.60, 59, 0.30f, 0.90, 62 }, { 2.70, 64, 0.35f, 0.45 }, { 3.20, 64, 0.25f, 0.45 },
        { 3.70, 62, 0.30f, 0.45 }, { 4.20, 59, 0.30f, 1.40 },
    };
}

static void testAllSoundsAndRender (const std::string& outDir, bool render)
{
    std::printf ("== All 72 sounds: numeric safety + note count on demo phrase\n");
    auto guitar = renderGuitar (demoPhrase(), 7.0, 7);
    const auto& infos = getModeInfos();
    const auto& bank = getPatchBank();

    std::vector<float> demoL, demoR;

    for (int m = 0; m < NumModes; ++m)
    {
        for (int v = 0; v < kVariationsPerMode; ++v)
        {
            for (float macro : { 0.0f, 0.5f, 1.0f })
            {
                MonolithicEngine eng;
                eng.prepare (SR);
                EngineParams p; p.mode = m; p.variation = v;
                p.param1 = p.param2 = p.param3 = macro;
                eng.setParams (p);
                eng.setTransport (110, 0, false);

                std::vector<float> l (guitar.size()), r (guitar.size());
                for (size_t i = 0; i < guitar.size(); i += 128)
                {
                    const int n = (int) std::min<size_t> (128, guitar.size() - i);
                    eng.process (guitar.data() + i, l.data() + i, r.data() + i, n);
                }

                bool finite = true; float peak = 0; double energy = 0;
                for (size_t i = 0; i < l.size(); ++i)
                {
                    if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) finite = false;
                    peak = std::max (peak, std::max (std::fabs (l[i]), std::fabs (r[i])));
                    energy += l[i] * l[i];
                }
                const double rms = std::sqrt (energy / l.size());

                CHECK (finite, "%s / %s macro %.1f produced NaN/Inf", infos[(size_t) m].name, bank[(size_t) m][(size_t) v].name, macro);
                CHECK (peak < 2.0f, "%s / %s macro %.1f peak %.2f", infos[(size_t) m].name, bank[(size_t) m][(size_t) v].name, macro, peak);
                CHECK (rms > 0.003, "%s / %s macro %.1f nearly silent (rms %.4f)", infos[(size_t) m].name, bank[(size_t) m][(size_t) v].name, macro, rms);

                if (macro == 0.5f)
                {
                    std::printf ("  %-9s %-16s peak %.2f  rms %.3f  notes %lld\n", infos[(size_t) m].name,
                                 bank[(size_t) m][(size_t) v].name, peak, rms, eng.getNoteOnCount());
                    if (v == 0 && render)
                    {
                        std::string name = std::string (infos[(size_t) m].name);
                        writeWav (outDir + "/" + std::to_string (m + 1) + "_" + name + ".wav", l, r);
                        // normalise-ish for demo reel: 2 s guitar-only intro handled separately
                        demoL.insert (demoL.end(), l.begin(), l.end());
                        demoR.insert (demoR.end(), r.begin(), r.end());
                    }
                }
            }
        }
    }

    if (render)
    {
    std::vector<float> gl (guitar), gr (guitar);
    writeWav (outDir + "/0_dry_guitar.wav", gl, gr);
    writeWav (outDir + "/all_modes_demo.wav", demoL, demoR);
    }
    std::printf ("\n");
}

static void testMidiInput()
{
    std::printf ("== MIDI input mode\n");
    MonolithicEngine eng; eng.prepare (SR);
    EngineParams p; p.midiInput = true; eng.setParams (p);
    std::vector<float> silence (4800, 0.0f), l (4800), r (4800);
    eng.midiNoteOn (60, 0.8f);
    eng.process (silence.data(), l.data(), r.data(), 4800);
    float peak = 0; for (float v : l) peak = std::max (peak, std::fabs (v));
    CHECK (eng.isNoteActive() && peak > 0.05f, "MIDI note did not sound");
    eng.midiNoteOff (60);
    std::printf ("  note-on peak %.2f, active after off: %s\n\n", peak, eng.isNoteActive() ? "yes" : "no");
    CHECK (! eng.isNoteActive(), "MIDI note-off ignored");
}

static void testCpu()
{
    std::printf ("== CPU\n");
    auto guitar = renderGuitar (demoPhrase(), 7.0, 3);
    MonolithicEngine eng; eng.prepare (SR);
    EngineParams p; p.mode = Pad; eng.setParams (p);
    std::vector<float> l (guitar.size()), r (guitar.size());
    const auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 5; ++rep)
        for (size_t i = 0; i < guitar.size(); i += 128)
        {
            const int n = (int) std::min<size_t> (128, guitar.size() - i);
            eng.process (guitar.data() + i, l.data() + i, r.data() + i, n);
        }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double audio = 5.0 * guitar.size() / SR;
    std::printf ("  %.1f s audio in %.3f s -> %.1f%% of one core\n\n", audio, secs, 100.0 * secs / audio);
}

int main (int argc, char** argv)
{
    const bool render = argc > 1;
    const std::string outDir = render ? argv[1] : ".";
    std::printf ("Melon Monolithic Synthesis DSP test\n\n");
    testTracking();
    testTrackingStress();
    testOctaveStability();
    testLegato();
    testMix();
    testMidiInput();
    testAllSoundsAndRender (outDir, render);
    testCpu();
    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
