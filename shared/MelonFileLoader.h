// MelonFileLoader.h - the file loader menu used by Melon NAM ("NAM File Loader") and
// Melon Cab ("IR File Loader") (JUCE).
//
//   ┌ NAM FILE LOADER ───────────────────────────────────────────────┐
//   │ [<] [ loaded file name                              v ] [>]    │
//   │ NAM model, trained at 48 kHz                                    │
//   └─────────────────────────────────────────────────────────────────┘
//
// Clicking the name opens the menu: Load File..., the other files in the same folder,
// recent files, Show in Folder and Unload. < and > step through the files in the folder.
// A file can also be dropped on the panel.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace melon
{
// What a plugin reports about its loaded file
struct LoadedFile
{
    juce::File   file;           // empty = nothing loaded
    juce::String info;           // e.g. "NAM model, trained at 48 kHz" or the error
    bool         loading = false;
    bool         error   = false;
};

//==============================================================================
// Recently loaded files, kept in Documents/Melon Plugins/Recent/<plugin>.txt (one path per line).
class RecentFiles
{
public:
    static constexpr int kMax = 10;

    explicit RecentFiles (const juce::String& pluginName) : plugin (pluginName) {}

    juce::File listFile() const
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Melon Plugins")
            .getChildFile ("Recent")
            .getChildFile (juce::File::createLegalFileName (plugin) + ".txt");
    }

    juce::Array<juce::File> get() const
    {
        juce::Array<juce::File> files;
        juce::StringArray lines;
        lines.addLines (listFile().loadFileAsString());
        for (auto& l : lines)
        {
            const auto path = l.trim();
            if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
                files.addIfNotAlreadyThere (juce::File (path));
        }
        return files;
    }

    void add (const juce::File& f)
    {
        if (! f.existsAsFile())
            return;
        auto files = get();
        files.removeFirstMatchingValue (f);
        files.insert (0, f);
        juce::StringArray lines;
        for (int i = 0; i < juce::jmin (kMax, files.size()); ++i)
            lines.add (files[i].getFullPathName());
        const auto lf = listFile();
        lf.getParentDirectory().createDirectory();
        lf.replaceWithText (lines.joinIntoString ("\n"));
    }

    void clear() { listFile().deleteFile(); }

private:
    juce::String plugin;
};

//==============================================================================
class FileLoader : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    // title: "NAM File Loader"; kind: "NAM" (used in "Load NAM File..."); wildcard: "*.nam"
    FileLoader (const juce::String& titleText, const juce::String& kindText, const juce::String& wc,
                const juce::String& emptyName, const juce::String& pluginName)
        : title (titleText), kind (kindText), wildcard (wc), emptyText (emptyName), recent (pluginName)
    {
        nameButton.setTooltip (title + ": click to load a file, pick another file from the same folder or a recent file");
        prev.setTooltip ("Previous file in the same folder");
        next.setTooltip ("Next file in the same folder");
        nameButton.onClick = [this] { showMenu(); };
        prev.onClick       = [this] { step (-1); };
        next.onClick       = [this] { step (+1); };
        for (auto* b : { &prev, &nameButton, &next })
            addAndMakeVisible (b);

        info.setFont (juce::FontOptions (12.0f));
        info.setColour (juce::Label::textColourId, juce::Colour (dimText));
        info.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (info);
    }

    std::function<void (const juce::File&)> onLoad;
    std::function<void()>                   onClear;
    std::function<juce::File()>             startFolder; // where Load File... opens when nothing is loaded

    // Called from the editor's timer
    void setStatus (const LoadedFile& st)
    {
        current = st.file;
        const bool has  = st.file != juce::File();
        const auto name = has ? st.file.getFileNameWithoutExtension() : emptyText;
        if (nameButton.getButtonText() != name)
            nameButton.setButtonText (name);
        nameButton.setColour (juce::TextButton::textColourOffId, juce::Colour (has ? textColour : dimText));

        const auto text = st.loading ? juce::String ("Loading...")
                                     : (st.info.isNotEmpty() ? st.info : (has ? juce::String() : "Drop a " + kind + " file here, or click above to load one"));
        if (info.getText() != text)
            info.setText (text, juce::dontSendNotification);
        info.setColour (juce::Label::textColourId, juce::Colour (st.error ? errorColour : dimText));

        prev.setEnabled (has);
        next.setEnabled (has);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (panelColour));
        g.fillRoundedRectangle (b, 6.0f);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (juce::Colour (accentColour));
        g.drawText (title.toUpperCase(), b.reduced (12.0f, 4.0f).withHeight (14.0f).toNearestInt(), juce::Justification::left);

        if (dragOver)
        {
            g.setColour (juce::Colour (accentColour).withAlpha (0.12f));
            g.fillRoundedRectangle (b, 6.0f);
            g.setColour (juce::Colour (accentColour));
            g.drawRoundedRectangle (b.reduced (0.75f), 6.0f, 1.5f);
        }
    }

    // Small "menu" arrow at the right of the name button
    void paintOverChildren (juce::Graphics& g) override
    {
        const auto b = nameButton.getBounds().toFloat();
        const float cx = b.getRight() - 14.0f, cy = b.getCentreY();
        juce::Path arrow;
        arrow.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.0f);
        g.setColour (juce::Colour (accentColour).withAlpha (nameButton.isEnabled() ? 1.0f : 0.4f));
        g.fillPath (arrow);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 6).withTrimmedTop (16);
        auto row = r.removeFromTop (juce::jmin (30, r.getHeight()));
        prev.setBounds (row.removeFromLeft (30));
        row.removeFromLeft (4);
        next.setBounds (row.removeFromRight (30));
        row.removeFromRight (4);
        nameButton.setBounds (row);
        r.removeFromTop (4);
        info.setBounds (r.removeFromTop (18));
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size() == 1 && matches (juce::File (files[0]));
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        repaint();
        if (files.size() == 1)
            load (juce::File (files[0]));
    }

