#include "ToneStrip.h"

#if MELON_HAS_T3K_LOGO
 #include <Tone3000Logo.h>
#endif

using namespace melon;

namespace
{
void openInBrowser (const juce::String& url) { juce::URL (url).launchInDefaultBrowser(); }

juce::String byCreator (const juce::String& creator) { return creator.isNotEmpty() ? " by " + creator : juce::String(); }
} // namespace

//==============================================================================
Tone3000SignIn::Tone3000SignIn (RackProcessor& p, std::function<void (bool)> closeCallback)
    : processor (p), onClose (std::move (closeCallback))
{
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, colours::text);
    title.setText ("Melon Rack + TONE3000", juce::dontSendNotification);
    addAndMakeVisible (title);

    body.setColour (juce::Label::textColourId, colours::text);
    body.setJustificationType (juce::Justification::topLeft);
    body.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (body);

    codeLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 30.0f, juce::Font::bold));
    codeLabel.setColour (juce::Label::textColourId, colours::accent);
    codeLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (codeLabel);

    primary.onClick = [this] {
        if (stage == Stage::intro)
            start();
        else if (stage == Stage::code)
            openInBrowser (pageUrlComplete.isNotEmpty() ? pageUrlComplete : pageUrl);
        else if (stage == Stage::done)
            close (success);
    };
    secondary.onClick = [this] {
        processor.tone3000.cancelSignIn();
        close (false);
    };
    addAndMakeVisible (primary);
    addAndMakeVisible (secondary);

    setStage (Stage::intro);
}

Tone3000SignIn::~Tone3000SignIn()
{
    if (stage == Stage::waitingForCode || stage == Stage::code)
        processor.tone3000.cancelSignIn();
}

void Tone3000SignIn::setStage (Stage s)
{
    stage = s;
    codeLabel.setVisible (s == Stage::code);
    primary.setVisible (s != Stage::waitingForCode);
    secondary.setVisible (s != Stage::done);

    switch (s)
    {
        case Stage::intro:
            body.setText ("Sign in with your free TONE3000 account to load amp models, pedals and cabinet IRs "
                          "from the TONE3000 library straight into Melon Rack.\n\n"
                          "Your password never goes through Melon Rack: you confirm the sign-in on tone3000.com. "
                          "Files are only downloaded when you click Load, into Documents/Melon Plugins/TONE3000.",
                          juce::dontSendNotification);
            primary.setButtonText ("Continue");
            break;
        case Stage::waitingForCode:
            body.setText ("Asking TONE3000 for a sign-in code...", juce::dontSendNotification);
            break;
        case Stage::code:
            body.setText ("1. Open " + pageUrl.fromFirstOccurrenceOf ("://", false, false)
                              + " in your browser (it should have opened already).\n"
                              "2. Sign in and enter this code. This window updates by itself when you are done.",
                          juce::dontSendNotification);
            codeLabel.setText (code, juce::dontSendNotification);
            primary.setButtonText ("Open the page again");
            break;
        case Stage::done:
            body.setText (message, juce::dontSendNotification);
            primary.setButtonText ("OK");
            break;
    }
    resized();
    repaint();
}

void Tone3000SignIn::start()
{
    setStage (Stage::waitingForCode);
    juce::Component::SafePointer<Tone3000SignIn> safe (this);
    processor.tone3000.signIn (
        [safe] (const juce::String& userCode, const juce::String& uri, const juce::String& uriComplete) {
            if (safe == nullptr)
                return;
            safe->code            = userCode;
            safe->pageUrl         = uri;
            safe->pageUrlComplete = uriComplete;
            safe->setStage (Stage::code);
            openInBrowser (uriComplete.isNotEmpty() ? uriComplete : uri);
        },
        [safe] (bool ok, const juce::String& msg) {
            if (safe == nullptr)
                return;
            safe->success = ok;
            safe->message = msg;
            safe->setStage (Stage::done);
        });
}

void Tone3000SignIn::close (bool signedIn)
{
    // The owner deletes this panel: do it after the click handler has returned
    auto cb = onClose;
    juce::MessageManager::callAsync ([cb, signedIn] {
        if (cb)
            cb (signedIn);
    });
}

void Tone3000SignIn::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));
    g.setColour (colours::panel);
    g.fillRoundedRectangle (panel.toFloat(), 8.0f);
    g.setColour (colours::accent.withAlpha (0.6f));
    g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 8.0f, 1.0f);
}

void Tone3000SignIn::resized()
{
    panel  = getLocalBounds().withSizeKeepingCentre (juce::jmin (480, getWidth() - 40), stage == Stage::code ? 270 : 230);
    auto r = panel.reduced (18, 14);
    title.setBounds (r.removeFromTop (28));
    auto buttons = r.removeFromBottom (30);
    primary.setBounds (buttons.removeFromRight (160));
    buttons.removeFromRight (8);
    secondary.setBounds (buttons.removeFromRight (90));
    r.removeFromBottom (10);
    if (stage == Stage::code)
        codeLabel.setBounds (r.removeFromBottom (48));
    body.setBounds (r);
}

