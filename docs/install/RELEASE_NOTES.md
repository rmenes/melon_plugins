## What's new in 1.2.0

### New plugin: Melon Shifter
- A dual-voice pitch shifter: two voices shifting up to ±24 semitones each, with detune, delay and climbing feedback for cascading harmonies.
- 10 factory presets and the same preset menu as the rest of the series.

### Raspberry Pi / ARM Linux
- New download: **`Melon-Plugins-…-Linux-arm64.zip`**, the LV2 versions of every plugin for 64-bit ARM Linux, such as a Raspberry Pi 4 or 5 with Raspberry Pi OS (64-bit). See its `INSTALL.txt` for setup and performance tips.

### Other changes
- Every plugin now shows **v1.2.0**.

**Your projects:** sessions and presets saved with 1.0.x or 1.1.0 open in 1.2.0 with the same settings.

<details>
<summary>What was new in 1.1.0</summary>

### Presets in every plugin
- A new preset row under each plugin's header: **Preset [<] [name] [>] [Save] [Save As]**. Click the name for the full menu (factory presets, your presets, Save, Save As, Delete, Reset to Default, Open Preset Folder).
- **10 factory presets per plugin**, plus Default. They also appear in your DAW's own preset/program list.
- **Up to 64 user presets per plugin**, saved as small `.melonpreset` files in `Documents/Melon Plugins/Presets/<Plugin name>/`. Updating the plugins never touches them, and the files work on Windows, macOS and Linux, so you can back them up, copy them to another computer or share them.
- A `*` after the preset name means you changed something since loading or saving it. The current preset is remembered in your DAW project.

### Melon Rack + TONE3000
- New TONE3000 row under the rack. Each factory preset suggests a pedal, amp and/or cabinet IR from the [TONE3000](https://www.tone3000.com) library.
- **Sign in** with your free TONE3000 account: Melon Rack shows a short code and you confirm it on tone3000.com (your password never goes through the plugin).
- **Click a suggestion** to download it and load it into its slot, or use **Load suggested tones** for the whole set. The **...** button lets you choose another capture of the same tone or view it on the website.
- Downloads go to `Documents/Melon Plugins/TONE3000/` and only happen when you click. Use **Save As** afterwards: user presets remember the loaded models and IRs.
- Files downloaded on the TONE3000 website don't load by themselves: drag them onto a slot, or click the slot to choose them.
- The tones belong to their creators and are not included in the download. Thanks to everyone who shares their captures on TONE3000.

### Other changes
- The version number now appears under each plugin's title (e.g. **v1.1.0**) and is reported to your DAW.
- Plugin windows are slightly taller to fit the preset row (Melon Rack a bit more, for the TONE3000 row).
- Linux: Melon Rack uses libcurl for its TONE3000 downloads. It is installed on almost every desktop distribution; if not, install `libcurl4` (Debian/Ubuntu) or `libcurl` (Fedora/Arch).

**Your projects:** sessions saved with 1.0.0 open in 1.1.0 with the same settings. The preset name shows as "Default *".

</details>

## Downloads

Pick the zip for your system under **Assets** below:

| System | File | Formats |
|---|---|---|
| **Windows** (64-bit) | `Melon-Plugins-…-Windows.zip` | VST3, Standalone |
| **macOS** (Apple Silicon + Intel) | `Melon-Plugins-…-macOS.zip` | VST3, AU, Standalone |
| **Linux** (x86_64) | `Melon-Plugins-…-Linux.zip` | LV2, VST3, Standalone |
| **Linux arm64** (Raspberry Pi 4/5, 64-bit OS) | `Melon-Plugins-…-Linux-arm64.zip` | LV2 |

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
