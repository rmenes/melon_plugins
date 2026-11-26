# Melon Plugins 1.2.5

## What's new

### New plugin: Melon Monolithic Synthesis
A monophonic guitar synthesizer in the vein of the Boss SY-200. Play your guitar normally: the plugin tracks each note you play and plays it on an analog-style synth. Bends, vibrato, slides, hammer-ons and pull-offs are all followed.

- **12 modes, 72 sounds:** LEAD, PAD, STRING, BELL, ORGAN, BASS, DUAL, SWEEP, NOISE, SFX, SEQ and ARPEGGIO, with 6 sounds each (Saw Lead, Warm Pad, FM Bell, Acid Bass, Sync Sweep…).
- **Param 1 / 2 / 3:** three knobs whose job changes with the mode (Filter, Resonance and Glide on LEAD; Attack, Release and Filter on PAD…). At 50 % you hear the sound exactly as designed.
- **Mix:** blends your dry guitar with the synth (equal-power, like the other Melon plugins), plus an **Effect** level for the synth.
- **Pitch tracking:** about 10 ms on high notes and up to about 30 ms on the low E string. An octave lock keeps a ringing note from jumping octaves when its fundamental fades.
- **SEQ and ARPEGGIO:** rhythmic filter sequences and arpeggios on the note you hold, synced to the host tempo (1/4 down to 1/32, with triplets).
- **Extras:** Sensitivity, Octave (±2), Pitch Snap, and **MIDI In** to play the synth from a MIDI pickup or keyboard instead of the guitar.
- **Display:** the note you're playing with its tuning in cents, an input meter with the trigger level, the 12 modes with the current one lit, and the SEQ/ARP steps as they play.
- **Presets:** 10 factory presets (Saw Lead, Pad Under Guitar, Crystal Bells, Acid Sequence, Octave Arpeggio…) and the same preset menu as the rest of the series.

It plays one note at a time: play single notes and mute the other strings. For a polyphonic, filter-based guitar synth, use Melon Guitar Synth.

### Other changes
- Every plugin now shows **v1.2.5** under its title.

**Your projects:** sessions and presets saved with 1.0.x, 1.1.0 or 1.2.0 – 1.2.4 open in 1.2.5 with the same settings.

## Downloads

Pick the zip for your system under **Assets** below:

| System | File | Formats |
|---|---|---|
| **Windows** (64-bit) | `Melon-Plugins-v1.2.5-Windows.zip` | VST3, Standalone |
| **macOS** (Apple Silicon + Intel) | `Melon-Plugins-v1.2.5-macOS.zip` | VST3, AU, Standalone |
| **Linux** (x86_64) | `Melon-Plugins-v1.2.5-Linux.zip` | LV2, VST3, Standalone |
| **Linux arm64** (Raspberry Pi 4/5, 64-bit OS) | `Melon-Plugins-v1.2.5-Linux-arm64.zip` | LV2 |

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
| **Melon Trem** | TR-2-style tremolo: Rate, Wave (triangle to square), Depth, sync, tap and stereo pan |
| **Melon Monolithic Synthesis** 🆕 | SY-200-style mono guitar synth: 12 modes × 6 sounds, 3 Params per mode, Mix, SEQ/ARP sync and MIDI In |