void Tone3000SignIn::parentSizeChanged()
{
    if (auto* p = getParentComponent())
        setBounds (p->getLocalBounds());
}

//==============================================================================
ToneStrip::ToneStrip (RackProcessor& p) : processor (p)
{
#if MELON_HAS_T3K_LOGO
    logo = juce::ImageCache::getFromMemory (Tone3000Logo::tone3000logo_png, Tone3000Logo::tone3000logo_pngSize);
#endif

    heading.setColour (juce::Label::textColourId, colours::dimText);
    heading.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (heading);

    status.setFont (juce::FontOptions (12.0f));
    status.setColour (juce::Label::textColourId, colours::dimText);
    status.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (status);

    loadAll.setTooltip ("Download this preset's suggested tones from TONE3000 and load them. "
                        "Slots without a suggestion are emptied.");
    browse.setTooltip ("Open the TONE3000 library in your browser");

    loadAll.onClick = [this] { loadSuggested(); };
    browse.onClick  = [] { openInBrowser (t3k::Client::catalogUrl()); };
    account.onClick = [this] { showAccountMenu(); };

    for (auto* b : { &loadAll, &browse, &account })
        addAndMakeVisible (b);

    refresh();
}

ToneStrip::~ToneStrip() = default;

void ToneStrip::refresh()
{
    const auto c       = processor.presets.getCurrent();
    const auto* tones  = c.factoryIndex >= 0 ? rack::tonesForPreset (c.name) : nullptr;
    if (tones != suggestions || c.name != shownPreset)
    {
        suggestions = tones;
        shownPreset = c.name;
        rebuildChips();
    }

    if (processor.toneStatusCounter() != shownStatus)
    {
        shownStatus = processor.toneStatusCounter();
        status.setText (processor.toneStatus(), juce::dontSendNotification);
    }

    const bool configured = t3k::Client::isConfigured();
    const bool busy       = processor.isLoadingTones();
    const auto user       = processor.tone3000.getUserName();

    juce::String accountText = "Sign in";
    if (configured && processor.tone3000.isSignedIn())
        accountText = user.isNotEmpty() ? user : "Signed in";
    if (account.getButtonText() != accountText)
        account.setButtonText (accountText);
    account.setTooltip (configured ? "Your TONE3000 account" : "TONE3000 sign-in is not set up in this build");

    loadAll.setEnabled (suggestions != nullptr && configured && ! busy);
    for (auto& chip : chips)
        chip->setEnabled (! busy);
}

void ToneStrip::rebuildChips()
{
    chips.clear();
    if (suggestions != nullptr)
    {
        heading.setText ("Suggested:", juce::dontSendNotification);
        for (size_t i = 0; i < suggestions->size(); ++i)
        {
            const auto& s = (*suggestions)[i];
            auto& chip    = *chips.emplace_back (std::make_unique<juce::TextButton> (juce::String (rack::slotName (s.slot)) + ": " + s.title));
            chip.setTooltip (s.title + byCreator (s.creator) + " on TONE3000 - click to open or load it");
            chip.onClick = [this, i] { showToneMenu (i); };
            addAndMakeVisible (chip);
        }
    }
    else
    {
        heading.setText ("Find amp models, pedals and cabinet IRs on TONE3000. The factory presets suggest some.",
                         juce::dontSendNotification);
    }
    resized();
}

void ToneStrip::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    if (logo.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (logo, badge.toFloat(), juce::RectanglePlacement::centred);
    }
    else // text mark until the official logo is added (see README)
    {
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (badge.toFloat(), 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText ("TONE3000", badge, juce::Justification::centred);
    }
}

void ToneStrip::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    auto row  = area.removeFromTop (28);
    status.setBounds (area.withTrimmedLeft (110));

    badge = row.removeFromLeft (100);
    row.removeFromLeft (10);
    account.setBounds (row.removeFromRight (120));
    row.removeFromRight (6);
    browse.setBounds (row.removeFromRight (130));
    row.removeFromRight (6);
    loadAll.setBounds (row.removeFromRight (160));
    row.removeFromRight (12);

    if (chips.empty())
    {
        heading.setBounds (row);
        return;
    }
    heading.setBounds (row.removeFromLeft (76));
    const int n = (int) chips.size();
    const int w = juce::jmin (280, (row.getWidth() - 6 * (n - 1)) / n);
    for (auto& chip : chips)
    {
        chip->setBounds (row.removeFromLeft (w));
        row.removeFromLeft (6);
    }
}

