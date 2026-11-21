# Melon Simple Gate — VST3 / LV2 noise gate for guitar and bass

A no-fuss pedal-style noise gate with the controls of the Fortin Zuul+: **Threshold**, **Hold** and **Release**, plus a **Guitar / Bass** switch. Put it after your drive or amp sim and turn Threshold up until the hiss between notes disappears. For the full mixing gate (ratio, knee, lookahead, side-chain filters, ducking) use [Melon Gates of Doom](../GatesOfDoom/README.md).
Built with [JUCE](https://juce.com). The same code produces **VST3** (Windows, Linux and Mac), **LV2** (Linux) and a **Standalone** app.

## Controls

| Control | Range | Default | What it does |
|---|---|---|---|
| **Threshold** | −90 – −10 dB | −55 dB | Level where the gate opens. It closes 4 dB lower (hysteresis), so decaying notes don't chatter. |
| **Hold** | 0 – 500 ms | 30 ms | Time the gate stays open after you stop playing, so short pauses don't cut the sound. |
| **Release** | 5 – 1000 ms | 80 ms | Fade-out time once the hold is over. Short = tight, percussive stops; long = natural ring-out. |
| **Instrument** | Guitar / Bass | Guitar | Voices the detector. **Guitar:** ignores hum below 70 Hz and fizz above 6 kHz; follows notes down to a 7-string low B. **Bass:** listens from 25 Hz to 3 kHz; follows notes down to a 5-string low B. |

Fixed, tuned for instruments: 0.3 ms attack (keeps the pick attack), fully closed when off (−90 dB), no lookahead (**no latency**). Both channels share one detector (stereo-linked). The header has the series' standard **Bypass** and **Output** gain, and the preset row has 10 factory presets.

The display shows a **gate LED** (green = open, red = closed, like the pedal), the **input level** against the **threshold** (orange marker; the shaded band below it is the hysteresis) and the current gain reduction.

## Factory presets

| Preset | Instrument | Threshold | Hold | Release |
|---|---|---|---|---|
| High-Gain Rhythm | Guitar | −50 dB | 20 ms | 60 ms |
| Djent Staccato | Guitar | −42 dB | 5 ms | 25 ms |
| Lead Sustain | Guitar | −68 dB | 80 ms | 400 ms |
| Clean Hum Cut | Guitar | −72 dB | 50 ms | 250 ms |
| Fuzz Tamer | Guitar | −55 dB | 30 ms | 120 ms |
| Doom Ring-Out | Guitar | −60 dB | 120 ms | 600 ms |
| Bass Pick Tight | Bass | −50 dB | 30 ms | 80 ms |
| Bass Fingerstyle | Bass | −62 dB | 60 ms | 200 ms |
| Bass Drive Gate | Bass | −48 dB | 25 ms | 70 ms |
| Slap & Pop | Bass | −52 dB | 15 ms | 50 ms |

## Test

```bash
g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp && ./test_dsp
```
