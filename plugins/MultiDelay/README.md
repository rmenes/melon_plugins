# Melon Multi Delay — VST3 / LV2 delay

A stereo multi-machine delay in the spirit of the **Strymon TimeLine**: twelve delay machines sharing the same knobs, plus two machine-specific parameters, host sync, tap tempo, **Infinite** (hold) and **Trails**. Built with [JUCE](https://juce.com).
The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux) and a **Standalone** app.

## Machines

| Machine | Param 1 | Param 2 | Sound |
|---|---|---|---|
| **Digital** | Smear | Low Cut | Clean, full-range echo. Smear diffuses the repeats; Low Cut (20–800 Hz) thins them |
| **Tape** | Tape Age | Flutter | Warm, saturated tape echo with a low-end head bump; Age darkens the repeats, Flutter adds transport wobble |
| **Analog** | Age | Hiss | Bucket-brigade echo: the longer the time, the darker the repeats (like a real BBD clock) |
| **Dual** | Ratio | Routing | Two delays: the second runs at a ratio of Time (1/4 … 2x); Parallel (L/R), Series or Cross-feedback |
| **Pattern** | Pattern | Spread | Rhythmic multi-tap patterns (Eighths, Gallop, Swing, Triplets, Sixteenths, Syncopated, Cascade, Quintuplets) panned across the stereo field |
| **Reverse** | Smooth | Forward | Backwards repeats; Smooth sets the crossfade between slices, Forward blends normal echoes back in |
| **Ice** | Interval | Blend | Pitch-shifted slices (octave down … two octaves up) that climb further with every repeat |
| **Duck** | Sensitivity | Release | Repeats duck (−24 dB) while you play and swell back when you stop |
| **Swell** | Rise | Dry Swell | Every new note fades in (20 ms – 3 s); Dry Swell also swells the dry signal |
| **Trem** | Trem Rate | Trem Depth | Tremolo on the repeats (0.5–12 Hz) |
| **Filter** | Sweep Rate | Resonance | Resonant low-pass sweeping over the repeats (250 Hz – 4 kHz) |
| **Lo-Fi** | Bits | Downsample | Band-limited, bit-crushed (16 → 4 bits) and downsampled repeats |

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Machine** | 12 machines | Digital | Drop-down, plus arrows. Switching fades the repeats out and back in. |
| **TAP** | — | — | Tap 2 or more times in rhythm: sets Time and turns Sync off. |
| **Sync** | Off, 1/1 … 1/16 T | Off | Locks Time to the host tempo (straight, dotted and triplet values). |
| **Infinite** | on / off | off | Holds the current repeats forever; new notes play dry over them. |
| **Trails** | on / off | off | When bypassed, the repeats ring out instead of stopping. |
| **Time** | 1 – 5000 ms | 400 ms | Delay time (ignored while Sync is on). Tape and Analog glide when you turn it. |
| **Repeats** | 0 – 100 % | 35 % | Feedback. 100 % = (almost) endless. |
| **Mix** | 0 – 100 % | 35 % | 50 % = dry and delay both at full level; above 50 % the dry fades out. |
| **Filter** | 0 – 100 % | 80 % | Low-pass on the repeats from 800 Hz to 20 kHz (open at 100 %), inside the loop. |
| **Grit** | 0 – 100 % | 0 % | Saturation of the repeats, inside the loop. |
| **Speed / Depth** | 0 – 100 % | 30 % / 10 % | Delay-time modulation: 0.05–10 Hz, up to 6 ms. |
| **Param 1 / 2** | 0 – 100 % | 50 % | Depend on the machine (labels and values change with it). |

The header has the series' standard **Bypass**, **Output** gain and preset row (10 factory presets, up to 64 user presets). The display shows the machine, the delay time and where the repeats land (left channel up, right channel down; reversed repeats drawn as ramps).

## Factory presets

Studio Slapback, Warm Analog Lead, Dotted Eighth Shimmer, Wide Dual Stereo, Gallop Rhythm, Backwards Dream, Crystal Octaves, Ducked Vocal Echo, Violin Swell Pad, Broken Radio.

## How it works

Two long delay lines (up to 10 s, for Dual's 2x ratio and Reverse's read-back) with Hermite-interpolated reads. Each machine colours the repeats **inside** the feedback loop (tape head bump and saturation, BBD low-pass tied to the delay time, diffusion allpasses, bit-crushing), so every repeat is more degraded than the last; or shapes only the **output** (ducking, tremolo, filter sweep). Dual, Pattern, Reverse and Ice read the lines in their own way: two delays with routing, a set of panned taps, two crossfaded grains reading backwards at twice the speed, and a two-grain pitch shifter in the loop. Infinite recirculates the lines with a gain of exactly 1 and no new input.

This is not a copy of the TimeLine's algorithms or presets: it follows the same idea and controls.

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```
