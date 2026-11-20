// MelonPresets.h - the series' preset system (JUCE).
//
// Every Melon plugin gets the same preset row under its header:
//
//   Preset [<] [ Preset name *            ] [>]               [Save] [Save As]
//
// - Factory presets: "Default" + the plugin's own list (FactoryPresets.h in each plugin).
//   They are built into the binary, read-only, and are also offered to the host as programs.
// - User presets: up to 64 per plugin, saved as small XML files (.melonpreset) in
//     Documents/Melon Plugins/Presets/<Plugin name>/
//   so they survive reinstalls and can be copied between computers and operating systems.
// - A preset stores every parameter except Bypass, in real units (Hz, ms, %...), so presets
//   keep working if a parameter range changes in a later version. Parameters missing from a
//   preset (e.g. added later) go to their default value.
// - "*" after the name means the sound was changed since the preset was loaded or saved.
//   The current preset name (and that state) is saved with the DAW project.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

namespace melon
{
//==============================================================================
struct FactoryPreset
{
    juce::String name;
    // Real (not normalised) values, e.g. { "rate", 0.8f }, { "model", 2 }, { "infinite", 1 }.
    // Anything not listed takes its default value.
    std::vector<std::pair<juce::String, float>> values;
};

//==============================================================================
class PresetManager
{
public:
    static constexpr int         kMaxUserPresets = 64;
    static constexpr const char* kExtension      = ".melonpreset";
    static constexpr const char* kFileTag        = "MelonPreset";
    static constexpr const char* kStateTag       = "MELON_PRESET";

    // Call after the processor's parameters exist (i.e. declare it after the APVTS).
    PresetManager (juce::AudioProcessor& p, juce::String pluginName) : processor (p), plugin (std::move (pluginName))
    {
        for (auto* prm : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (r->getParameterID() != "bypass")
                {
                    index[r->getParameterID()] = (int) params.size();
                    params.push_back (r);
                }

        factory.push_back ({ "Default", {} });
        current.name = "Default";
        snapshot     = defaultValues();
    }

    void setFactoryPresets (std::vector<FactoryPreset> list)
    {
        factory.resize (1);
        for (auto& f : list)
        {
           #if JUCE_DEBUG
            for (auto& v : f.values)
                jassert (index.count (v.first) == 1); // unknown parameter ID in a factory preset
           #endif
            factory.push_back (std::move (f));
        }
    }

    //==========================================================================
    struct Current
    {
        juce::String name;
        int          factoryIndex = 0; // -1 = user preset
        juce::File   file;             // user preset file
    };

    Current getCurrent() const
    {
        const juce::ScopedLock sl (lock);
        return current;
    }

    bool isUserPreset() const { return getCurrent().factoryIndex < 0; }

    // True when a parameter moved since the preset was loaded or saved
    bool isModified() const
    {
        const juce::ScopedLock sl (lock);
        for (size_t i = 0; i < params.size(); ++i)
            if (std::abs (params[i]->getValue() - snapshot[i]) > 1.0e-4f)
                return true;
        return false;
    }

    int getChangeCount() const { return changes.load(); }

    int                 getNumFactoryPresets() const { return (int) factory.size(); }
    const juce::String& getFactoryName (int i) const { return factory[(size_t) juce::jlimit (0, (int) factory.size() - 1, i)].name; }

    //==========================================================================
    juce::File getFolder() const
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Melon Plugins")
            .getChildFile ("Presets")
            .getChildFile (plugin);
    }

