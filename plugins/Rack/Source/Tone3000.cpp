#include "Tone3000.h"

#ifndef MELON_TONE3000_CLIENT_ID
 #define MELON_TONE3000_CLIENT_ID ""
#endif

namespace t3k
{
namespace
{
const juce::String kApi = "https://www.tone3000.com/api/v1";

juce::String clientId() { return juce::String (MELON_TONE3000_CLIENT_ID).trim(); }

struct Response
{
    bool         connected = false;
    int          status    = 0;
    juce::String body;
    juce::var    json;

    bool ok() const { return connected && status >= 200 && status < 300; }
    juce::String error() const { return json.getProperty ("error", {}).toString(); }
};

juce::String headersFor (bool form, const juce::String& bearer)
{
    juce::String h;
    h << "Accept: application/json\r\n"
      << "User-Agent: MelonRack/" << JucePlugin_VersionString << "\r\n";
    if (form)
        h << "Content-Type: application/x-www-form-urlencoded\r\n";
    if (bearer.isNotEmpty())
        h << "Authorization: Bearer " << bearer << "\r\n";
    return h;
}

Response send (const juce::URL& url, bool post, const juce::String& bearer)
{
    int  status = 0;
    auto opts   = juce::URL::InputStreamOptions (post ? juce::URL::ParameterHandling::inPostData
                                                      : juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders (headersFor (post, bearer))
                    .withConnectionTimeoutMs (15000)
                    .withNumRedirectsToFollow (5)
                    .withStatusCode (&status);
    Response r;
    if (auto stream = url.createInputStream (opts))
    {
        r.connected = true;
        r.body      = stream->readEntireStreamAsString();
        r.json      = juce::JSON::parse (r.body);
    }
    r.status = status;
    return r;
}

juce::String describe (const Response& r)
{
    if (! r.connected)
        return "Could not reach TONE3000. Check your internet connection.";
    if (r.status == 401 || r.status == 403)
        return "TONE3000 refused access (" + juce::String (r.status) + "). Try signing in again.";
    if (r.status == 429)
        return "TONE3000 is busy (too many requests). Please try again in a minute.";
    auto msg = r.json.getProperty ("error_description", r.json.getProperty ("message", {})).toString();
    if (msg.isEmpty())
        msg = r.error();
    return "TONE3000 error " + juce::String (r.status) + (msg.isNotEmpty() ? ": " + msg : juce::String());
}

juce::File sessionFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Melon Plugins")
        .getChildFile ("tone3000-session.json");
}

bool hasModelExtension (const juce::String& name)
{
    for (auto* ext : { ".nam", ".wav", ".json", ".aidax", ".aif", ".aiff", ".flac" })
        if (name.endsWithIgnoreCase (ext))
            return true;
    return false;
}
} // namespace

//==============================================================================
Client::Client() { loadSession(); }

Client::~Client()
{
    alive->store (false);
    ++signInGeneration;
    pool.removeAllJobs (true, 20000);
}

bool Client::isConfigured() { return clientId().isNotEmpty(); }

juce::File Client::downloadFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Melon Plugins")
        .getChildFile ("TONE3000");
}

bool Client::isSignedIn() const
{
    const juce::ScopedLock sl (lock);
    return session.refreshToken.isNotEmpty() || session.accessToken.isNotEmpty();
}

juce::String Client::getUserName() const
{
    const juce::ScopedLock sl (lock);
    return session.userName;
}

//==============================================================================
void Client::loadSession()
{
    const auto json = juce::JSON::parse (sessionFile());
    const juce::ScopedLock sl (lock);
    session.accessToken  = json.getProperty ("access_token", {}).toString();
    session.refreshToken = json.getProperty ("refresh_token", {}).toString();
    session.userName     = json.getProperty ("username", {}).toString();
    session.expiresAtMs  = (juce::int64) json.getProperty ("expires_at_ms", 0);
}

void Client::saveSession()
{
    auto* obj = new juce::DynamicObject();
    {
        const juce::ScopedLock sl (lock);
        obj->setProperty ("access_token", session.accessToken);
        obj->setProperty ("refresh_token", session.refreshToken);
        obj->setProperty ("username", session.userName);
        obj->setProperty ("expires_at_ms", session.expiresAtMs);
    }
    const auto file = sessionFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (obj)));
}

void Client::storeTokens (const juce::var& json)
{
    const juce::ScopedLock sl (lock);
    session.accessToken = json.getProperty ("access_token", {}).toString();
    if (const auto r = json.getProperty ("refresh_token", {}).toString(); r.isNotEmpty())
        session.refreshToken = r; // keep the old one if the server does not rotate it
    const int expiresIn = (int) json.getProperty ("expires_in", 3600);
    session.expiresAtMs = juce::Time::currentTimeMillis() + (juce::int64) expiresIn * 1000;
}

