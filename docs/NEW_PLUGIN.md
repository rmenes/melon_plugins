# Creating a new Melon plugin

The steps below use a compressor called **Melon Compressor** as an example.

## 1. Choose the identifiers

Check [`PLUGIN_REGISTRY.md`](PLUGIN_REGISTRY.md) and pick values that are not taken yet:

| Item | Example |
|---|---|
| Folder and CMake target | `Compressor` |
| Product name | `Melon Compressor` |
| Plugin code | `Cmp1` |
| LV2 URI | `https://github.com/melon-audio/melon-compressor` |

Add these values to `PLUGIN_REGISTRY.md` before you start.

## 2. Copy the structure of an existing plugin

Copy `plugins/GraphicEQ` to `plugins/Compressor` and delete everything specific to the EQ (`Source/EqDsp.h`, `tests/`, `docs/`).
Every plugin has this structure:

```
plugins/Compressor/
├── CMakeLists.txt
├── README.md
├── Source/
│   ├── PluginProcessor.h / .cpp   ← parameters and audio processing
│   ├── PluginEditor.h / .cpp      ← user interface
│   ├── FactoryPresets.h           ← the plugin's 10 factory presets
│   └── (DSP in plain C++, without JUCE, so it can be tested on its own)
└── tests/
```

## 3. Edit the plugin's CMakeLists.txt

Change the project name, the target, `PRODUCT_NAME`, `PLUGIN_CODE`, `VST3_CATEGORIES` and `LV2URI`:

```cmake
project(MelonCompressor VERSION 1.0.0 LANGUAGES C CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/MelonCommon.cmake)

juce_add_plugin(Compressor
    COMPANY_NAME              "${MELON_COMPANY_NAME}"
    PLUGIN_MANUFACTURER_CODE  ${MELON_MANUFACTURER_CODE}
    PRODUCT_NAME              "Melon Compressor"
    ICON_BIG                  "${MELON_BRANDING_DIR}/png/melon-icon-512.png"
    ICON_SMALL                "${MELON_BRANDING_DIR}/png/melon-icon-128.png"
    PLUGIN_CODE               Cmp1
    FORMATS                   ${MELON_FORMATS}
    VST3_CATEGORIES           Fx Dynamics
    VST3_AUTO_MANIFEST        ${MELON_VST3_MANIFEST}
    LV2URI                    "${MELON_LV2_URI_BASE}/melon-compressor")

target_sources(Compressor PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Compressor PRIVATE Source)
melon_configure_plugin(Compressor)
```

## 4. Add the plugin to the series

In the root `CMakeLists.txt`, add:

```cmake
add_subdirectory(plugins/Compressor)
```

Then update the plugin table in the root `README.md`.

## 5. Build and test

```powershell
cmake -B build -A x64
cmake --build build --config Release --target Compressor_VST3
```

## Series visual style

To keep every plugin looking the same:
- **Background:** `#15181d`. **Panels:** `#1d2128`. **Grid:** `#2c323b`.
- **Text:** `#c9d1d9`. **Secondary text:** `#7d8590`.
- **Accent:** `#4fc3f7` (blue). **Cut/negative:** `#ffa657` (orange).
- **Header:** the Melon logo at the top-left (40 px tall), a thin divider, then the plugin name without "Melon" (e.g. `Compressor`) in bold, 20 px, text colour. Load the logo with:

  ```cpp
  #include <MelonBranding.h>
  logo = juce::ImageCache::getFromMemory (MelonBranding::melonlogoondarkheader_png,
                                          MelonBranding::melonlogoondarkheader_pngSize);
  ```

  `melon_configure_plugin()` links the `MelonBranding` library, so the header is available in every plugin. See `paint()` and `resized()` in `plugins/GraphicEQ/Source/PluginEditor.cpp`.
- **Common header controls:** Bypass and output gain.
- **Language:** all interface text, parameter names and documentation in English.

## Shared code (`shared/`)

The newer plugins (Dirt, Muffs, Gates of Doom, Reverberare, Delay 2290) are built on the code in `shared/`, which `melon_configure_plugin()` adds to the include path. Using it, a new effect only needs its parameters, its DSP and its display:

- `MelonProcessorBase.h` — `melon::ProcessorBase`: buses (mono, stereo, mono → stereo), state saving, the Bypass parameter, and parameter helpers (`melon::params::addPercent`, `addFloat`, `addDb`, `addChoice`, `addBool`, `addOutputAndBypass`). Override `prepareDsp()` and `processDsp()`.
- `MelonEditorBase.h` — the series palette (`melon::colours`), the look-and-feel and `melon::EditorBase<Processor>`, which draws the standard header (logo, title, Bypass, Output) and lays out an optional display, a selector row (`addChoiceBox`, `addToggle`, `addRowLabel`, `makeButton` + `addRowItem`) and the knob panel (`addKnob`). Finish the constructor with `finishSetup (width, height, minWidth, minHeight)`.
- `MelonPresets.h` — the preset system shared by every plugin: `melon::PresetManager` (factory presets, up to 64 user presets, host program list, preset name saved with the project) and `melon::PresetBar` (the preset row under the header). `ProcessorBase` and `EditorBase` include it automatically; a plugin only calls `presets.setFactoryPresets (myns::factoryPresets());` in its processor constructor. See "Presets" below.
- `MelonDsp.h` — plain C++ building blocks: `Smoother`, `Biquad`, `OnePole`, `DelayLine` (Hermite interpolation), `Oversampler4x`, `Noise`.
- `TestSignals.h` — test signals (sine, plucked guitar chords) and measurements for the DSP tests.

Compile a DSP test with `g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp`.

## Presets

Every plugin has a preset row under its header: `Preset [<] [name] [>]   [Save] [Save As]`. Clicking the name opens the menu (factory presets, user presets, Save, Save As, Delete, Reset to Default, Open Preset Folder). A `*` after the name means the settings were changed since the preset was loaded or saved.

- **Factory presets** live in `Source/FactoryPresets.h`: a list of `{ "Name", { { "paramId", value }, ... } }` in real units (Hz, ms, %, choice index). Parameters that are not listed take their default value. "Default" is always added as the first preset. Factory presets are also the host's program list (VST3 program list, LV2 presets).
- **User presets** are XML files (`.melonpreset`) in `Documents/Melon Plugins/Presets/<Plugin name>/`, up to 64 per plugin. They store every parameter except Bypass, in real units, so they keep working if a parameter range changes later and can be shared between Windows, Linux and Mac.
- The current preset name and its "changed" state are saved with the DAW project.
- Plugins with extra state that is not a parameter (Melon Rack's model and IR files) fill `presets.writeExtras` / `presets.readExtras`. Factory presets never call `readExtras`, so they keep what is loaded.
- For a plugin that does not use `ProcessorBase`, add a `melon::PresetManager presets { *this, JucePlugin_Name };` member after the APVTS, forward the four program functions to it, and call `presets.writeState()` / `extractState()` / `restoreState()` in `getStateInformation()` / `setStateInformation()` (see Melon Chorus). Add a `melon::PresetBar` to the editor.