    // User presets, sorted by name
    juce::Array<juce::File> getUserPresets() const
    {
        auto files = getFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) {
            return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
        });
        return files;
    }

    juce::File fileForName (const juce::String& name) const
    {
        return getFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + kExtension);
    }

    //==========================================================================
    void loadFactory (int i, bool notifyHost = true)
    {
        if (! juce::isPositiveAndBelow (i, (int) factory.size()))
            return;

        std::map<juce::String, float> values;
        for (auto& v : factory[(size_t) i].values)
            values[v.first] = v.second;
        apply (values);
        setCurrent ({ factory[(size_t) i].name, i, {} }, notifyHost);
    }

    bool loadUser (const juce::File& file)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr || ! xml->hasTagName (kFileTag))
            return false;
        const auto owner = xml->getStringAttribute ("plugin");
        if (owner.isNotEmpty() && owner != plugin)
            return false; // a preset from another plugin of the series

        std::map<juce::String, float> values;
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            values[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");
        apply (values);

        if (readExtras)
            readExtras (*xml);

        setCurrent ({ file.getFileNameWithoutExtension(), -1, file }, true);
        return true;
    }

    // Saves (or overwrites) a user preset with the current settings
    juce::Result saveUser (const juce::String& name)
    {
        const auto legal = juce::File::createLegalFileName (name.trim());
        if (legal.isEmpty())
            return juce::Result::fail ("Please enter a name for the preset.");

        const auto file = fileForName (legal);
        if (! file.existsAsFile() && getUserPresets().size() >= kMaxUserPresets)
            return juce::Result::fail ("You already have " + juce::String (kMaxUserPresets)
                                       + " user presets. Delete one before saving a new one.");

        if (auto r = getFolder().createDirectory(); r.failed())
            return juce::Result::fail ("Could not create the preset folder:\n" + getFolder().getFullPathName());

        juce::XmlElement xml (kFileTag);
        xml.setAttribute ("plugin", plugin);
        xml.setAttribute ("name", legal);
        xml.setAttribute ("version", 1);
        for (auto* p : params)
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", p->getParameterID());
            e->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
        }
        if (writeExtras)
            writeExtras (xml);

        if (! xml.writeTo (file))
            return juce::Result::fail ("Could not write the preset file:\n" + file.getFullPathName());

        {
            const juce::ScopedLock sl (lock);
            snapshot = currentValues();
        }
        setCurrent ({ legal, -1, file }, true);
        return juce::Result::ok();
    }

    bool deleteUser (const juce::File& file)
    {
        const bool ok = file.moveToTrash() || file.deleteFile();
        if (ok)
        {
            const juce::ScopedLock sl (lock);
            if (current.file == file)
                current.file = juce::File(); // keep the sound and the name, it is just not on disk any more
            ++changes;
        }
        return ok;
    }

    // Previous / next across factory and user presets (wraps around)
    void step (int delta)
    {
        const auto users = getUserPresets();
        const int  nf = (int) factory.size(), n = nf + users.size();
        const auto c  = getCurrent();

        int pos = c.factoryIndex >= 0 ? c.factoryIndex : -1;
        if (pos < 0)
            if (const int u = users.indexOf (c.file); u >= 0)
                pos = nf + u;

        const int next = pos < 0 ? (delta > 0 ? 0 : n - 1) : (pos + delta + n) % n;
        if (next < nf)
            loadFactory (next);
        else
            loadUser (users[next - nf]);
    }

    //==========================================================================
    // Host program list = the factory presets
    int getNumPrograms() const { return (int) factory.size(); }
    int getCurrentProgram() const { return juce::jmax (0, getCurrent().factoryIndex); }
    void setCurrentProgram (int i)
    {
        // Ignore the host echoing back the program we already report (keeps user presets intact)
        if (i != getCurrentProgram())
            loadFactory (i, false);
    }
    juce::String getProgramName (int i) const
    {
        return juce::isPositiveAndBelow (i, (int) factory.size()) ? factory[(size_t) i].name : juce::String();
    }

    //==========================================================================
    // Session state: call writeState() on the XML in getStateInformation, and
    // extractState() before / restoreState() after apvts.replaceState() in setStateInformation.
    void writeState (juce::XmlElement& stateXml) const
    {
        const juce::ScopedLock sl (lock);
        auto* e = stateXml.createNewChildElement (kStateTag);
        e->setAttribute ("name", current.name);
        e->setAttribute ("factory", current.factoryIndex);
        e->setAttribute ("file", current.file.getFullPathName());
        for (size_t i = 0; i < params.size(); ++i)
        {
            auto* v = e->createNewChildElement ("V");
            v->setAttribute ("id", params[i]->getParameterID());
            v->setAttribute ("v", (double) snapshot[i]);
        }
    }

    std::unique_ptr<juce::XmlElement> extractState (juce::XmlElement& stateXml) const
    {
        if (auto* e = stateXml.getChildByName (kStateTag))
        {
            auto copy = std::make_unique<juce::XmlElement> (*e);
            stateXml.removeChildElement (e, true);
            return copy;
        }
        return nullptr;
    }

    void restoreState (const juce::XmlElement* info)
    {
        const juce::ScopedLock sl (lock);
        if (info == nullptr) // project saved before presets existed
        {
            current  = { "Default", 0, {} };
            snapshot = defaultValues();
        }
        else
        {
            const auto path = info->getStringAttribute ("file");
            current.name         = info->getStringAttribute ("name", "Default");
            current.factoryIndex = juce::jlimit (-1, (int) factory.size() - 1, info->getIntAttribute ("factory", 0));
            current.file         = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();

            snapshot = currentValues();
            for (auto* v : info->getChildWithTagNameIterator ("V"))
                if (auto it = index.find (v->getStringAttribute ("id")); it != index.end())
                    snapshot[(size_t) it->second] = (float) v->getDoubleAttribute ("v");
        }
        ++changes;
    }

    // Optional: extra, non-parameter data for user presets (e.g. Melon Rack's model files).
    // Factory presets never call readExtras, so they keep whatever is loaded.
    std::function<void (juce::XmlElement&)>       writeExtras;
    std::function<void (const juce::XmlElement&)> readExtras;