//==============================================================================
void ToneStrip::showToneMenu (size_t index)
{
    if (suggestions == nullptr || index >= suggestions->size())
        return;
    const auto s          = (*suggestions)[index];
    const bool configured = t3k::Client::isConfigured();
    const bool busy       = processor.isLoadingTones();

    juce::PopupMenu m;
    m.addSectionHeader (s.title + byCreator (s.creator));
    m.addItem (1, "Open on TONE3000");
    m.addItem (2, "Load into " + juce::String (rack::slotName (s.slot)), configured && ! busy);
    m.addItem (3, "Choose another capture...", configured && ! busy);
    if (! configured)
        m.addItem (4, "(Loading needs a TONE3000 key in this build)", false);

    juce::Component::SafePointer<ToneStrip> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (chips[index].get()), [safe, s] (int r) {
        if (safe == nullptr || r == 0)
            return;
        if (r == 1)
            return openInBrowser (s.url);

        auto action = [safe, s, r] {
            if (safe == nullptr)
                return;
            if (r == 2)
                safe->processor.loadTones ({ s }, false);
            else if (r == 3)
                safe->chooseCapture (s);
        };
        if (safe->processor.tone3000.isSignedIn())
            action();
        else
            safe->openSignIn (action);
    });
}

void ToneStrip::chooseCapture (const rack::ToneSuggestion& s)
{
    status.setText ("TONE3000: getting the captures of " + s.title + "...", juce::dontSendNotification);
    juce::Component::SafePointer<ToneStrip> safe (this);
    processor.tone3000.fetchTone (s.toneId, [safe, s] (juce::Result r, const t3k::Tone& tone) {
        if (safe == nullptr)
            return;
        if (r.failed())
            return safe->status.setText (r.getErrorMessage(), juce::dontSendNotification);
        safe->status.setText ({}, juce::dontSendNotification);

        juce::PopupMenu m;
        m.addSectionHeader ((tone.title.isNotEmpty() ? tone.title : s.title) + byCreator (tone.creator.isNotEmpty() ? tone.creator : s.creator));
        for (size_t i = 0; i < tone.models.size(); ++i)
            m.addItem ((int) i + 1, tone.models[i].name);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (safe.getComponent()), [safe, s, tone] (int pick) {
            if (safe != nullptr && pick > 0 && pick <= (int) tone.models.size())
                safe->processor.loadToneModel (s.slot, tone, tone.models[(size_t) pick - 1]);
        });
    });
}

void ToneStrip::loadSuggested()
{
    if (suggestions == nullptr || processor.isLoadingTones())
        return;
    if (! processor.tone3000.isSignedIn())
    {
        juce::Component::SafePointer<ToneStrip> safe (this);
        return openSignIn ([safe] {
            if (safe != nullptr)
                safe->loadSuggested();
        });
    }
    processor.loadTones (*suggestions, true);
}

void ToneStrip::showAccountMenu()
{
    if (! t3k::Client::isConfigured())
    {
        status.setText ("This build of Melon Rack has no TONE3000 key, so it can only open tones in the browser.",
                        juce::dontSendNotification);
        return;
    }
    if (! processor.tone3000.isSignedIn())
        return openSignIn (nullptr);

    juce::PopupMenu m;
    const auto user = processor.tone3000.getUserName();
    m.addSectionHeader (user.isNotEmpty() ? "Signed in to TONE3000 as " + user : "Signed in to TONE3000");
    m.addItem (1, "Open TONE3000");
    m.addItem (2, "Open the download folder");
    m.addSeparator();
    m.addItem (3, "Sign out");

    juce::Component::SafePointer<ToneStrip> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&account), [safe] (int r) {
        if (safe == nullptr)
            return;
        if (r == 1)
            openInBrowser (t3k::Client::catalogUrl());
        else if (r == 2)
        {
            const auto folder = t3k::Client::downloadFolder();
            folder.createDirectory();
            folder.startAsProcess();
        }
        else if (r == 3)
        {
            safe->processor.tone3000.signOut();
            safe->status.setText ("Signed out of TONE3000.", juce::dontSendNotification);
        }
    });
}

void ToneStrip::openSignIn (std::function<void()> afterSignIn)
{
    if (signInPanel != nullptr || ! t3k::Client::isConfigured())
        return;

    juce::Component* host = findParentComponentOfClass<juce::AudioProcessorEditor>();
    if (host == nullptr)
        host = getTopLevelComponent();

    juce::Component::SafePointer<ToneStrip> safe (this);
    signInPanel = std::make_unique<Tone3000SignIn> (processor, [safe, afterSignIn] (bool ok) {
        if (safe == nullptr)
            return;
        safe->signInPanel.reset();
        if (ok && afterSignIn)
            afterSignIn();
    });
    host->addAndMakeVisible (*signInPanel);
    signInPanel->setBounds (host->getLocalBounds());
    signInPanel->toFront (true);
}