private:
    // Series palette (same values as melon::colours, repeated so this header stands alone)
    static constexpr juce::uint32 panelColour = 0xff1d2128, textColour = 0xffc9d1d9, dimText = 0xff7d8590,
                                  accentColour = 0xff4fc3f7, errorColour = 0xffffa657;
    static constexpr int kMaxFolderItems = 200;

    enum MenuIds { idLoad = 1, idUnload, idShow, idClearRecent, idFolder = 1000, idRecent = 5000 };

    bool matches (const juce::File& f) const
    {
        for (auto pattern : juce::StringArray::fromTokens (wildcard, ";", {}))
            if (f.getFileName().matchesWildcard (pattern.trim(), true))
                return true;
        return false;
    }

    juce::Array<juce::File> folderFiles() const
    {
        if (! current.existsAsFile())
            return {};
        auto files = current.getParentDirectory().findChildFiles (juce::File::findFiles, false, wildcard);
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) {
            return a.getFileName().compareNatural (b.getFileName()) < 0;
        });
        return files;
    }

    void load (const juce::File& f)
    {
        if (! f.existsAsFile() || ! onLoad)
            return;
        recent.add (f);
        onLoad (f);
    }

    void showMenu()
    {
        const auto inFolder = folderFiles();
        const auto recents  = recent.get();
        const bool has      = current != juce::File();

        juce::PopupMenu m;
        m.addSectionHeader (title);
        m.addItem (idLoad, "Load " + kind + " File...");

        juce::PopupMenu folderMenu;
        for (int i = 0; i < juce::jmin (kMaxFolderItems, inFolder.size()); ++i)
            folderMenu.addItem (idFolder + i, inFolder[i].getFileNameWithoutExtension(), true, inFolder[i] == current);
        m.addSubMenu ("Files in This Folder", folderMenu, inFolder.size() > 0);

        juce::PopupMenu recentMenu;
        for (int i = 0; i < recents.size(); ++i)
            recentMenu.addItem (idRecent + i, recents[i].getFileNameWithoutExtension()
                                                  + "   (" + recents[i].getParentDirectory().getFileName() + ")",
                                true, recents[i] == current);
        if (recents.size() > 0)
        {
            recentMenu.addSeparator();
            recentMenu.addItem (idClearRecent, "Clear Recent Files");
        }
        m.addSubMenu ("Recent Files", recentMenu, recents.size() > 0);

        m.addSeparator();
        m.addItem (idShow, "Show in Folder", current.existsAsFile());
        m.addItem (idUnload, "Unload \"" + current.getFileNameWithoutExtension() + "\"", has);

        juce::Component::SafePointer<FileLoader> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton).withMinimumWidth (nameButton.getWidth()),
                         [safe, inFolder, recents] (int r) {
                             if (safe != nullptr)
                                 safe->handleMenu (r, inFolder, recents);
                         });
    }

    void handleMenu (int r, const juce::Array<juce::File>& inFolder, const juce::Array<juce::File>& recents)
    {
        if (r == idLoad)
            choose();
        else if (r == idUnload)
        {
            if (onClear)
                onClear();
        }
        else if (r == idShow)
            current.revealToUser();
        else if (r == idClearRecent)
            recent.clear();
        else if (r >= idRecent && r - idRecent < recents.size())
            load (recents[r - idRecent]);
        else if (r >= idFolder && r - idFolder < inFolder.size())
            load (inFolder[r - idFolder]);
    }

    void choose()
    {
        const auto folder = current.existsAsFile() ? current.getParentDirectory()
                                                   : (startFolder ? startFolder() : juce::File());
        chooser = std::make_unique<juce::FileChooser> ("Load " + kind + " file", folder, wildcard);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<FileLoader> (this)] (const juce::FileChooser& fc) {
                                  if (safe != nullptr && fc.getResult().existsAsFile())
                                      safe->load (fc.getResult());
                              });
    }

    void step (int delta)
    {
        const auto files = folderFiles();
        if (files.isEmpty())
            return;
        int idx = files.indexOf (current);
        idx     = idx < 0 ? 0 : (idx + delta + files.size()) % files.size();
        load (files[idx]);
    }

    juce::String     title, kind, wildcard, emptyText;
    RecentFiles      recent;
    juce::TextButton prev { "<" }, nameButton, next { ">" };
    juce::Label      info;
    juce::File       current;
    bool             dragOver = false;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FileLoader)
};

} // namespace melon