private:
    std::vector<float> currentValues() const
    {
        std::vector<float> v;
        for (auto* p : params)
            v.push_back (p->getValue());
        return v;
    }

    std::vector<float> defaultValues() const
    {
        std::vector<float> v;
        for (auto* p : params)
            v.push_back (p->getDefaultValue());
        return v;
    }

    void apply (const std::map<juce::String, float>& values)
    {
        for (auto* p : params)
        {
            float norm = p->getDefaultValue();
            if (auto it = values.find (p->getParameterID()); it != values.end())
                norm = p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (it->second));
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
        const juce::ScopedLock sl (lock);
        snapshot = currentValues();
    }

    void setCurrent (Current c, bool notifyHost)
    {
        {
            const juce::ScopedLock sl (lock);
            current = std::move (c);
            ++changes;
        }
        if (notifyHost)
            processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    }

    juce::AudioProcessor&                     processor;
    const juce::String                        plugin;
    std::vector<juce::RangedAudioParameter*>  params;
    std::map<juce::String, int>               index;
    std::vector<FactoryPreset>                factory;

    juce::CriticalSection lock; // current + snapshot (hosts may call program functions from any thread)
    Current               current;
    std::vector<float>    snapshot;
    std::atomic<int>      changes { 0 };

    JUCE_DECLARE_NON_COPYABLE (PresetManager)
};

//==============================================================================
// Small modal panel drawn inside the plugin window (safer than OS dialog windows in hosts).
class PresetDialog : public juce::Component
{
public:
    std::function<void (bool ok, const juce::String& text)> onFinish;

    PresetDialog (const juce::String& title, const juce::String& message, bool withTextField,
                  const juce::String& initialText, const juce::String& okText, bool withCancel)
        : hasText (withTextField), hasCancel (withCancel)
    {
        titleLabel.setText (title, juce::dontSendNotification);
        titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc9d1d9));
        addAndMakeVisible (titleLabel);

        messageLabel.setText (message, juce::dontSendNotification);
        messageLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc9d1d9));
        messageLabel.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (messageLabel);

        if (hasText)
        {
            textField.setText (initialText, false);
            textField.setInputRestrictions (60);
            textField.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff15181d));
            textField.setColour (juce::TextEditor::textColourId, juce::Colour (0xffc9d1d9));
            textField.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff2c323b));
            textField.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff4fc3f7));
            textField.onReturnKey = [this] { finish (true); };
            textField.onEscapeKey = [this] { finish (false); };
            addAndMakeVisible (textField);
        }

        okButton.setButtonText (okText);
        okButton.onClick = [this] { finish (true); };
        addAndMakeVisible (okButton);

        cancelButton.onClick = [this] { finish (false); };
        addChildComponent (cancelButton);
        cancelButton.setVisible (hasCancel);
    }

    void focusTextField()
    {
        if (hasText)
        {
            textField.grabKeyboardFocus();
            textField.selectAll();
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        g.setColour (juce::Colour (0xff1d2128));
        g.fillRoundedRectangle (panel.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff4fc3f7).withAlpha (0.6f));
        g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 8.0f, 1.0f);
    }

    void resized() override
    {
        panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (400, getWidth() - 40), hasText ? 170 : 150);
        auto r = panel.reduced (16, 12);
        titleLabel.setBounds (r.removeFromTop (24));
        auto buttons = r.removeFromBottom (28);
        okButton.setBounds (buttons.removeFromRight (90));
        if (hasCancel)
        {
            buttons.removeFromRight (8);
            cancelButton.setBounds (buttons.removeFromRight (90));
        }
        r.removeFromBottom (10);
        if (hasText)
            textField.setBounds (r.removeFromBottom (28));
        messageLabel.setBounds (r);
    }

    void parentSizeChanged() override
    {
        if (auto* p = getParentComponent())
            setBounds (p->getLocalBounds());
    }

    void mouseDown (const juce::MouseEvent&) override {} // block clicks to the plugin behind