juce::String Client::accessToken (bool forceRefresh)
{
    juce::String refresh;
    {
        const juce::ScopedLock sl (lock);
        if (! forceRefresh && session.accessToken.isNotEmpty()
            && juce::Time::currentTimeMillis() < session.expiresAtMs - 60000)
            return session.accessToken;
        refresh = session.refreshToken;
    }
    if (refresh.isEmpty() || ! isConfigured())
        return {};

    const auto r = send (juce::URL (kApi + "/oauth/token")
                             .withParameter ("grant_type", "refresh_token")
                             .withParameter ("refresh_token", refresh)
                             .withParameter ("client_id", clientId()),
                         true, {});
    if (r.ok() && r.json.getProperty ("access_token", {}).toString().isNotEmpty())
    {
        storeTokens (r.json);
        saveSession();
        const juce::ScopedLock sl (lock);
        return session.accessToken;
    }
    if (r.connected && r.status == 400 && r.error() == "invalid_grant")
    {
        {
            const juce::ScopedLock sl (lock);
            session = {};
        }
        saveSession(); // expired: the user has to sign in again
    }
    return {};
}

void Client::fetchUserName()
{
    const auto token = accessToken();
    const auto r     = send (juce::URL (kApi + "/user"), false, token);
    if (r.ok())
    {
        const juce::ScopedLock sl (lock);
        session.userName = r.json.getProperty ("username", r.json.getProperty ("name", {})).toString();
    }
}

//==============================================================================
void Client::signIn (std::function<void (const juce::String&, const juce::String&, const juce::String&)> onCode,
                     std::function<void (bool, const juce::String&)> onDone)
{
    if (! isConfigured())
    {
        post ([onDone] { onDone (false, "TONE3000 sign-in is not set up in this build (no publishable key)."); });
        return;
    }

    const int generation = ++signInGeneration;
    signingIn            = true;

    pool.addJob ([this, generation, onCode, onDone] {
        auto finish = [this, generation, onDone] (bool ok, const juce::String& message) {
            if (generation == signInGeneration.load())
                signingIn = false;
            post ([onDone, ok, message] { onDone (ok, message); });
        };

        const auto start = send (juce::URL (kApi + "/oauth/device_authorization")
                                     .withParameter ("client_id", clientId())
                                     .withParameter ("scope", "read"),
                                 true, {});
        if (! start.ok())
            return finish (false, describe (start));

        const auto deviceCode = start.json.getProperty ("device_code", {}).toString();
        const auto userCode   = start.json.getProperty ("user_code", {}).toString();
        const auto uri        = start.json.getProperty ("verification_uri", "https://www.tone3000.com/activate").toString();
        const auto uriFull    = start.json.getProperty ("verification_uri_complete", uri).toString();
        int        interval   = juce::jmax (1, (int) start.json.getProperty ("interval", 5));
        const auto deadline   = juce::Time::currentTimeMillis() + 1000 * (juce::int64) (int) start.json.getProperty ("expires_in", 600);

        if (deviceCode.isEmpty() || userCode.isEmpty())
            return finish (false, "TONE3000 sent an unexpected reply. Please try again later.");

        post ([onCode, userCode, uri, uriFull] { onCode (userCode, uri, uriFull); });

        while (juce::Time::currentTimeMillis() < deadline)
        {
            for (int i = 0; i < interval * 10; ++i) // wait, but react quickly to Cancel / plugin closing
            {
                if (generation != signInGeneration.load() || ! alive->load())
                    return; // cancelled: no callback
                juce::Thread::sleep (100);
            }

            const auto r = send (juce::URL (kApi + "/oauth/token")
                                     .withParameter ("grant_type", "urn:ietf:params:oauth:grant-type:device_code")
                                     .withParameter ("device_code", deviceCode)
                                     .withParameter ("client_id", clientId()),
                                 true, {});
            if (r.ok() && r.json.getProperty ("access_token", {}).toString().isNotEmpty())
            {
                storeTokens (r.json);
                fetchUserName();
                saveSession();
                const auto name = getUserName();
                return finish (true, name.isNotEmpty() ? "Signed in to TONE3000 as " + name : "Signed in to TONE3000");
            }

            const auto err = r.error();
            if (err == "authorization_pending")
                continue;
            if (err == "slow_down")
            {
                interval += 5;
                continue;
            }
            if (err == "access_denied")
                return finish (false, "The sign-in was declined on TONE3000.");
            if (err == "expired_token")
                break;
            if (! r.connected)
                continue; // a network hiccup: keep trying until the code expires
            return finish (false, describe (r));
        }
        finish (false, "The sign-in code expired. Please try again.");
    });
}

void Client::cancelSignIn()
{
    ++signInGeneration;
    signingIn = false;
}

void Client::signOut()
{
    cancelSignIn();
    {
        const juce::ScopedLock sl (lock);
        session = {};
    }
    sessionFile().deleteFile();
}

