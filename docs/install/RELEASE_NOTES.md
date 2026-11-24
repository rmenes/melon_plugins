# Melon Plugins 1.2.4

## What's new

### New plugin: Melon Trem
A classic tremolo in the vein of the Boss TR-2, with the same three knobs: **Rate**, **Wave** and **Depth**. It goes from a smooth amp-style pulse to a hard on/off chop.

- **Wave:** morphs from a smooth, slightly rounded triangle to a choppy square, like the pedal's Wave knob. The square's edges are softened just enough not to click.
- **Depth:** from a gentle shimmer to a full cut at the bottom of each cycle. The peaks stay at your normal level, so there's no volume jump when you switch it on.
- **Host sync and tap tempo:** Sync locks the tremolo to the host tempo, from 4 bars down to 1/32, including dotted and triplet values. While the host plays, it also follows the beat.
- **Stereo:** **Mono** pulses both channels together, like the pedal; **Pan** runs the right channel in anti-phase for an auto-pan.
- **Display:** a red Rate LED that pulses with the tremolo, like the pedal's, and the volume curve with a moving playhead.
- **Presets:** 10 factory presets (Surf Shimmer, Helicopter, Eighth Chop, Stereo Swirl…) and the same preset menu as the rest of the series.

### Other changes
- Every plugin now shows **v1.2.4** under its title.

**Your projects:** sessions and presets saved with 1.0.x, 1.1.0, 1.2.0, 1.2.1, 1.2.2 or 1.2.3 open in 1.2.4 with the same settings.

## Downloads

Pick the zip for your system under **Assets** below:

| System | File | Formats |
|---|---|---|
| **Windows** (64-bit) | `Melon-Plugins-v1.2.4-Windows.zip` | VST3, Standalone |
| **macOS** (Apple Silicon + Intel) | `Melon-Plugins-v1.2.4-macOS.zip` | VST3, AU, Standalone |
| **Linux** (x86_64) | `Melon-Plugins-v1.2.4-Linux.zip` | LV2, VST3, Standalone |
| **Linux arm64** (Raspberry Pi 4/5, 64-bit OS) | `Melon-Plugins-v1.2.4-Linux-arm64.zip` | LV2 |

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
| **Melon Tuner** | Guitar and bass tuner: chromatic or 14 tunings, needle or strobe, adjustable A4, mute |
| **Melon Simple Gate** | Pedal-style noise gate for guitar and bass (Threshold, Hold, Release, Guitar/Bass) |
| **Melon Multi Delay** | TimeLine-style multi-machine delay: 12 machines, sync, tap, Infinite and Trails |
| **Melon NAM** | Light Neural Amp Modeler loader with gate, Bass/Middle/Treble, Normalize and Model Size |
| **Melon Cab** | Cabinet IR loader (mono/stereo) with Low Cut, High Cut and Mix, zero latency |
| **Melon MultiMod** | Mobius-style multi-machine modulation: 12 machines, sync, tap and LFO wave |
| **Melon Trem** 🆕 | TR-2-style tremolo: Rate, Wave (triangle to square), Depth, sync, tap and stereo pan |
