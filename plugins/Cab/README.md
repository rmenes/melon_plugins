# Melon Cab — VST3 / LV2 impulse response (cabinet) loader

A simple IR loader for guitar and bass cabinets. Load a `.wav` impulse response, trim the lows and highs, and blend it with the dry signal if you like. It is the cabinet to put after **[Melon NAM](../Nam/README.md)** (or any amp sim or drive). For two IRs with stereo / blend inside one plugin, see **[Melon Rack](../Rack/README.md)**.

Built with [JUCE](https://juce.com). The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux x86_64 and arm64, e.g. Raspberry Pi) and a **Standalone** app.

## Signal chain

```
Input -> IR (convolution) -> Low Cut / High Cut -> Mix with the dry signal -> Output
```

- **Mono source** (mono track, or the Melon Pedal) with a mono IR: one convolution, the result on both outputs. This is the lightest setting.
- **Stereo source:** each side goes through the IR. A **stereo IR** gives the left and right sides their own channel.

Zero latency. The IR is trimmed (silence at the start and end), normalized to a standard level and resampled to the session rate automatically.

## IR File Loader

The panel under the preset row is the **IR File Loader**. Click the IR name to open its menu:

| Menu item | What it does |
|---|---|
| **Load IR File...** | Opens a file chooser (`.wav`, `.aif`, `.aiff`, `.flac`). |
| **Files in This Folder** | Every IR in the same folder as the loaded one, to switch quickly (handy for IR packs with many mic positions). |
| **Recent Files** | The last 10 IRs you loaded (kept in `Documents/Melon Plugins/Recent/Melon Cab.txt`), plus **Clear Recent Files**. |
| **Show in Folder** | Opens the loaded file's folder in Explorer / Finder. |
| **Unload** | Removes the IR (the plugin then passes the dry signal through the filters). |

`<` and `>` step to the previous / next IR in the same folder. You can also drag a file onto the panel. The line under the name shows whether the IR is mono or stereo, its length and its sample rate.

## Library index (Melon Pedal)

Hosts without a plugin window, like the Melon Pedal (mod-host), choose the IR with the
**`ir_index`** parameter instead of the file loader:

| Value | Result |
|---|---|
| `-1` (default) | No IR; unloads the current one. Desktop use is unchanged. |
| `0`…`65535` | The file at that position in the library list. Past the end of the list = no IR, with a message in the status line. |

- **Library folder:** `$MELON_IR_DIR` if set, otherwise `/data/media/IR` on Linux, or `Documents/Melon Plugins/Library/IR` on Windows and macOS.
- **List:** every `.wav` / `.aif` / `.aiff` / `.flac` file in that folder and all its sub-folders. Names starting with `.` are skipped, along with hidden folders. Sorted by relative path (`/` separators, UTF-8 byte order). The code is in `shared/MelonLibraryScan.h`; the pedal's `library.py` must produce the same order.
- **Example:** `patch_set 0 https://github.com/melon-audio/melon-cab:ir_index 12`
- The saved file path in a project or user preset still wins over the index.
- A host only sends changes. To reload the same number after files were added, send `-1` first.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Low Cut** | 20 – 500 Hz | 20 Hz (off) | 12 dB/oct high-pass after the IR. Tightens boomy cabinets. |
| **High Cut** | 2 – 20 kHz | 20 kHz (off) | 12 dB/oct low-pass after the IR. Tames fizz and harshness. |
| **Mix** | 0 – 100 % | 100 % | Dry / cabinet balance. 100 % = cabinet only. |

The header has the series' standard **Bypass** and **Output** gain.

## Presets

10 factory presets set the controls only and **keep the loaded IR**. User presets (Save / Save As) also remember the IR file. The IR path is saved with your DAW project too; if the file has moved, the loader shows "File not found" with the old path.

| Preset | Low Cut | High Cut | Mix |
|---|---|---|---|
| Full Range | off | off | 100 % |
| Tight Low End | 90 Hz | off | 100 % |
| Modern Metal | 80 Hz | 8.5 kHz | 100 % |
| Smooth Top | off | 6.5 kHz | 100 % |
| Vintage Dark | 70 Hz | 5 kHz | 100 % |
| Bright & Open | 60 Hz | 16 kHz | 100 % |
| Mix Ready | 100 Hz | 9 kHz | 100 % |
| Lo-Fi Radio | 300 Hz | 3 kHz | 100 % |
| Half Blend | off | off | 50 % |
| Bass Cabinet | 30 Hz | 5 kHz | 100 % |

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```

Covers the Low Cut / High Cut filters (the convolution is JUCE's `juce::dsp::Convolution`).

Library list order (shared by Melon NAM and Melon Cab):

```bash
cd ../../shared/tests && g++ -std=c++17 -O2 -I.. test_library_scan.cpp -o test_library_scan && ./test_library_scan
```
