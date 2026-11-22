# Melon Plugins 1.2.2

## What's new

### New plugin: Melon NAM
A simple, light loader for Neural Amp Modeler captures (`.nam`), in the spirit of the official NeuralAmpModelerPlugin. Load a model from the **NAM File Loader** menu and shape it with Input, Noise Gate and a Bass / Middle / Treble tone stack. AIDA-X models also work. It has no cabinet on purpose, so put Melon Cab after it. It has 10 factory presets and the same preset menu as the rest of the series.

### New plugin: Melon Cab
A simple cabinet IR loader for guitar and bass. Load a mono or stereo IR (`.wav`, `.aif`, `.flac`) from the **IR File Loader** menu, then trim it with Low Cut and High Cut and blend it with Mix. It has zero latency, 10 factory presets and the same preset menu as the rest of the series.

Together, Melon NAM and Melon Cab make a light amp + cabinet chain that runs well on a Raspberry Pi. Melon Rack is still there for the full rig on a computer.

### Other changes
- Melon Shifter: at 100 % Mix, the original pitch no longer sneaks through right after starting or when the pitch jumps.
- Melon Tuner (LV2): the detected frequency is also sent to the host, for hosts that can't show the plugin's window.
- Every plugin now shows **v1.2.2** under its title.

**Your projects:** sessions and presets saved with 1.0.x, 1.1.0, 1.2.0 or 1.2.1 open in 1.2.2 with the same settings.

## Downloads

Pick the zip for your system under **Assets** below:

| System | File | Formats |
|---|---|---|
| **Windows** (64-bit) | `Melon-Plugins-v1.2.2-Windows.zip` | VST3, Standalone |
| **macOS** (Apple Silicon + Intel) | `Melon-Plugins-v1.2.2-macOS.zip` | VST3, AU, Standalone |
| **Linux** (x86_64) | `Melon-Plugins-v1.2.2-Linux.zip` | LV2, VST3, Standalone |
| **Linux arm64** (Raspberry Pi 4/5, 64-bit OS) | `Melon-Plugins-v1.2.2-Linux-arm64.zip` | LV2 |

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
| **Melon NAM** 🆕 | Light Neural Amp Modeler loader with gate, Bass/Middle/Treble and Normalize |
| **Melon Cab** 🆕 | Cabinet IR loader (mono/stereo) with Low Cut, High Cut and Mix, zero latency |