//==============================================================================
void Client::fetchTone (int toneId, std::function<void (juce::Result, const Tone&)> done)
{
    pool.addJob ([this, toneId, done] {
        Tone tone;
        tone.id      = toneId;
        tone.pageUrl = catalogUrl() + "/tones/" + juce::String (toneId);

        auto fail = [this, done, &tone] (const juce::String& msg) {
            post ([done, msg, tone] { done (juce::Result::fail (msg), tone); });
        };

        auto token = accessToken();
        if (token.isEmpty())
            return fail ("Sign in to TONE3000 first.");

        auto info = send (juce::URL (kApi + "/tones/" + juce::String (toneId)), false, token);
        if (info.connected && info.status == 401) // token revoked or expired early: refresh once
        {
            token = accessToken (true);
            if (token.isEmpty())
                return fail ("Your TONE3000 session expired. Please sign in again.");
            info = send (juce::URL (kApi + "/tones/" + juce::String (toneId)), false, token);
        }
        if (! info.ok())
            return fail (describe (info));

        tone.title   = info.json.getProperty ("title", {}).toString();
        tone.format  = info.json.getProperty ("format", {}).toString();
        tone.creator = info.json.getProperty ("user", {}).getProperty ("username", {}).toString();
        if (const auto u = info.json.getProperty ("url", {}).toString(); u.startsWith ("http"))
            tone.pageUrl = u;

        const auto models = send (juce::URL (kApi + "/models")
                                      .withParameter ("tone_id", juce::String (toneId))
                                      .withParameter ("page", "1")
                                      .withParameter ("page_size", "100"),
                                  false, token);
        if (! models.ok())
            return fail (describe (models));

        if (auto* list = models.json.getProperty ("data", {}).getArray())
            for (const auto& m : *list)
            {
                Model model;
                model.id   = m.getProperty ("id", {}).toString();
                model.name = m.getProperty ("name", {}).toString();
                model.url  = m.getProperty ("model_url", {}).toString();
                if (model.url.isNotEmpty())
                    tone.models.push_back (model);
            }

        if (tone.models.empty())
            return fail ("This tone has no files you can download.");

        post ([done, tone] { done (juce::Result::ok(), tone); });
    });
}

void Client::download (const Tone& tone, const Model& model, std::function<void (juce::Result, const juce::File&)> done)
{
    pool.addJob ([this, tone, model, done] {
        auto fail = [this, done] (const juce::String& msg) {
            post ([done, msg] { done (juce::Result::fail (msg), juce::File()); });
        };

        // Documents/Melon Plugins/TONE3000/<id> <title>/<model name>.<ext>
        const auto folder = downloadFolder().getChildFile (
            juce::File::createLegalFileName (juce::String (tone.id) + " " + tone.title).trim());
        auto name = juce::File::createLegalFileName (model.name.isNotEmpty() ? model.name : "model " + model.id).trim();
        if (! hasModelExtension (name))
        {
            const auto fromUrl = juce::URL (model.url).getFileName();
            name += hasModelExtension (fromUrl) ? fromUrl.fromLastOccurrenceOf (".", true, false)
                                                : juce::String (tone.isIr() ? ".wav" : ".nam");
        }
        const auto file = folder.getChildFile (name);

        if (file.existsAsFile() && file.getSize() > 0) // downloaded before
        {
            post ([done, file] { done (juce::Result::ok(), file); });
            return;
        }

        const auto token = accessToken();
        if (token.isEmpty())
            return fail ("Sign in to TONE3000 first.");

        int  status = 0;
        auto stream = juce::URL (model.url).createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders (headersFor (false, token))
                .withConnectionTimeoutMs (15000)
                .withNumRedirectsToFollow (5)
                .withStatusCode (&status));
        if (stream == nullptr)
            return fail ("Could not reach TONE3000. Check your internet connection.");
        if (status < 200 || status >= 300)
            return fail ("TONE3000 could not send the file (error " + juce::String (status) + ").");

        if (! folder.createDirectory())
            return fail ("Could not create the folder " + folder.getFullPathName());

        const auto temp = file.getSiblingFile (file.getFileName() + ".part");
        temp.deleteFile(); // FileOutputStream appends to an existing file
        bool written = false;
        {
            juce::FileOutputStream out (temp);
            written = out.openedOk() && out.writeFromInputStream (*stream, -1) > 0;
        } // closed here, so it can be moved or deleted
        if (! written)
        {
            temp.deleteFile();
            return fail ("Could not save " + file.getFullPathName());
        }
        if (! temp.moveFileTo (file))
        {
            temp.deleteFile();
            return fail ("Could not save " + file.getFullPathName());
        }

        post ([done, file] { done (juce::Result::ok(), file); });
    });
}
} // namespace t3k
