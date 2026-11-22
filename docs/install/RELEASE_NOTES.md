# Melon Plugins 1.2.3

## What's new

### New plugin: Melon MultiMod
A multi-machine modulation effect in the spirit of the Strymon Mobius. It has 12 machines: **Chorus, Flanger, Rotary, Vibe, Phaser, Filter, Formant, Vintage Trem, Pattern Trem, Autoswell, Destroyer** and **Quadrature**. They share Speed, Depth, Mix and Width, and each machine has two parameters of its own (Param 1 / Param 2), whose names change with the machine.

- **Host sync and tap tempo:** Sync locks the LFO to the host tempo, from 4 bars down to 1/32, including dotted and triplet values. While the host plays, the LFO also follows the beat, so Pattern Trem stays in time.
- **LFO wave:** Sine, Triangle, Square, Ramp Up, Ramp Down or Random.
- **Display:** shows the modulation of both channels with a moving playhead.
- **Presets:** 10 factory presets and the same preset menu as the rest of the series.

### Melon NAM: Model Size
A new **Model Size** switch (Full / Light) for NAM A2 models. These store a full and a light version of the same capture, and **Light** needs much less CPU. Other models ignore the switch.

### Melon NAM and Melon Cab: library index (LV2 only)
LV2 hosts can now choose a model or IR by number from a library folder (`model_index` / `ir_index`), for the Melon Pedal. The VST3, AU and Standalone versions don't have this parameter, so nothing changes in your DAW. See the plugins' READMEs for details.

### Other changes
- Every plugin now shows **v1.2.3** under its title.

**Your projects:** sessions and presets saved with 1.0.x, 1.1.0, 1.2.0, 1.2.1 or 1.2.2 open in 1.2.3 with the same settings.

## Downloads

Pick the zip for your system under **Assets** below:

| System | File | Formats |
|---|---|---|
| **Windows** (64-bit) | `Melon-Plugins-v1.2.3-Windows.zip` | VST3, Standalone |
| **macOS** (Apple Silicon + Intel) | `Melon-Plugins-v1.2.3-macOS.zip` | VST3, AU, Standalone |
| **Linux** (x86_64) | `Melon-Plugins-v1.2.3-Linux.zip` | LV2, VST3, Standalone |
| **Linux arm64** (Raspberry Pi 4/5, 64-bit OS) | `Melon-Plugins-v1.2.3-Linux-arm64.zip` | LV2 |

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
| **Melon MultiMod** 🆕 | Mobius-style multi-machine modulation: 12 machines, sync, tap and LFO wave |
