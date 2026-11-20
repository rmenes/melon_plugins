# Melon Shifter — VST3 / LV2 pitch shifter

A dual-voice pitch shifter in the spirit of the classic studio **"harmonizer"** racks (Eventide H910 / H949). Built with [JUCE](https://juce.com).
The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux) and a **Standalone** app.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Pitch A** | -24 – +24 st | +12 st | Pitch of voice A in semitones. |
| **Detune A** | -50 – +50 ct | 0 ct | Fine tuning of voice A in cents. 5–15 cents gives a doubling / chorus thickness. |
| **Pitch B** | -24 – +24 st | -12 st | Pitch of voice B (active when **Voice B** is on). |
| **Detune B** | -50 – +50 ct | 0 ct | Fine tuning of voice B. Detune A and B in opposite directions for a wide doubler. |
| **Width** | 0 – 100 % | 60 % | With Voice B on, spreads voice A to the left and voice B to the right. With Voice B off, voice A stays centred. |
| **Delay** | 0 – 1000 ms | 0 ms | Delay before the shifted signal. Short delays (10–30 ms) thicken, long ones give pitched echoes. |
| **Feedback** | 0 – 100 % | 0 % | Sends the delayed, shifted signal back into the shifter, so every repeat climbs (or falls) another step: +12 gives a rising shimmer, +7 a spiral of fifths. |
| **Tone** | 1 – 20 kHz | 20 kHz | High cut on the shifted signal (12 dB/oct). Turn it down to tame the fizz of big upward shifts. |
| **Mix** | 0 – 100 % | 50 % | Balance of dry and shifted signal (equal-power). 100 % = shifted only, for retuning the whole guitar. |
| **Mode** | Tight / Normal / Smooth | Normal | Splice window: **Tight** (20 ms, ~12 ms latency) for single notes and riffs; **Normal** (40 ms, ~24 ms); **Smooth** (70 ms, ~41 ms) for chords and pads. |
| **Voice B** | on / off | off | Turns on the second voice. |

The header has the series' standard **Bypass** and **Output** gain.

The display is a pitch ruler from -24 to +24 semitones: the dry note at 0, voice A (blue, above) and voice B (orange, below) with their intervals, and — when Feedback is up — the climbing echoes as fading dots.

## How it works

```
                ┌──────────────────────────── dry ─────────────────────────────┐
  in ──► (+) ──►┤                                                              Mix ─► Output ─► out
          ▲     └─► buffer ─┬─► Voice A (pitch, detune) ─► pan ─┐              │
          │                 └─► Voice B (pitch, detune) ─► pan ─┴─► Tone ─► Delay ─┘
          └──────────────────────── Feedback ◄──────────────────────────────┘
```

- **Time-domain shifting, like the original racks.** Each voice reads the input buffer at `ratio` times normal speed with two read taps. While one tap plays, the other jumps back and fades in; their sin² crossfades always add up to exactly 1, so the level never dips.
- **De-glitching.** When a tap jumps, it searches a few milliseconds for the spot where the waveform best matches what the other tap is playing (tapered normalised cross-correlation, refined to a fraction of a sample). The crossfade then happens between two signals in phase — no hollow "phasey" dips on sustained notes.
- **Stereo-safe.** The splice search uses a mono sum, and both channels share the same tap positions, so the stereo image stays put.
- **Pitch changes glide** over about 25 ms, so automating Pitch A (or sweeping it with a MIDI-mapped expression pedal) gives Whammy-style bends without zipper noise.
- Typical CPU use: under 1 % of one core at 48 kHz (stereo, both voices, Smooth mode).
- Like the hardware, the shifted signal sits a little behind the dry one (the latency shown in the display). The plugin does not report it to the host, so the dry signal is never delayed.

## Starting points

| Use | Pitch A | Pitch B | Mode | Delay | Feedback | Mix |
|---|---|---|---|---|---|---|
| Octave-up lead | +12 | — | Tight | 0 | 0 % | 40 % |
| Bass-like octave down | -12 | — | Normal | 0 | 0 % | 55 % |
| Tune down a whole step | -2 | — | Tight | 0 | 0 % | 100 % |
| Wide doubler | 0 / +9 ct | 0 / -9 ct | Tight | 18 ms | 0 % | 45 % |
| Shimmer | +12 | — | Smooth | 250 ms | 55 % | 35 % |

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```

The test checks pitch accuracy at every interval and mode (44.1, 48 and 96 kHz), level and smoothness across splices, the two voices and Width, Delay, climbing Feedback, Tone, Mix, Bypass and Output, and that sweeping the pitch or switching modes while playing stays clean.

*Eventide, H910 and H949 are trademarks of their owner. Melon Shifter is an independent design inspired by those units and is not affiliated with Eventide.*
