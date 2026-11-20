// Tone3000.h - Melon Rack's TONE3000 API client (https://www.tone3000.com/api).
//
// - Sign-in uses the OAuth device flow: the plugin shows a short code, the user confirms it
//   on tone3000.com/activate in their browser. No password ever passes through the plugin.
// - Tokens are kept in <user app data>/Melon Plugins/tone3000-session.json and refreshed
//   automatically. "Sign out" deletes them.
// - Files are only downloaded when the user asks for them (a button click), as the API terms
//   require, into Documents/Melon Plugins/TONE3000/<tone>/.
// - The app's publishable key comes from MELON_TONE3000_CLIENT_ID (cmake/MelonCommon.cmake).
//   Without it, the plugin still shows the suggestions and opens them on tone3000.com.
//
// All callbacks run on the message thread, and never after the Client is destroyed.
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <memory>
#include <vector>

namespace t3k
{
struct Model
{
    juce::String id, name, url;
};

struct Tone
{
    int          id = 0;
    juce::String title, format, creator, pageUrl;
    std::vector<Model> models;

    bool isIr() const { return format.containsIgnoreCase ("ir"); }
};

class Client
{
public:
    Client();
    ~Client();

    static bool         isConfigured();       // a publishable key was compiled in
    static juce::File   downloadFolder();     // Documents/Melon Plugins/TONE3000
    static juce::String catalogUrl() { return "https://www.tone3000.com"; }

    bool         isSignedIn() const;
    juce::String getUserName() const;
    bool         isSigningIn() const { return signingIn.load(); }

    // Device sign-in. onCode receives the code to show and the page to open.
    void signIn (std::function<void (const juce::String& userCode, const juce::String& verifyUri, const juce::String& verifyUriComplete)> onCode,
                 std::function<void (bool ok, const juce::String& message)> onDone);
    void cancelSignIn();
    void signOut();

    // Tone details and its list of models (captures / IRs)
    void fetchTone (int toneId, std::function<void (juce::Result, const Tone&)> done);

    // Downloads one model of a tone (skipped if it was downloaded before) and returns the file
    void download (const Tone& tone, const Model& model, std::function<void (juce::Result, const juce::File&)> done);

private:
    struct Session
    {
        juce::String accessToken, refreshToken, userName;
        juce::int64  expiresAtMs = 0;
    };

    juce::String accessToken (bool forceRefresh = false); // worker thread; empty = not signed in
    void         storeTokens (const juce::var& json);
    void         loadSession();
    void         saveSession();
    void         fetchUserName();

    template <typename F>
    void post (F&& f)
    {
        auto a = alive;
        juce::MessageManager::callAsync ([a, fn = std::forward<F> (f)]() mutable {
            if (a->load())
                fn();
        });
    }

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<int>                   signInGeneration { 0 };
    std::atomic<bool>                  signingIn { false };

    mutable juce::CriticalSection lock; // session
    Session                       session;

    juce::ThreadPool pool { 1 }; // declared last: destroyed first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Client)
};
} // namespace t3k
