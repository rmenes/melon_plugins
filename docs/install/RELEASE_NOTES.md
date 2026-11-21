# Melon Plugins 1.2.1

## What's new

Three new plugins join the series, bringing it to 16. Like the others, each one has the Melon look and the preset menu with 10 factory presets and room for up to 64 of your own. All three are also in the Raspberry Pi / ARM Linux download.

### New plugin: Melon Tuner
A precise tuner for guitar and bass. Use it in **Chromatic** mode or pick one of **14 guitar and bass tunings** (Standard, Eb, D, Drop D, Drop C, DADGAD, Open G, Open D, 7-string, and 4/5/6-string bass). In a tuning it shows the nearest **string** with "Tune up" / "Tune down", even when a string is several semitones off, and ticks each string once it's in tune.

- **Needle** (±50 cents) or **Strobe** display.
- Adjustable **A4 reference** (410–460 Hz), plus Threshold and Smoothing.
- **Mute when engaged** works like a tuner pedal: map Bypass to a footswitch and tune silently.
- Accurate to well under 1 cent, from the low B of a 5-string bass to the 24th fret of the high E, at about 1 % CPU (fine on a Pi).

### New plugin: Melon Simple Gate
A no-fuss pedal-style noise gate with the controls of the Fortin Zuul+: **Threshold**, **Hold**, **Release** and a **Guitar / Bass** switch. It has zero latency, a fast attack that keeps your pick attack, and hysteresis so decaying notes don't chatter. A gate LED and an input meter show what it's doing. If you need the full mixing gate, Melon Gates of Doom is still there.

### New plugin: Melon Multi Delay
A stereo multi-machine delay in the spirit of the Strymon TimeLine, with **12 machines**: Digital, Tape, Analog, Dual, Pattern, Reverse, Ice, Duck, Swell, Trem, Filter and Lo-Fi. Each machine has two controls of its own on top of Time, Repeats, Mix, Filter, Grit and modulation.

- **Host sync** (straight, dotted and triplet) and **Tap tempo**.
- **Infinite** holds the current repeats while you play over them.
- **Trails** let the repeats ring out when you bypass it.

### Other changes
- Every plugin now shows **v1.2.1** under its title.

**Your projects:** sessions and presets saved with 1.0.x, 1.1.0 or 1.2.0 open in 1.2.1 with the same settings.

## Downloads

Pick the zip for your system under **Assets** below:

| System | File | Formats |
|---|---|---|
| **Windows** (64-bit) | `Melon-Plugins-v1.2.1-Windows.zip` | VST3, Standalone |
| **macOS** (Apple Silicon + Intel) | `Melon-Plugins-v1.2.1-macOS.zip` | VST3, AU, Standalone |
| **Linux** (x86_64) | `Melon-Plugins-v1.2.1-Linux.zip` | LV2, VST3, Standalone |
| **Linux arm64** (Raspberry Pi 4/5, 64-bit OS) | `Melon-Plugins-v1.2.1-Linux-arm64.zip` | LV2 |

Each zip has an `INSTALL.txt` with step-by-step instructions.

**Mac users:** the plugins are not signed with an Apple Developer ID. If macOS blocks them, run this once in Terminal after copying them:

```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Melon*.vst3 ~/Library/Audio/Plug-Ins/Components/Melon*.component
```

## Plugins

| Plugin | What it is |
|---|---|
| **Melon GraphicEQ** | 10/31-band graphic equalizer |
| **Melon Chorus** | Stereo guitar chorus |
| **Melon Flanger** | MXR M117R-style flanger |
| **Melon Guitar Synth** | EHX Micro Synthesizer-style guitar synth |
| **Melon Dirt** | 11 classic overdrive voicings |
| **Melon Muffs** | Six Big Muff Pi versions |
| **Melon Gates of Doom** | Gate / expander / ducker with lookahead |
| **Melon Reverberare** | Multi-machine reverb (12 machines, Infinite) |
| **Melon Delay 2290** | Digital delay with tap, sync, modulation and dynamic delay |
| **Melon Rack** | Neural amp model loader (NAM / AIDA-X) with gate, EQ and stereo IR cabinet |
| **Melon Comp** | Keeley Compressor Plus-style compressor with Blend, Tone and Single Coil / Humbucker release |
| **Melon Pro EQ** | 24-band parametric EQ with analyzer, dynamic EQ, Mid/Side and Linear Phase |
| **Melon Shifter** | Dual-voice pitch shifter (±24 semitones) with detune, delay and climbing feedback |
| **Melon Tuner** 🆕 | Guitar and bass tuner: chromatic or 14 tunings, needle or strobe, adjustable A4, mute |
| **Melon Simple Gate** 🆕 | Pedal-style noise gate for guitar and bass (Threshold, Hold, Release, Guitar/Bass) |
| **Melon Multi Delay** 🆕 | TimeLine-style multi-machine delay: 12 machines, sync, tap, Infinite and Trails |
