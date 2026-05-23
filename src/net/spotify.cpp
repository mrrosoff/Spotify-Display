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

constexpr const char *TOKEN_URL = "https://accounts.spotify.com/api/token";
constexpr const char *NOW_PLAYING_URL =
    "https://api.spotify.com/v1/me/player/currently-playing";

// State guarded by mtx — accessed from the polling thread only today, but
// keeping the discipline mirrors Muni's cache pattern.
mutex mtx;
string client_id;
string client_secret;
string refresh_token;
string access_token;
double access_token_expires_at = 0.0;  // unix seconds

string env_or(const char *name, string_view fallback) {
    const char *v = getenv(name);
    return (v && *v) ? string(v) : string(fallback);
}

string url_encode(const string &s) {
    string out;
    out.reserve(s.size() * 3);
    static const char hex[] = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

string base64_encode(const string &in) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) | c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(tbl[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(tbl[((val << 8) >> (bits + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

// Caller must NOT hold mtx.
bool refresh_access_token() {
    string id, secret, rt;
    {
        lock_guard lg(mtx);
        id = client_id;
        secret = client_secret;
        rt = refresh_token;
    }
    if (rt.empty()) {
        log("spotify: no refresh_token available");
        return false;
    }
    const string body = "grant_type=refresh_token&refresh_token=" + url_encode(rt);
    const string basic = "Basic " + base64_encode(id + ":" + secret);
    const vector<string> headers = {
        "Content-Type: application/x-www-form-urlencoded",
        "Authorization: " + basic,
    };
    string resp, err;
    long code = 0;
    const double t0 = tu::monotonic();
    if (!http::post(
            TOKEN_URL,
            headers,
            body,
            cfg::CONNECT_TIMEOUT_S,
            cfg::READ_TIMEOUT_S,
            &resp,
            &code,
            &err
        )) {
        log("spotify token refresh FAILED in ", tu::monotonic() - t0, "s: ", err);
        return false;
    }
    try {
        const auto j = json::parse(resp);
        const auto at = j.at("access_token").get<string>();
        const int expires_in = j.value("expires_in", 3600);
        // Spotify *may* rotate the refresh token; if so, persist it in memory.
        string new_rt;
        if (auto it = j.find("refresh_token"); it != j.end() && it->is_string()) {
            new_rt = it->get<string>();
        }
        {
            lock_guard lg(mtx);
            access_token = at;
            access_token_expires_at =
                static_cast<double>(tu::now_unix()) + expires_in;
            if (!new_rt.empty()) refresh_token = new_rt;
        }
        log("spotify token refreshed in ", tu::monotonic() - t0, "s (expires_in=",
            expires_in, "s)");
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
    }
    if (!refresh_access_token()) return {};
    lock_guard lg(mtx);
    return access_token;
}

}  // namespace

bool init() {
    const string id = env_or("SPOTIFY_CLIENT_ID", cfg::DEFAULT_CLIENT_ID);
    const string secret = env_or("SPOTIFY_CLIENT_SECRET", cfg::DEFAULT_CLIENT_SECRET);
    const string rt = env_or("SPOTIFY_REFRESH_TOKEN", "");
    if (rt.empty()) {
        log("spotify: SPOTIFY_REFRESH_TOKEN unset — run scripts/setup.sh");
    }
    {
        lock_guard lg(mtx);
        client_id = id;
        client_secret = secret;
        refresh_token = rt;
    }
    return !rt.empty();
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
    if (!http::get_bearer(
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
