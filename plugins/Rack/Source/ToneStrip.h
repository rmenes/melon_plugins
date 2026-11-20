// ToneStrip.h - Melon Rack's TONE3000 row, under the rack:
//
//   [TONE3000]  Suggested:  [Pedal: ...][...] [Amp: ...][...]   [Load suggested tones] [Browse TONE3000] [Sign in]
//               status line / hint
//
// Clicking a suggestion loads it into its slot (signing in first if needed). Its "..." button
// opens a menu: choose another capture of the same tone, or view it on the website. Credits (creator names) are shown
// in the tooltips and menus, as TONE3000 asks.
#pragma once

#include <MelonEditorBase.h>
#include "PluginProcessor.h"

//==============================================================================
// Sign-in panel drawn over the plugin window: partnership splash, then the device code.
class Tone3000SignIn : public juce::Component
{
public:
    Tone3000SignIn (RackProcessor&, std::function<void (bool signedIn)> onClose);
    ~Tone3000SignIn() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentSizeChanged() override;
    void mouseDown (const juce::MouseEvent&) override {} // block clicks to the plugin behind

private:
    enum class Stage { intro, waitingForCode, code, done };
    void start();
    void close (bool signedIn);
    void setStage (Stage s);

    RackProcessor&                     processor;
    std::function<void (bool)>         onClose;
    Stage                              stage = Stage::intro;
    bool                               success = false;
    juce::String                       code, pageUrl, pageUrlComplete, message;
    juce::Rectangle<int>               panel;
    juce::Label                        title, body, codeLabel;
    juce::TextButton                   primary, secondary { "Cancel" };
};

//==============================================================================
class ToneStrip : public juce::Component
{
public:
    explicit ToneStrip (RackProcessor&);
    ~ToneStrip() override;

    void refresh(); // from the editor's timer
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildChips();
    void showToneMenu (size_t index);
    void loadOne (size_t index);
    void setLocalStatus (const juce::String& text);
    void chooseCapture (const rack::ToneSuggestion& s);
    void showAccountMenu();
    void openSignIn (std::function<void()> afterSignIn);
    void loadSuggested();

    RackProcessor& processor;

    juce::Image                                    logo;
    juce::Rectangle<int>                           badge;
    juce::Label                                    heading, status;
    std::vector<std::unique_ptr<juce::TextButton>> chips, chipMenus;
    juce::TextButton loadAll { "Load suggested tones" }, browse { "Browse TONE3000" }, account;

    const std::vector<rack::ToneSuggestion>* suggestions = nullptr;
    juce::String                             shownPreset;
    int                                      shownStatus = -1;
    bool                                     localStatus = false; // a message from the strip itself is shown
    std::unique_ptr<Tone3000SignIn>          signInPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneStrip)
};
