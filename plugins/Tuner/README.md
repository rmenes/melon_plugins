# Melon Tuner — VST3 / LV2 guitar and bass tuner

A precise tuner for guitar and bass, with a needle or strobe display. Built with [JUCE](https://juce.com).
The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux, x86_64 and arm64) and a **Standalone** app.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Tuning** | Chromatic + 14 tunings | Chromatic | **Chromatic** shows the nearest note. A guitar or bass tuning shows the nearest **string** instead, so a string that is far off (even several semitones) still points to the note it should be, with "Tune up" / "Tune down". |
| **Display** | Needle / Strobe | Needle | **Needle:** cents meter (±50 cents). **Strobe:** three rows of bands that drift left (flat) or right (sharp) and stand still when the note is in tune; the rows move at 1×, 2× and 4× speed, so the top row shows the finest error. |
| **Mute when engaged** | on / off | off | Works like a tuner pedal: while the plugin is engaged the output is silent, so you can tune without being heard; bypass it to play again. A **MUTED** badge shows on the display. The tuner keeps listening, and the "Silent Stage Strobe" preset turns it on. |
| **Reference** | 410 – 460 Hz | 440 Hz | Concert pitch (frequency of A4). 442 Hz is common for orchestras, 415 Hz for baroque pitch. |
| **Threshold** | -90 – -20 dB | -60 dB | Input level below which the signal is ignored. Lower it for quiet pickups, raise it on a noisy stage. |
| **Smoothing** | 0 – 100 % | 50 % | How steady the reading is: low = fast and jumpy, high = slow and calm (useful for the long, wobbly notes of a bass). |

The header has the series' standard **Bypass** and **Output** gain. When **Bypass** is on, the signal always passes through (Mute when engaged only applies while the plugin is engaged), and the tuner still shows the note. Map Bypass to a footswitch or MIDI controller in your DAW to use it hands-free, like stepping on a tuner pedal.

### Tunings

| Tuning | Strings (low → high) |
|---|---|
| Guitar Standard | E2 A2 D3 G3 B3 E4 |
| Guitar Eb Standard | Eb2 Ab2 Db3 Gb3 Bb3 Eb4 |
| Guitar D Standard | D2 G2 C3 F3 A3 D4 |
| Guitar Drop D | D2 A2 D3 G3 B3 E4 |
| Guitar Drop C | C2 G2 C3 F3 A3 D4 |
| Guitar DADGAD | D2 A2 D3 G3 A3 D4 |
| Guitar Open G | D2 G2 D3 G3 B3 D4 |
| Guitar Open D | D2 A2 D3 F#3 A3 D4 |
| Guitar 7-String | B1 E2 A2 D3 G3 B3 E4 |
| Bass Standard | E1 A1 D2 G2 |
| Bass 5-String | B0 E1 A1 D2 G2 |
| Bass 6-String | B0 E1 A1 D2 G2 C3 |
| Bass Drop D | D1 A1 D2 G2 |
| Bass Eb Standard | Eb1 Ab1 Db2 Gb2 |

In a tuning, the string row under the meter highlights the string being tuned (blue when in tune, orange when not) and ticks each string once it has stayed in tune for a moment. The ticks reset when you change the tuning.

## Display

- Big note name and octave, with the **flat** (left) and **sharp** (right) lights; both turn blue when the note is within ±2 cents.
- Error in cents and the measured frequency.
- After the note stops, the last reading stays (dimmed) for 1.5 seconds.

## How it works

```
  in ─► 4th-order low-pass (1.8 kHz) ─┬─► full-rate buffer ───────────────────┐
                                      └─► decimate to ~11 kHz ─► YIN ─────────┴─► refine period ─► tracker ─► display
  in ─────────────────────────────────────────────────────── Mute when engaged ─► Output ─► out
```

- **YIN** pitch detection runs on a copy decimated to about 11 kHz, which keeps the CPU load around 1 % of one core (fine for a Raspberry Pi). The period is then **refined on the full-rate signal** around YIN's estimate, so a steady note is measured to well under 1 cent.
- Range **25 Hz – 1.4 kHz**: from the low B of a 5- or 6-string bass (30.9 Hz) to the 24th fret of the high E string (1318.5 Hz). A new estimate every 20 ms.
- The **tracker** ignores single-frame octave jumps (a common tuner glitch on the attack of a bass note) and needs two agreeing frames before it follows a new note.
- The audio is not processed: it passes through unchanged (or muted), so the tuner can stay on the track.

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```

Checks note maths and tunings, accuracy on sines and harmonic-rich notes from 31 Hz to 1.3 kHz at 44.1, 48 and 96 kHz (worst case about 0.2 cents), a bass note with a weak fundamental (no octave error), silence / low level / noise (no note), the audio path (unity, Mute, Bypass, Output) and CPU load.
