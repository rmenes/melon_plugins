# Melon Trem — VST3 / LV2 tremolo

A classic tremolo in the vein of the **Boss TR-2**: the same three knobs, **Rate**, **Wave** and **Depth**, and the same range of sounds, from a smooth amp-style pulse to a hard on/off chop. On top of the pedal it adds host **Sync**, **Tap** tempo and a **Pan** stereo mode. For more modulation types (vibe, rotary, pattern trem…) use [Melon MultiMod](../MultiMod/README.md).
Built with [JUCE](https://juce.com). The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux) and a **Standalone** app.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Rate** | 0.3 – 15 Hz | 4 Hz | Speed of the tremolo. Ignored while Sync is on. |
| **Wave** | 0 – 100 % | 50 % | Shape of the volume change, like the pedal's Wave knob: a smooth, slightly rounded triangle at 0 %, a choppy square at 100 %, and everything in between. |
| **Depth** | 0 – 100 % | 50 % | How far the volume drops at the bottom of each cycle. 100 % cuts to silence. The peaks stay at your normal level (no boost). |
| **Sync** | Off, 4 bars … 1/32 (dotted and triplet values) | Off | Locks the tremolo to the host tempo; the value is the length of one cycle. While the host plays, the tremolo also follows the beat. |
| **TAP** | — | — | Click 2 or more times in rhythm: sets Rate to one cycle per tap and turns Sync off. |
| **Stereo** | Mono / Pan | Mono | **Mono:** both channels pulse together, like the pedal. **Pan:** the right channel pulses in anti-phase, so the sound moves from side to side (auto-pan). |

The square's edges are softened just enough not to click. No latency, no tail. The header has the series' standard **Bypass** and **Output** gain, and the preset row has 10 factory presets.

The display has a red **Rate LED** that pulses with the tremolo (like the pedal's), the rate readout (with the Sync value and host tempo) and two cycles of the **volume curve** with a moving playhead (left channel in blue, right channel in orange in Pan mode).

## Factory presets

| Preset | Rate / Sync | Wave | Depth | Stereo |
|---|---|---|---|---|
| Surf Shimmer | 6 Hz | 0 % | 60 % | Mono |
| Vintage Amp Trem | 4.5 Hz | 20 % | 45 % | Mono |
| Slow Pulse | 1.5 Hz | 10 % | 50 % | Mono |
| Gentle Throb | 2.5 Hz | 35 % | 30 % | Mono |
| Choppy Square | 6.5 Hz | 90 % | 90 % | Mono |
| Helicopter | 12 Hz | 100 % | 100 % | Mono |
| Eighth Chop | Sync 1/8 | 100 % | 100 % | Mono |
| Triplet Swing | Sync 1/8 T | 60 % | 75 % | Mono |
| Stereo Swirl | 3 Hz | 20 % | 70 % | Pan |
| Ambient Pan | 0.6 Hz | 0 % | 80 % | Pan |

The synced presets fall back to the Rate knob in hosts that don't report a tempo.

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```
