# Melon Comp — VST3 / LV2 guitar compressor

A guitar compressor / sustainer modelled on the **Keeley Compressor Plus** pedal. Built with [JUCE](https://juce.com).
The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux) and a **Standalone** app.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Sustain** | 0 – 100 % | 50 % | Amount of compression. It is the gain inside the compressor's feedback loop (0 to +40 dB), so turning it up squeezes the peaks harder *and* lifts quiet, fading notes — the pedal's "expander / sustainer" behaviour. |
| **Level** | 0 – 100 % | 60 % | Output volume of the pedal (audio taper; 50 % = unity for the dry signal, 100 % = +12 dB). |
| **Blend** | 0 – 100 % | 75 % | Mix of the dry signal (kept in phase) and the compressed signal. Lower it to let the pick attack and dynamics through while keeping the sustain underneath. |
| **Tone** | 0 – 100 % | 50 % | Treble emphasis on the compressed signal (high shelf at 2.2 kHz, ±9 dB). Noon is flat; turn it up to bring back the sparkle compression takes away. |
| **Release** | Single Coil / Humbucker | Single Coil | The pedal's toggle switch. **Single Coil:** 2 ms attack, 380 ms release for long, smooth sustain. **Humbucker:** 1 ms attack, 120 ms release, so hotter pickups don't keep the compressor clamped down and every note stays defined. |

The header has the series' standard **Bypass** and **Output** gain.

The display is a scrolling 5-second history: **input level** (grey area), **output level** (blue) and **gain reduction** (orange, from the top), with a gain-reduction bar on the right.

## How it works

The model follows the OTA feedback compressor of the Ross / Dyna Comp family that the Keeley compressors grew from:

```
        ┌──────────────── dry (in phase) ────────────────┐
  in ───┤                                                Blend ─► Level ─► out
        └─► OTA cell ─► Sustain gain ─┬─► Tone ─► ───────┘
               ▲                      │
               └── bias ◄── cap ◄── rectifier (attack / release from the switch)
```

- The detector listens to the cell's **output** (feedback), which gives the soft, knee-less squeeze of the pedal (about 4:1 well above the threshold).
- The OTA input has a soft `tanh` curve, and the stage after it softly runs into its rails, which tames the attack "snap" on hard picking.
- The compressor runs 4× oversampled, so the fast attack doesn't add aliasing.
- Both channels share one detector (stereo-linked), so the stereo image never shifts.

## Starting points

| Use | Sustain | Level | Blend | Tone | Release |
|---|---|---|---|---|---|
| Country chicken-picking (Strat/Tele) | 60 % | 55 % | 80 % | 65 % | Single Coil |
| Funk rhythm, natural attack | 45 % | 55 % | 50 % | 55 % | Single Coil |
| Long sustaining leads (humbuckers) | 80 % | 50 % | 90 % | 55 % | Humbucker |
| Always-on "glue" before the amp | 30 % | 55 % | 40 % | 45 % | either |

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```

*Keeley and Compressor Plus are trademarks of their owner. Melon Comp is an independent emulation and is not affiliated with Keeley Electronics.*