private:
    void finish (bool ok)
    {
        if (done)
            return;
        done = true;
        setVisible (false);
        if (onFinish)
            onFinish (ok, textField.getText());
    }

    const bool           hasText, hasCancel;
    bool                 done = false;
    juce::Rectangle<int> panel;
    juce::Label          titleLabel, messageLabel;
    juce::TextEditor     textField;
    juce::TextButton     okButton, cancelButton { "Cancel" };
};

//==============================================================================
// The preset row: Preset [<] [name] [>]   [Save] [Save As]
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PresetManager& m) : manager (m)
    {
        label.setText ("Preset", juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredRight);
        label.setColour (juce::Label::textColourId, juce::Colour (0xff7d8590));
        addAndMakeVisible (label);

        prevButton.setTooltip ("Previous preset");
        nextButton.setTooltip ("Next preset");
        nameButton.setTooltip ("Choose, save or delete presets");
        saveButton.setTooltip ("Save the changes to this user preset (factory presets are saved as a new user preset)");
        saveAsButton.setTooltip ("Save the current settings as a new user preset");

        prevButton.onClick   = [this] { manager.step (-1); refresh(); };
        nextButton.onClick   = [this] { manager.step (+1); refresh(); };
        nameButton.onClick   = [this] { showMenu(); };
        saveButton.onClick   = [this] { save(); };
        saveAsButton.onClick = [this] { saveAs(); };

        for (auto* b : { &prevButton, &nextButton, &nameButton, &saveButton, &saveAsButton })
            addAndMakeVisible (b);

        refresh();
        startTimerHz (8);
    }

    ~PresetBar() override { stopTimer(); }

    void resized() override
    {
        auto r = getLocalBounds();
        saveAsButton.setBounds (r.removeFromRight (78));
        r.removeFromRight (6);
        saveButton.setBounds (r.removeFromRight (64));
        r.removeFromRight (16);

        label.setBounds (r.removeFromLeft (52));
        r.removeFromLeft (6);
        prevButton.setBounds (r.removeFromLeft (28));
        r.removeFromLeft (4);
        auto nameArea = r.removeFromLeft (juce::jmin (360, r.getWidth() - 32));
        nameButton.setBounds (nameArea);
        r.removeFromLeft (4);
        nextButton.setBounds (r.removeFromLeft (28));
    }

