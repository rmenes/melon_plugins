# Melon MultiMod — VST3 / LV2 modulation

A stereo multi-machine modulation effect in the spirit of the **Strymon Mobius**: twelve modulation machines sharing the same knobs, plus two machine-specific parameters, an LFO waveform choice, host sync and tap tempo. Built with [JUCE](https://juce.com).
The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux) and a **Standalone** app.

## Machines

| Machine | Param 1 | Param 2 | Sound |
|---|---|---|---|
| **Chorus** | Mode | Tone | Classic (one voice), Dimension (two opposite voices) or Multi (three voices); Tone darkens the chorus (1.5–20 kHz) |
| **Flanger** | Regen | Manual | Swept short delay; Regen from −95 % to +95 % (50 % = none), Manual sets where the sweep starts (8 → 0.2 ms) |
| **Rotary** | Ramp | Horn Mix | Rotating horn and drum with Doppler and amplitude modulation; Ramp is how long the rotors take to speed up or slow down (0.2–5 s); Width spreads the two microphones |
| **Vibe** | Bias | Mode | Photocell four-stage phaser with uneven stages and a lagging lamp; Bias moves the sweep, Mode is Chorus (mixed with dry) or Vibrato (pitch only) |
| **Phaser** | Stages | Regen | 2, 4, 6, 8, 12 or 16 stages; Regen adds feedback (up to 90 %) |
| **Filter** | Resonance | Type | Resonant filter (120 Hz – 15 kHz): Low Pass, Band Pass or High Pass swept by the LFO, or Env Up / Env Down following your playing (Speed sets how fast it falls back) |
| **Formant** | Vowels | Resonance | Three-formant talking filter morphing between two vowels (A-E, A-I, A-O, A-U, E-I, E-O, O-U, I-U) |
| **Vintage Trem** | Type | Shape | Tube (bias), Harmonic (low and high bands in opposite phase) or Photocell; Shape goes from smooth to choppy |
| **Pattern Trem** | Pattern | Smooth | Rhythmic step tremolo (Pulse, Gallop, Reverse Gallop, Syncopated, Triplets, Accents, Ramp Up, Stutter); Speed / Sync set the length of **one step** |
| **Autoswell** | Rise | Sensitivity | Each new note fades in (20 ms – 3 s); Sensitivity sets the detection threshold (−20 to −65 dB) |
| **Destroyer** | Bits | Downsample | Bit crusher (16 → 3 bits) and sample-rate reducer (1/1 … 1/32), the rate swept by the LFO |
| **Quadrature** | Mode | Range | AM (ring modulation at full Depth), FM, Shift Up / Shift Down (frequency shifter, Depth = feedback); Range multiplies Speed by 1, 10 or 100 |

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Machine** | 12 machines | Chorus | Drop-down, plus arrows. Switching fades the effect out and back in. |
| **TAP** | — | — | Tap 2 or more times in rhythm: sets Speed (one cycle per tap; one step in Pattern Trem) and turns Sync off. |
| **Sync** | Off, 4 bars … 1/32 | Off | Locks the LFO to the host tempo (straight, dotted and triplet values). While the host plays, the LFO is also locked to the beat. |
| **Wave** | Sine, Triangle, Square, Ramp Up, Ramp Down, Random | Sine | LFO shape. Rotary, Pattern Trem, Autoswell and Quadrature ignore it (greyed out). |
| **Speed** | 0.05 – 15 Hz | 1 Hz | LFO speed (ignored while Sync is on). |
| **Depth** | 0 – 100 % | 50 % | Amount of modulation. |
| **Mix** | 0 – 100 % | 50 % | 50 % = dry and effect in equal parts; 100 % = effect only (use it for the trems, filters and Destroyer). |
| **Width** | 0 – 100 % | 50 % | Stereo: offsets the right channel's LFO by up to half a cycle (one step in Pattern Trem, the microphone spread in Rotary). 0 % = mono. |
| **Param 1 / 2** | 0 – 100 % | 50 % | Depend on the machine (labels and values change with it). |

The header has the series' standard **Bypass**, **Output** gain (the Mobius' Level) and preset row (10 factory presets, up to 64 user presets). The display shows the machine, the speed and two cycles of the modulation (left channel in blue, right in orange) with a moving playhead; Pattern Trem shows its whole pattern and Autoswell its fade-in curve.

## Factory presets

Lush Stereo Chorus, Jet Flanger, Fast Rotary, Machine Gun Vibe, Orange Four Stage, Funky Auto Wah, Talking Vowels, Brownface Harmonic, Sixteenth Stutter, Violin Swells.

## How it works

One LFO with a running position (so the random wave and the step patterns stay in place), the right channel offset by Width. Chorus, Flanger, Rotary and FM read Hermite-interpolated delay lines; Vibe and Phaser are chains of first-order allpasses (the Vibe's four stages tuned far apart, driven by a lamp-and-photocell model); Filter and Formant use TPT state-variable filters; Rotary splits the signal at 800 Hz (Linkwitz-Riley) into a horn with Doppler and a slower, heavier drum; the frequency shifter uses a 90° allpass network (Hilbert pair). Each machine fades out and back in when you switch.

This is not a copy of the Mobius' algorithms or presets: it follows the same idea and controls.

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```
