# Melon Rack — VST3 / LV2 neural amp model loader

A guitar rig in one plugin: a **noise gate**, a **pedal** and an **amp** played by neural models, a **6-band EQ** you can put anywhere in the chain, and a **stereo cabinet** built from two impulse responses. Melon Rack loads **NAM** (`.nam`) captures and **AIDA-X / RTNeural** (`.aidax`, `.json`) models, the formats shared on [TONE3000](https://www.tone3000.com). Built with [JUCE](https://juce.com) and the [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) engine.
The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux), **AU** (Mac) and a **Standalone** app.

The design follows **[NeuralRack](https://github.com/brummer10/NeuralRack)** by Hermann Meyer (brummer10): the same chain of two models in series, gate, movable EQ and a two-IR stereo cabinet, with resampling when the host's sample rate differs from the model's. Melon Rack is written from scratch for the Melon series (JUCE, Melon look) and contains no NeuralRack code. See [Credits](#credits).

![Melon Rack](docs/screenshot.png)

## Signal chain

```
Input ─► Gate ─► [EQ] ─► Pedal model ─► [EQ] ─► Amp model ─► [EQ] ─► Cabinet (IR 1 / IR 2) ─► Output
                  └──────── the EQ sits in one of the three [EQ] places ────────┘
```

The input is mono (pick the channel your guitar is on). The output is stereo.

## Controls

| Unit | Control | Range | Default | What it does |
|---|---|---|---|---|
| **Input** | Channel | Left / Right / Left + Right | Left | Which input channel feeds the rack. |
| | Input | −24 – +24 dB | 0 dB | Level into the rack. |
| | Gate | on / off | off | Noise gate. The LED lights while the gate is open. |
| | Threshold | −90 – −10 dB | −60 dB | Signal below it is muted (with 4 dB hysteresis and a short hold, so notes don't chatter). |
| **Pedal** | On | on / off | on | Switches the pedal model on/off (timing stays the same, no jump in latency). |
| | Model | file | — | Click the name to choose a file, drop a file on it, or use **<** **>** to step through the other models in the same folder. **X** unloads it. |
| | Normalize | on / off | on | Uses the loudness stored in the model to bring every model to a similar level, so switching models doesn't blast you. |
| | Input | −24 – +24 dB | 0 dB | Level into the model: drives it harder or softer. |
| | Output | −24 – +24 dB | 0 dB | Level out of the model. |
| **EQ** | On | on / off | off | Turns the EQ on. |
| | Position | Before Pedal / Between Pedal and Amp / After Amp | Between | Where the EQ sits in the chain. |
| | 100, 220, 500, 1.2k, 2.7k, 6k | −12 – +12 dB | 0 dB | Six peaking bands tuned for guitar. |
| **Amp** | (same as Pedal) | | | The second model, usually the amp capture. |
| **Cabinet** | On | on / off | on | Turns the cabinet (impulse responses) on. |
| | IR 1 / IR 2 | file | — | Impulse responses (`.wav`, `.aif`, `.flac`), loaded like the models. With only one IR loaded, it is used on both sides. |
| | Mode | Stereo L/R / Blend | Stereo L/R | **Stereo L/R:** IR 1 on the left, IR 2 on the right (e.g. two mics, or two cabinets). **Blend:** the two IRs mixed together on both sides. |
| | Blend | 0 – 100 % | 50 % | Blend mode only: balance between IR 1 (0 %) and IR 2 (100 %). |
| | Level | −24 – +12 dB | 0 dB | Cabinet output level. |

The header has the series' standard **Bypass** and **Output** gain. The footer shows the plugin's current **latency**.

Either model slot can stay empty. A pedal-only rig (Pedal model + Amp empty), an amp-only rig, or a "full rig" capture in the Amp slot with no cabinet all work.

## Files and presets

The paths of the loaded models and IRs are saved with the DAW project (and with the plugin's presets), and reloaded when the project opens. If a file has moved, its slot shows **File not found** in orange: load it again from its new place.

Models load in the background, so the audio never stops while a big model is being read.

## Sample rates and latency

Neural models are trained at one sample rate (almost always 48 kHz). If the DAW runs at another rate, Melon Rack converts the signal to the model's rate and back for that model (the slot shows *resampled*). This adds a small, constant latency that is reported to the DAW so it can compensate:

| Host rate | Latency (one 48 kHz model) |
|---|---|
| 48 kHz | 0 |
| 44.1 kHz | 40 samples (0.9 ms) |
| 88.2 / 96 kHz | 70 / 75 samples (0.8 ms) |
| 192 kHz | 146 samples (0.8 ms) |

With two resampled models, their latencies add up. For the lowest latency, run your DAW at the models' rate (48 kHz).
The impulse responses are resampled automatically and add no latency.

## How it works

- **Models:** [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) runs NAM WaveNet and LSTM models with its own optimized code (the same output as NAM Core, faster and lighter), and falls back to [RTNeural](https://github.com/jatinchowdhury18/RTNeural) for other AIDA-X / Keras models. The sample rate is read from the model file (`sample_rate`); models without one are treated as 48 kHz.
- **Resampling:** a windowed-sinc converter (32 taps at full bandwidth, Blackman window, low-pass at 92 % of the lower Nyquist), in `Source/Resampler.h`. Round trip error is below −100 dB.
- **Cabinet:** JUCE's partitioned convolution, zero latency, IRs trimmed and normalized.
- **Thread safety:** a new model is loaded and prepared on a background thread, then swapped in between two audio blocks.

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```

Tests the resampler (latency and accuracy at 22.05–192 kHz, anti-aliasing), the EQ, the gate and the latency compensation. The neural engine itself is tested by the NeuralAudio project.

## Build notes

CMake downloads NeuralAudio (with its submodules) on the first configure, like JUCE. To use a local clone instead:

```bash
git clone --recurse-submodules https://github.com/mikeoliphant/NeuralAudio
cmake -B build -DNEURALAUDIO_DIR=/path/to/NeuralAudio
```

`NEURALAUDIO_TAG` picks the branch, tag or commit to download (default `main`). Everything that talks to NeuralAudio is in `Source/NeuralModelSlot.cpp`, so a future change in its API only touches that file.

## Credits

- **NeuralRack** by Hermann Meyer (brummer10) — the design this plugin follows. BSD 3-Clause. https://github.com/brummer10/NeuralRack
- **NeuralAudio** by Mike Oliphant — model engine. MIT. https://github.com/mikeoliphant/NeuralAudio
- **Neural Amp Modeler** by Steven Atkinson — the NAM model format and reference implementation. MIT. https://github.com/sdatkinson/NeuralAmpModelerCore
- **RTNeural** by Jatin Chowdhury — engine for AIDA-X / Keras models. BSD 3-Clause. https://github.com/jatinchowdhury18/RTNeural
- **Eigen** — linear algebra used by the engines. MPL 2.0.

The full licence texts are in [`THIRD_PARTY.md`](THIRD_PARTY.md).
