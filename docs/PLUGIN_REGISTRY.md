# Melon plugin registry

Every plugin needs **unique, permanent** identifiers. DAWs use them to recognise the plugin inside saved projects. If you change an identifier after using the plugin in a project, the DAW treats it as a different plugin and the project loses track of it.

## Series identity

| Item | Value | Defined in |
|---|---|---|
| Manufacturer | `Melon` | `cmake/MelonCommon.cmake` |
| Manufacturer code | `Meln` | `cmake/MelonCommon.cmake` |
| LV2 URI base | `https://github.com/melon-audio` | `cmake/MelonCommon.cmake` |

## Plugins

| Name | CMake target | Plugin code | LV2 URI | Version |
|---|---|---|---|---|
| Melon GraphicEQ | `GraphicEQ` | `Geq1` | `https://github.com/melon-audio/melon-graphiceq` | 1.2.0 |
| Melon Chorus | `Chorus` | `Chr1` | `https://github.com/melon-audio/melon-chorus` | 1.2.0 |
| Melon Flanger | `Flanger` | `Flg1` | `https://github.com/melon-audio/melon-flanger` | 1.2.0 |
| Melon Guitar Synth | `GuitarSynth` | `Gsy1` | `https://github.com/melon-audio/melon-guitarsynth` | 1.2.0 |
| Melon Dirt | `Dirt` | `Drt1` | `https://github.com/melon-audio/melon-dirt` | 1.2.0 |
| Melon Muffs | `Muffs` | `Muf1` | `https://github.com/melon-audio/melon-muffs` | 1.2.0 |
| Melon Gates of Doom | `GatesOfDoom` | `God1` | `https://github.com/melon-audio/melon-gatesofdoom` | 1.2.0 |
| Melon Reverberare | `Reverberare` | `Rvb1` | `https://github.com/melon-audio/melon-reverberare` | 1.2.0 |
| Melon Delay 2290 | `Delay2290` | `D290` | `https://github.com/melon-audio/melon-delay2290` | 1.2.0 |
| Melon Rack | `Rack` | `Rck1` | `https://github.com/melon-audio/melon-rack` | 1.2.0 |
| Melon Comp | `Comp` | `Cmp1` | `https://github.com/melon-audio/melon-comp` | 1.2.0 |
| Melon Pro EQ | `ProEQ` | `Peq1` | `https://github.com/melon-audio/melon-proeq` | 1.2.0 |
| Melon Shifter | `Shifter` | `Shf1` | `https://github.com/melon-audio/melon-shifter` | 1.2.0 |
| Melon Tuner | `Tuner` | `Tun1` | `https://github.com/melon-audio/melon-tuner` | 1.2.0 |
| Melon Simple Gate | `SimpleGate` | `Sgt1` | `https://github.com/melon-audio/melon-simplegate` | 1.2.0 |
| Melon Multi Delay | `MultiDelay` | `Mdl1` | `https://github.com/melon-audio/melon-multidelay` | 1.2.0 |

## Rules

- **Plugin code:** exactly 4 characters, at least one uppercase letter, different from every other code in the table.
- **CMake target:** short, no spaces, unique within the series. It becomes the prefix of the build targets (`<Target>_VST3`, `<Target>_LV2`).
- **Product name:** always starts with "Melon ". It is the name shown in the DAW and used for the `.vst3` and `.lv2` files.
- **LV2 URI:** `<base>/melon-<lowercase-name>`.
- **Once a plugin is released or used in a project, never change** its plugin code, the manufacturer or its URI.
