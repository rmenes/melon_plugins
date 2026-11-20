<img width="397" height="96" alt="melon-logo-ondark-header" src="https://github.com/user-attachments/assets/7618c0bc-3326-4c94-aa6f-49344cdb449a" />

# Melon Plugins

A series of **VST3** (Windows/Linux) and **LV2** (Linux x86_64 and arm64, e.g. Raspberry Pi) audio plugins built with [JUCE](https://juce.com).
All plugins share the same identity (manufacturer **Melon**) and the same build setup. JUCE is downloaded only once for the whole series.

## Plugins

| Plugin | Type | Folder | Status |
|---|---|---|---|
| **Melon GraphicEQ** | 10/31-band graphic equalizer | `plugins/GraphicEQ` | v1.0.0 |
| **Melon Chorus** | Stereo guitar chorus (Rate, Depth, Width, Mix, Tone) | `plugins/Chorus` | v1.0.0 |
| **Melon Flanger** | MXR M117R-style flanger (Manual, Width, Speed, Regen, Mix) | `plugins/Flanger` | v1.0.0 |
| **Melon Guitar Synth** | EHX Micro Synthesizer-style guitar synth (4 voices, Attack Delay, swept resonant filter) | `plugins/GuitarSynth` | v1.0.0 |
| **Melon Dirt** | 11 classic overdrive voicings (TS808, TS9, SD-1, Angry Charlie, RAT, BE-OD, Klon, Morning Glory, BD-1, OCD, ODR-1 styles) | `plugins/Dirt` | v1.0.0 |
| **Melon Muffs** | Six Big Muff Pi versions (Triangle, Ram's Head, Op-Amp, Russian Civil War, NYC, Pi 2) | `plugins/Muffs` | v1.0.0 |
| **Melon Gates of Doom** | Pro-G-style gate / expander / ducker with lookahead and side-chain filters | `plugins/GatesOfDoom` | v1.0.0 |
| **Melon Reverberare** | BigSky-style multi-machine reverb (12 machines, Infinite) | `plugins/Reverberare` | v1.0.0 |
| **Melon Delay 2290** | TC 2290-style digital delay (tap, sync, modulation, dynamic delay) | `plugins/Delay2290` | v1.0.0 |
| **Melon Rack** | Neural amp model loader (NAM / AIDA-X) with gate, EQ and stereo IR cabinet, after NeuralRack | `plugins/Rack` | v1.0.0 |
| **Melon Comp** | Keeley Compressor Plus-style guitar compressor (Sustain, Level, Blend, Tone, Single Coil/Humbucker release) | `plugins/Comp` | v1.0.0 |
| **Melon Pro EQ** | Pro-Q-style 24-band parametric EQ (analyzer, dynamic EQ, Mid/Side, Linear Phase, slopes up to 96 dB/oct) | `plugins/ProEQ` | v1.0.0 |
| **Melon Shifter** | Harmonizer-style dual-voice pitch shifter (±24 semitones, detune, delay with climbing feedback) | `plugins/Shifter` | v1.0.0 |

The full list of plugin codes and identifiers is in [`docs/PLUGIN_REGISTRY.md`](docs/PLUGIN_REGISTRY.md).

## Presets

Every plugin has a preset menu under its header, with 10 factory presets and room for 64 user presets. User presets are saved as small `.melonpreset` files in `Documents/Melon Plugins/Presets/<Plugin name>/`. Copy that folder to move or share your presets (the files work on Windows, Linux and Mac). The factory presets also appear in the host's own preset/program list. Melon Rack's factory presets also suggest amp models and IRs from [TONE3000](https://www.tone3000.com), which it can download and load after you sign in (see [plugins/Rack/README.md](plugins/Rack/README.md#tone3000)). Details: [docs/NEW_PLUGIN.md](docs/NEW_PLUGIN.md#presets).

## Folder layout

```
melon_plugins/
├── CMakeLists.txt            ← builds every plugin in the series
├── cmake/MelonCommon.cmake   ← shared settings (manufacturer, formats, JUCE, branding)
├── branding/                 ← Melon logo, icon and mascot (SVG + PNG)
├── plugins/
│   ├── GraphicEQ/            ← one folder per plugin (code, tests, README)
│   ├── Chorus/
│   ├── Flanger/
│   ├── GuitarSynth/
│   ├── Dirt/
│   ├── Muffs/
│   ├── GatesOfDoom/
│   ├── Reverberare/
│   ├── Delay2290/
│   ├── Rack/
│   ├── Comp/
│   ├── ProEQ/
│   └── Shifter/
├── shared/                   ← code shared by the newer plugins (DSP blocks, base classes, test helpers)
├── docs/
│   ├── NEW_PLUGIN.md         ← step-by-step guide to creating a new plugin
│   ├── PLUGIN_REGISTRY.md    ← codes and URIs already in use
│   ├── install/              ← install instructions and release notes put in the release zips
│   └── github-actions-build.yml ← copy of .github/workflows/build.yml
└── builds/                   ← compiled plugins, ready to install
    ├── windows/
    └── linux/
```

## Branding

Every plugin shows the Melon logo at the top-left of its window, followed by the plugin name.
- **Header logo:** `branding/png/melon-logo-ondark-header.png`, a version of the logo for dark backgrounds, stored at 2× size for high-DPI screens. The build embeds it in a shared library (`MelonBranding`) that every plugin links automatically.
- **Icon:** `branding/png/melon-icon-512.png` and `melon-icon-128.png`. These become the Standalone app icon and the icon of the `.vst3` folder in Windows Explorer.

If you change the logo, regenerate the header image from `branding/melon-logo-ondark.svg`. It needs the [Fredoka](https://fonts.google.com/specimen/Fredoka) font installed. For example, with `rsvg-convert` and ImageMagick:

```bash
rsvg-convert -w 2400 -h 1200 branding/melon-logo-ondark.svg -o big.png
magick big.png -trim +repage -resize x96 branding/png/melon-logo-ondark-header.png
```

## Building on Windows

You will need:
- **Visual Studio 2022 Community** (free), with the *"Desktop development with C++"* workload.
- **CMake 3.22 or newer**, which ships with Visual Studio.
- **Git**, because CMake downloads JUCE automatically on the first build.

Open the *"Developer PowerShell for VS 2022"* in `E:\melon_plugins` and run:

```powershell
# every plugin in the series
cmake -B build -A x64
cmake --build build --config Release

# or a single plugin
cmake --build build --config Release --target GraphicEQ_VST3
```

Each plugin ends up in `build\plugins\<Plugin>\<Plugin>_artefacts\Release\VST3\`.

Shortcuts (double-click in Explorer, or run from a terminal in `E:\melon_plugins`):
- `build-vst3.bat <Target>` builds one plugin and copies it to `builds\windows` (e.g. `build-vst3.bat Dirt`).
- `build-all-vst3.bat` builds every plugin in the series and copies them all to `builds\windows`.
To install it, copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\` and rescan plugins in your DAW.

## Building on Linux

```bash
# Debian/Ubuntu
sudo apt install build-essential cmake git libasound2-dev libjack-jackd2-dev libx11-dev \
  libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

To install, copy the `.lv2` folders to `~/.lv2/` and the `.vst3` folders to `~/.vst3/`.

The same steps work on a 64-bit ARM board (e.g. Raspberry Pi 4/5 with 64-bit Raspberry Pi OS); add `-DMELON_FORMATS=LV2` to build only the LV2s. Compiling the whole series on a Pi is slow; the release zips already include ready-made arm64 LV2s.

## Downloads and automatic builds (GitHub Actions)

**Ready-to-install plugins are on the [Releases page](https://github.com/rmenes/melon_plugins/releases):** one zip per system (Windows, macOS, Linux x86_64, Linux arm64), each with an `INSTALL.txt`.

The build is defined in `.github/workflows/build.yml` (copy in `docs/github-actions-build.yml`). It builds the whole series on GitHub's machines and runs the tests:

| Job | Formats | Notes |
|---|---|---|
| Windows | VST3, Standalone | x64 |
| macOS | VST3, AU, Standalone | Universal (Apple Silicon + Intel), macOS 10.15 or newer; AUs are checked with `auval` |
| Linux | LV2, VST3, Standalone | x64 |
| Linux arm64 | LV2 | aarch64 (Raspberry Pi 4/5 with a 64-bit OS), built natively on GitHub's `ubuntu-22.04-arm` runner |

### Publishing a new release

1. On GitHub, open **Releases → Draft a new release**.
2. Under **Choose a tag**, type a new version (for example `v1.2.0`) and pick **Create new tag on publish**.
3. Give it a title (for example `Melon Plugins 1.2.0`). Leave the description empty to get the standard plugin list and install notes, or write your own.
4. Click **Publish release**.

GitHub then builds all four systems (about 40 minutes) and attaches `Melon-Plugins-v1.2.0-Windows.zip`, `-macOS.zip`, `-Linux.zip` and `-Linux-arm64.zip` to the release. Before tagging, bump `MELON_VERSION` in `cmake/MelonCommon.cmake` so the plugins show the same version.

Every ordinary push is also built and tested; those zips are under **Actions → (run) → Artifacts** (kept for 90 days). `builds/` is in `.gitignore`: compiled plugins come from GitHub, so they don't need to be in the repository.

### Installing on a Mac

Copy the `.vst3` folders to `~/Library/Audio/Plug-Ins/VST3/` and the `.component` folders to `~/Library/Audio/Plug-Ins/Components/`, then rescan in your DAW.
The plugins are not signed with an Apple Developer ID, so macOS may block them. To allow them, run this once in Terminal:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Melon*.vst3 ~/Library/Audio/Plug-Ins/Components/Melon*.component
```

To distribute them to other people without that step, they need to be signed and notarized with an Apple Developer account.

## Build options

| Option | Default | Purpose |
|---|---|---|
| `MELON_FORMATS` | `VST3;LV2;Standalone` | Formats to build. |
| `MELON_VST3_MANIFEST` | `ON` | Generates the VST3 `moduleinfo.json`. Turn it off when cross-compiling. |
| `JUCE_DIR` | empty | Uses a local copy of JUCE instead of downloading it. |
| `NEURALAUDIO_DIR` | empty | Melon Rack only: uses a local clone of [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) (cloned with `--recurse-submodules`) instead of downloading it. |
| `NEURALAUDIO_TAG` | `main` | Melon Rack only: NeuralAudio branch, tag or commit to download. |

Example: `cmake -B build -A x64 "-DMELON_FORMATS=VST3"`

## JUCE license

JUCE 8 is free under the **AGPLv3** license. If you distribute the plugins, their source code must also be released under AGPLv3. The alternative is a commercial JUCE license (see the options at juce.com). There are no restrictions for personal use.
