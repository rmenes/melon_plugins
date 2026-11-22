# Melon NAM — VST3 / LV2 Neural Amp Modeler loader

A simple, light loader for [Neural Amp Modeler](https://www.neuralampmodeler.com) captures (`.nam`), in the spirit of the official [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin): load a model, set the input, gate and tone, play. There is no cabinet on purpose: put **[Melon Cab](../Cab/README.md)** (or any IR loader) after it. That keeps each plugin small enough for the Melon Pedal, where you chain only what you need.

For the full rig on a computer (pedal + amp models, 6-band EQ, two IRs, TONE3000 downloads) use **[Melon Rack](../Rack/README.md)**.

Built with [JUCE](https://juce.com) and the [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) engine by Mike Oliphant. The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux x86_64 and arm64, e.g. Raspberry Pi) and a **Standalone** app.

## Signal chain

```
Input -> Noise Gate -> NAM model -> Bass / Middle / Treble -> Output
```

Mono: the plugin plays the left input (the guitar) and sends the result to both outputs.

## NAM File Loader

The panel under the preset row is the **NAM File Loader**. Click the model name to open its menu:

| Menu item | What it does |
|---|---|
| **Load NAM File...** | Opens a file chooser (`.nam`; AIDA-X `.aidax` and RTNeural `.json` models also work). |
| **Files in This Folder** | Every model in the same folder as the loaded one, to switch quickly. |
| **Recent Files** | The last 10 models you loaded (kept in `Documents/Melon Plugins/Recent/Melon NAM.txt`), plus **Clear Recent Files**. |
| **Show in Folder** | Opens the loaded file's folder in Explorer / Finder. |
| **Unload** | Removes the model (the plugin then passes the signal through the gate and EQ only). |

`<` and `>` step to the previous / next model in the same folder. You can also drag a `.nam` file onto the panel. The line under the name shows the model's sample rate, whether it is being resampled, and its loudness data.

Models load on a background thread, so there are no audio dropouts when you switch. Models trained at another rate than the session (e.g. a 48 kHz model in a 44.1 kHz session) are resampled automatically; the plugin then reports a few samples of latency to the host.

## Library index (Melon Pedal)

**LV2 only.** VST3, AU and Standalone don't have this parameter, so it never shows up in a DAW.

Hosts without a plugin window, like the Melon Pedal (mod-host), choose the model with the
**`model_index`** parameter instead of the file loader:

| Value | Result |
|---|---|
| `-1` (default) | No model; unloads the current one. |
| `0`…`65535` | The file at that position in the library list. Past the end of the list = no model, with a message in the status line. |

- **Library folder:** `$MELON_NAM_DIR` if set, otherwise `/data/media/NAM` on Linux, or `Documents/Melon Plugins/Library/NAM` elsewhere.
- **List:** every `.nam` / `.aidax` / `.json` file in that folder and all its sub-folders. Names starting with `.` are skipped, along with hidden folders. Sorted by relative path (`/` separators, UTF-8 byte order). The code is in `shared/MelonLibraryScan.h`; the pedal's `library.py` must produce the same order.
- **Example:** `patch_set 0 https://github.com/melon-audio/melon-nam:model_index 12`
- The saved file path in a project or user preset still wins over the index.
- A host only sends changes. To reload the same number after files were added, send `-1` first.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Input** | −20 – +20 dB | 0 dB | Level into the model: up = more drive, down = cleaner. |
| **Gate** | −100 – 0 dB | −80 dB | Noise gate threshold. 4 dB hysteresis and 30 ms hold, so decaying notes don't chatter. |
| **Noise Gate** | on / off | on | Turns the gate on and off. |
| **Bass** | 0 – 10 | 5 (flat) | Low shelf at 150 Hz, ±20 dB. |
| **Middle** | 0 – 10 | 5 (flat) | Mid band at 425 Hz, ±15 dB (narrower when cutting). |
| **Treble** | 0 – 10 | 5 (flat) | High shelf at 1.8 kHz, ±10 dB. |
| **EQ** | on / off | on | Turns Bass / Middle / Treble on and off. |
| **Normalize** | on / off | on | Uses the loudness data stored in the `.nam` file to bring every model to the same level. |
| **Model Size** | Full / Light | Full | NAM A2 models (architecture `SlimmableContainer`) store a full and a light network of the same capture. **Light** runs the small one and needs much less CPU (useful on the Melon Pedal). Other models ignore it. The loader shows "sizes: Full / Light" when a model has them. |

The header has the series' standard **Bypass** (latency-compensated crossfade) and **Output** gain.

## Presets

10 factory presets set the controls only and **keep the loaded model**, so you can try them on any capture. User presets (Save / Save As) also remember the model file. The model path is saved with your DAW project too; if the file has moved, the loader shows "File not found" with the old path.

| Preset | Settings |
|---|---|
| Flat (EQ Off) | EQ off |
| Tight High Gain | Gate −60 dB, Bass 4, Middle 6, Treble 6 |
| Scooped Metal | Gate −58 dB, Bass 7, Middle 2.5, Treble 7 |
| Warm Blues | Gate off, Bass 6, Middle 6, Treble 3.5 |
| Bright Clean | Gate off, Bass 4.5, Treble 7 |
| Mid Push Lead | Gate −65 dB, Bass 4, Middle 8, Treble 5.5 |
| Dark Jazz | Gate off, Bass 6, Middle 4.5, Treble 2 |
| Hot Input | Input +6 dB, Gate −60 dB |
| Hot Pickups (−6 dB) | Input −6 dB |
| Raw Model | Gate, EQ and Normalize off |

## Build

Part of the series build (`cmake --build build --target Nam_VST3`, or `build-vst3.bat Nam`). NeuralAudio is downloaded once by `cmake/MelonNeuralAudio.cmake` and shared with Melon Rack; `NEURALAUDIO_DIR` / `NEURALAUDIO_TAG` work the same way. Melon NAM has no network code, so it does not need libcurl on Linux.

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```

Covers the tone stack, the gate, the bypass delay and the resampler.

## Credits

- [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) by Steven Atkinson — the layout and the tone stack this plugin follows.
- [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) by Mike Oliphant (MIT) — the model engine (with [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore), MIT).
