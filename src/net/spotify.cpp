#include "net/spotify.hpp"

#include "core/config.hpp"
#include "core/log.hpp"
#include "net/http.hpp"
#include "util/time_utils.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <mutex>
#include <string>

using namespace std;
using logx::log;
using nlohmann::json;

namespace spotify {

namespace {

constexpr const char *NOW_PLAYING_URL =
    "https://api.spotify.com/v1/me/player/currently-playing";

// State guarded by mtx.
mutex mtx;
string token_url;
string device_token;
string access_token;
double access_token_expires_at = 0.0;  // unix seconds
double next_token_attempt_at = 0.0;    // monotonic seconds; retry cooldown
bool reauth_needed = false;            // broker last returned 246

// Reused across polls to keep TCP + TLS warm. Lazy function-local static so the
// curl handle is created after curl_global_init(), not during static init.
http::Session &session() {
    static http::Session s;
    return s;
}

string env_or(const char *name, string_view fallback) {
    const char *v = getenv(name);
    return (v && *v) ? string(v) : string(fallback);
}

// Caller must NOT hold mtx. Asks the broker for a fresh access token.
bool fetch_access_token() {
    string url, dev;
    {
        lock_guard lg(mtx);
        url = token_url;
        dev = device_token;
    }
    if (dev.empty()) return false;

    string resp, err;
    long code = 0;
    if (!session().get_bearer(
            url, dev, cfg::CONNECT_TIMEOUT_S, cfg::READ_TIMEOUT_S, &resp, &code, &err
        )) {
        if (err.rfind("HTTP ", 0) != 0) session().reset();
        log("spotify token fetch failed: ", err);
        return false;
    }
    // 246 means the broker has no usable token until the owner reconnects via
    // `sudo spotify`; it's a success status (no token in the body), so flag it.
    if (code == 246) {
        lock_guard lg(mtx);
        if (!reauth_needed) log("spotify: reauth required — run `sudo spotify`");
        reauth_needed = true;
        return false;
    }
    try {
        const auto j = json::parse(resp);
        lock_guard lg(mtx);
        access_token = j.at("access_token").get<string>();
        access_token_expires_at =
            static_cast<double>(tu::now_unix()) + j.value("expires_in", 3600);
        reauth_needed = false;
        return true;
    } catch (const exception &e) {
        log("spotify token parse failed: ", e.what());
        return false;
    }
}

// Caller must NOT hold mtx. Returns empty string on failure.
string get_valid_access_token() {
    {
        lock_guard lg(mtx);
        const double slack = static_cast<double>(cfg::TOKEN_REFRESH_SLACK.count());
        if (!access_token.empty() &&
            static_cast<double>(tu::now_unix()) + slack < access_token_expires_at) {
            return access_token;
        }
        if (tu::monotonic() < next_token_attempt_at) return {};
    }
    if (!fetch_access_token()) {
        lock_guard lg(mtx);
        next_token_attempt_at =
            tu::monotonic() + static_cast<double>(cfg::TOKEN_RETRY_COOLDOWN.count());
        return {};
    }
    lock_guard lg(mtx);
    return access_token;
}

}  // namespace

bool init() {
    const string url = env_or("SPOTIFY_TOKEN_URL", cfg::DEFAULT_TOKEN_URL);
    const string dev = env_or("SPOTIFY_DEVICE_TOKEN", "");
    if (dev.empty()) {
        log("spotify: SPOTIFY_DEVICE_TOKEN unset — set it in /etc/spotifydisplay.env");
    }
    {
        lock_guard lg(mtx);
        token_url = url;
        device_token = dev;
    }
    return !dev.empty();
}

bool reauth_required() {
    lock_guard lg(mtx);
    return reauth_needed;
}

bool current_playback(NowPlaying *out, string *error) {
    *out = {};
    const string token = get_valid_access_token();
    if (token.empty()) {
        if (error) *error = "no access token";
        return false;
    }
    string body, err;
    long code = 0;
    if (!session().get_bearer(
            NOW_PLAYING_URL,
            token,
            cfg::CONNECT_TIMEOUT_S,
            cfg::READ_TIMEOUT_S,
            &body,
            &code,
            &err
        )) {
        // 401 means the token expired earlier than we expected — force a
        // refresh on the next call.
        if (code == 401) {
            lock_guard lg(mtx);
            access_token_expires_at = 0.0;
        }
        // Server-status errors leave the connection healthy; anything else
        // is transport, so recycle the pooled curl handle.
        if (err.rfind("HTTP ", 0) != 0) session().reset();
        if (error) *error = err;
        return false;
    }
    if (code == 204 || body.empty()) {
        // Nothing playing — successful, but no track.
        return true;
    }
    try {
        const auto j = json::parse(body);
        const auto item_it = j.find("item");
        if (item_it == j.end() || item_it->is_null()) return true;

        // Album images first; some item types (podcasts) put them on the item.
        const json *images = nullptr;
        if (auto album_it = item_it->find("album");
            album_it != item_it->end() && album_it->is_object()) {
            if (auto im_it = album_it->find("images");
                im_it != album_it->end() && im_it->is_array() && !im_it->empty()) {
                images = &(*im_it);
            }
        }
        if (!images) {
            if (auto im_it = item_it->find("images");
                im_it != item_it->end() && im_it->is_array() && !im_it->empty()) {
                images = &(*im_it);
            }
        }
        out->active = true;
        const bool is_playing = j.value("is_playing", true);
        out->paused = !is_playing;
        if (auto name_it = item_it->find("name");
            name_it != item_it->end() && name_it->is_string()) {
            out->track_name = name_it->get<string>();
        }
        if (images) {
            // Spotify lists largest-first; smallest is last (typically 64×64).
            const auto &smallest = images->back();
            if (auto url_it = smallest.find("url");
                url_it != smallest.end() && url_it->is_string()) {
                out->album_art_url = url_it->get<string>();
            }
        }
        return true;
    } catch (const exception &e) {
        if (error) *error = string("parse: ") + e.what();
        return false;
    }
}

}  // namespace spotify
