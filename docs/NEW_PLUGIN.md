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
- `MelonDsp.h` — plain C++ building blocks: `Smoother`, `Biquad`, `OnePole`, `DelayLine` (Hermite interpolation), `Oversampler4x`, `Noise`.
- `TestSignals.h` — test signals (sine, plucked guitar chords) and measurements for the DSP tests.

Compile a DSP test with `g++ -std=c++17 -O2 -ISource -I../../shared tests/test_dsp.cpp -o test_dsp`.