private:
    enum MenuIds { idSave = 1, idSaveAs, idDelete, idDefault, idFolder, idFactory = 100, idUser = 10000 };

    void timerCallback() override { refresh(); }

    void refresh()
    {
        const auto c    = manager.getCurrent();
        const auto text = c.name + (manager.isModified() ? " *" : "");
        if (nameButton.getButtonText() != text)
            nameButton.setButtonText (text);
    }

    void showMenu()
    {
        const auto c     = manager.getCurrent();
        const auto users = manager.getUserPresets();

        juce::PopupMenu m;
        m.addSectionHeader ("Factory");
        for (int i = 0; i < manager.getNumFactoryPresets(); ++i)
            m.addItem (idFactory + i, manager.getFactoryName (i), true, c.factoryIndex == i);

        m.addSectionHeader ("User (" + juce::String (users.size()) + "/" + juce::String (PresetManager::kMaxUserPresets) + ")");
        if (users.isEmpty())
            m.addItem (idUser - 1, "No user presets yet", false);
        for (int i = 0; i < users.size(); ++i)
            m.addItem (idUser + i, users[i].getFileNameWithoutExtension(), true, c.factoryIndex < 0 && c.file == users[i]);

        m.addSeparator();
        m.addItem (idSave, "Save");
        m.addItem (idSaveAs, "Save As...");
        m.addItem (idDelete, "Delete \"" + c.name + "\"", c.factoryIndex < 0 && c.file.existsAsFile());
        m.addItem (idDefault, "Reset to Default");
        m.addItem (idFolder, "Open Preset Folder");

        juce::Component::SafePointer<PresetBar> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton).withMinimumWidth (nameButton.getWidth()),
                         [safe, users] (int r) {
                             if (safe != nullptr)
                                 safe->handleMenu (r, users);
                         });
    }

    void handleMenu (int r, const juce::Array<juce::File>& users)
    {
        if (r >= idUser && r - idUser < users.size())
        {
            if (! manager.loadUser (users[r - idUser]))
                message ("Could not load preset", "The file could not be read:\n" + users[r - idUser].getFullPathName());
        }
        else if (r >= idFactory && r < idUser)
            manager.loadFactory (r - idFactory);
        else if (r == idSave)    save();
        else if (r == idSaveAs)  saveAs();
        else if (r == idDelete)  deleteCurrent();
        else if (r == idDefault) manager.loadFactory (0);
        else if (r == idFolder)
        {
            manager.getFolder().createDirectory();
            manager.getFolder().startAsProcess();
        }
        refresh();
    }

    void save()
    {
        const auto c = manager.getCurrent();
        if (c.factoryIndex < 0 && c.file.existsAsFile())
            store (c.name);
        else
            saveAs();
    }

    void saveAs()
    {
        const auto c = manager.getCurrent();
        ask ("Save Preset", "Name of the user preset:", true, c.name, "Save", true, [this] (bool ok, const juce::String& name) {
            if (! ok || name.trim().isEmpty())
                return;
            if (manager.fileForName (name).existsAsFile())
                ask ("Replace Preset", "A user preset called \"" + juce::File::createLegalFileName (name.trim()) + "\" already exists. Replace it?",
                     false, {}, "Replace", true, [this, name] (bool yes, const juce::String&) {
                         if (yes)
                             store (name);
                     });
            else
                store (name);
        });
    }

    void store (const juce::String& name)
    {
        if (auto r = manager.saveUser (name); r.failed())
            message ("Could not save preset", r.getErrorMessage());
        refresh();
    }

    void deleteCurrent()
    {
        const auto c = manager.getCurrent();
        if (c.factoryIndex >= 0 || ! c.file.existsAsFile())
            return;
        ask ("Delete Preset", "Delete the user preset \"" + c.name + "\"?", false, {}, "Delete", true,
             [this, file = c.file] (bool ok, const juce::String&) {
                 if (ok && ! manager.deleteUser (file))
                     message ("Could not delete preset", file.getFullPathName());
                 refresh();
             });
    }

    void message (const juce::String& title, const juce::String& text)
    {
        ask (title, text, false, {}, "OK", false, nullptr);
    }

    void ask (const juce::String& title, const juce::String& text, bool withTextField, const juce::String& initial,
              const juce::String& okText, bool withCancel, std::function<void (bool, const juce::String&)> callback)
    {
        juce::Component* host = findParentComponentOfClass<juce::AudioProcessorEditor>();
        if (host == nullptr)
            host = getTopLevelComponent();

        dialog = std::make_unique<PresetDialog> (title, text, withTextField, initial, okText, withCancel);
        juce::Component::SafePointer<PresetBar> safe (this);
        dialog->onFinish = [safe, callback] (bool ok, const juce::String& t) {
            // Run after the click handler returns: the dialog can then be deleted safely,
            // and the callback may open the next dialog.
            juce::MessageManager::callAsync ([safe, callback, ok, t] {
                if (safe == nullptr)
                    return;
                safe->dialog.reset();
                if (callback)
                    callback (ok, t);
            });
        };
        host->addAndMakeVisible (*dialog);
        dialog->setBounds (host->getLocalBounds());
        dialog->toFront (true);
        dialog->focusTextField();
    }

    PresetManager&                manager;
    juce::Label                   label;
    juce::TextButton              prevButton { "<" }, nextButton { ">" }, nameButton, saveButton { "Save" }, saveAsButton { "Save As" };
    std::unique_ptr<PresetDialog> dialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

} // namespace melon
