// MelonLibrary.h - pick a file from a fixed library folder with a parameter (JUCE).
//
// Used by Melon NAM ("model_index") and Melon Cab ("ir_index") so a host without a plugin
// window - the Melon Pedal running mod-host - can choose the model / IR by setting a number:
//
//     patch_set 0 https://github.com/melon-audio/melon-nam:model_index 12
//
//   -1        no file (unloads the current one). Default, so desktop users see no change.
//   0..65535  the file at that position in the library list (MelonLibraryScan.h);
//             past the end of the list = no file, with a message in the status line.
//
// Library folder: $MELON_NAM_DIR / $MELON_IR_DIR if set, else /data/media/NAM and
// /data/media/IR on Linux (the pedal), else Documents/Melon Plugins/Library/NAM|IR.
//
// The parameter may be set from the audio thread (LV2 patch messages), so the listener only
// stores the number and wakes a worker thread, which scans the folder and calls onSelect /
// onNone. A host only sends a change, so to reload after the library changed, set -1 first.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MelonLibraryScan.h"

namespace melon
{
class LibrarySelector : private juce::AudioProcessorValueTreeState::Listener, private juce::Thread
{
public:
    static constexpr int kMaxIndex = 65535;

    static void addParameter (juce::AudioProcessorValueTreeState::ParameterLayout& l, const juce::String& id, const juce::String& name)
    {
        l.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id, 1 }, name, -1, kMaxIndex, -1,
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int v, int) { return v < 0 ? juce::String ("None") : "#" + juce::String (v); })
                .withValueFromStringFunction ([] (const juce::String& s) {
                    return s.trim().equalsIgnoreCase ("none") ? -1 : s.retainCharacters ("-0123456789").getIntValue();
                })));
    }

    // envVar: "MELON_NAM_DIR"; subFolder: "NAM"; exts: { "nam", "aidax", "json" }
    LibrarySelector (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& envVar,
                     const juce::String& subFolder, std::vector<std::string> extensions)
        : juce::Thread ("Melon library " + subFolder), apvts (state), id (paramId), env (envVar), sub (subFolder),
          exts (std::move (extensions))
    {
        if (auto* p = apvts.getRawParameterValue (id))
            requested = applied = juce::roundToInt (p->load());
    }

    ~LibrarySelector() override { stop(); }

    // Called on the worker thread, with the selector's lock held
    std::function<void (const juce::File&)>   onSelect;
    std::function<void (const juce::String&)> onNone; // empty message = deliberate "None"

    void start()
    {
        apvts.addParameterListener (id, this);
        startThread (juce::Thread::Priority::low);
    }

    void stop()
    {
        apvts.removeParameterListener (id, this);
        signalThreadShouldExit();
        notify();
        stopThread (4000);
    }

    // Call right after restoring a saved state: the saved file path wins over a stale index.
    void syncToCurrent()
    {
        const juce::ScopedLock sl (lock);
        applied = requested.load();
    }

    juce::File root() const
    {
        const auto fromEnv = juce::SystemStats::getEnvironmentVariable (env, {});
        if (fromEnv.isNotEmpty() && juce::File::isAbsolutePath (fromEnv))
            return juce::File (fromEnv);
       #if JUCE_LINUX || JUCE_BSD
        return juce::File ("/data/media").getChildFile (sub);
       #else
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Melon Plugins").getChildFile ("Library").getChildFile (sub);
       #endif
    }

private:
    void parameterChanged (const juce::String&, float v) override
    {
        requested = juce::roundToInt (v);
        notify();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            wait (-1);
            if (! threadShouldExit())
                apply();
        }
    }

    void apply()
    {
        const juce::ScopedLock sl (lock);
        const int r = requested.load();
        if (r == applied)
            return;
        applied = r;

        if (r < 0)
        {
            if (onNone)
                onNone ({});
            return;
        }

        const auto dir   = root();
        const auto files = library::scan (library::fromUtf8 (dir.getFullPathName().toStdString()), exts);
        if ((size_t) r >= files.size())
        {
            if (onNone)
                onNone ("No file #" + juce::String (r) + " in " + dir.getFullPathName() + " (" + juce::String ((int) files.size())
                        + " files)");
            return;
        }
        if (onSelect)
            onSelect (dir.getChildFile (juce::String::fromUTF8 (files[(size_t) r].c_str())));
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String                        id, env, sub;
    std::vector<std::string>            exts;
    juce::CriticalSection               lock;
    std::atomic<int>                    requested { -1 };
    int                                 applied = -1; // guarded by lock

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibrarySelector)
};

} // namespace melon
