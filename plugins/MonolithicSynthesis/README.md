# Melon Monolithic Synthesis — VST3 / LV2 guitar synthesizer

A monophonic guitar synth in the vein of the **Boss SY-200**: play your guitar normally and the plugin tracks each note and plays it on an analog-style synth. It has **12 modes** (LEAD, PAD, STRING, BELL, ORGAN, BASS, DUAL, SWEEP, NOISE, SFX, SEQ, ARPEGGIO), **6 sounds per mode** (72 in total), three **Param** knobs whose job changes with the mode, and a guitar/synth **Mix**. On top of a pedal it adds host-synced SEQ and ARPEGGIO, **Pitch Snap** and an optional **MIDI In** mode. For a polyphonic, filter-based octave synth use [Melon Guitar Synth](../GuitarSynth/README.md).
Built with [JUCE](https://juce.com). The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux) and a **Standalone** app.

**Monophonic:** it follows one note at a time. Play single notes and mute the other strings; chords give unpredictable results.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Mode** | 12 modes | LEAD | The type of synth sound. Use the `<` `>` arrows to step through them. |
| **Sound** | 1 – 6 | 1 | The sounds of the current mode (the list shows their names). |
| **Param 1 / 2 / 3** | 0 – 100 % | 50 % | Mode-specific knobs; their names change with the mode (table below). At 50 % the sound is exactly as designed; turning left or right moves toward each end of the range. |
| **Effect** | −40 … +6 dB | 0 dB | Level of the synth (before the Mix). |
| **Mix** | 0 – 100 % | 100 % | Balance between your dry guitar and the synth: 0 % = guitar only, 100 % = synth only, 50 % = both at equal loudness (equal-power blend). |
| **Sensitivity** | 0 – 100 % | 60 % | How easily notes trigger (threshold −30 to −66 dBFS). Raise it if soft notes are missed; lower it if string noise or pick scrapes trigger notes. The orange mark on the Input meter shows the trigger level. |
| **Octave** | −2 … +2 | 0 | Shifts the synth by whole octaves. |
| **Pitch Snap** | Off / On | Off | Rounds the tracked pitch to the nearest semitone: bends and vibrato then step instead of glide. |
| **MIDI In** | Off / On | Off | Plays the synth from MIDI notes (a MIDI pickup, a keyboard, a MIDI clip) instead of the guitar. Last-note priority; velocity sets the level. |

The header has the series' standard **Bypass** and **Output** gain, and the preset row has 10 factory presets.

The display shows the **note** being played (with its tuning in cents) and the **input level** with the trigger threshold, then the **12 modes** with the current one lit, the sound name, the three Param values and, for SEQ and ARPEGGIO, the **pattern steps** with the current step lit.

## Modes

| Mode | Param 1 | Param 2 | Param 3 | Sounds |
|---|---|---|---|---|
| **LEAD** | Filter | Resonance | Glide | Saw Lead, Square Lead, Sync Lead, Fat Lead, Soft Lead, Resonant Lead |
| **PAD** | Attack | Release | Filter | Warm Pad, Glass Pad, Dark Pad, Swell Pad, Shimmer Pad, Choir Pad |
| **STRING** | Attack | Release | Brightness | Ensemble, Cello, Pizzicato, Slow Strings, Bright Strings, Tremolo Strings |
| **BELL** | Decay | Tone | Brightness | FM Bell, Tubular Bell, Ring Bell, Glockenspiel, Soft Mallet, Metal Bell |
| **ORGAN** | Tone | Vibrato | Sub | Drawbar Organ, Rock Organ, Church Organ, Combo Organ, Jazz Organ, Reed Organ |
| **BASS** | Filter | Resonance | Decay | Synth Bass, Acid Bass, Sub Bass, Pluck Bass, Fat Bass, Wobble Bass |
| **DUAL** | Detune | Filter | Balance | Octave Dual, Fifth Dual, Detuned Dual, Sub Dual, Fourth Dual, Wide Dual |
| **SWEEP** | Time | Depth | Resonance | Filter Sweep, Rising Sweep, Sync Sweep, PWM Sweep, Slow LFO Sweep, Band Sweep |
| **NOISE** | Color | Resonance | Decay | Wind, Breath Lead, Noise Burst, Tuned Noise, Ocean, Static |
| **SFX** | Rate | Depth | Filter | Laser, Siren, Ring Mod, Robot, Rise Up, Trill |
| **SEQ** | Rate | Depth | Resonance | Pulse 16, Gallop, Octave Bounce, Acid Line, Trance Gate, Stutter |
| **ARPEGGIO** | Rate | Gate | Filter | Octave Arp, Major Arp, Minor Arp, Fifths Arp, Up & Down, Sus Arp |

**SEQ** plays a 16-step rhythm of filter (and some pitch) changes on the note you hold; **ARPEGGIO** plays a pattern of intervals above it. Both follow the host tempo (120 BPM if the host reports none) and, while the host plays, lock to its beat. Their **Rate** knob picks the step length: 1/4, 1/8, 1/8 T, 1/16, 1/16 T, 1/32.

## How it works

- **Pitch tracking:** the McLeod Pitch Method on a filtered copy of the input at about 12 kHz, updated every 2.5 ms, with a check against octave errors. A new note starts when two readings in a row agree, and large pitch jumps must be confirmed by consecutive readings, so pick noise doesn't glitch the synth.
- **Octave lock:** once a note is past its attack (45 ms), readings an octave, two octaves, a fifth or an octave + fifth away from it are treated as the tracker locking onto a harmonic (this happens when a string's fundamental is weak or fades before its overtones) and are folded back onto the note. A ringing note therefore never changes octave, and bends and vibrato keep working. A new pick, or the small level bump of a pull-off or hammer-on, unlocks it, so a 12th-fret-to-open pull-off is still followed.
- **Playing feel:** bends, vibrato and slides are followed continuously; hammer-ons, pull-offs and slides change the pitch without re-triggering the sound; every new pick attack re-triggers it (pads and other "legato" sounds just glide). The synth follows your picking strength and the string's decay, by an amount set per sound (pads sustain at a steady level while the string rings).
- **Latency:** about 10 ms on high notes and up to about 30 ms on the low E string, the time needed to hear two cycles of the note. Average 17.6 ms across E2–E6 in the tests.
- **Synth voice:** two anti-aliased oscillators (saw, square with PWM, triangle, sine), hard sync, ring modulation, 2-operator FM, square sub-oscillator and noise; a 24 dB ladder filter with drive or a 12 dB low/band/high-pass; amp and filter envelopes; an LFO (sine, triangle, square, sample & hold) with delay; pitch envelope; glide; stereo chorus; soft output limiter.

No latency is added to the host; the tail is up to 3 s (long pad and bell releases).

## Tips

- Plug the guitar in directly (DI) or put the plugin first in the chain, before drive and time effects.
- The neck pickup with the tone rolled back a little gives the most stable tracking.
- Turn **Mix** down to blend the synth under your guitar ("Pad Under Guitar" preset).

## Factory presets

| Preset | Mode / Sound | Notes |
|---|---|---|
| Saw Lead | LEAD 1 | Brighter filter, a touch of glide |
| Sync Scream | LEAD 3 | Sync sweep lead |
| Pad Under Guitar | PAD 1 | Slower attack, guitar and pad blended (Mix 45 %) |
| Ensemble Strings | STRING 1 | |
| Crystal Bells | BELL 1 | Longer decay, some guitar mixed in (Mix 75 %) |
| Rock Organ | ORGAN 2 | More vibrato |
| Acid Bass | BASS 2 | More resonance |
| Big Filter Sweep | SWEEP 1 | |
| Acid Sequence | SEQ 4 | 1/16 steps |
| Octave Arpeggio | ARPEGGIO 1 | Some guitar mixed in (Mix 70 %) |

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```

The test plucks a simulated guitar string across E2–E6 (392 notes) and checks pitch accuracy, latency and glitches; rings 270 notes for 3 s each from a harsher string model (weak and fast-fading fundamentals) and checks that no note changes octave while ringing; checks that an octave pull-off and a whole-tone bend are followed; checks the Mix blend; runs every sound at three Param settings checking for silence, overloads and NaN; tests MIDI In and measures CPU. `./test_dsp <folder>` also renders demo WAV files.
