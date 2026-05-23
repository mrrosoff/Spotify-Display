#pragma once

#include <string>

namespace spotify {

// Initialize from environment. Reads SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET,
// and (required) SPOTIFY_REFRESH_TOKEN. Returns true if a refresh token was
// present.
bool init();

struct NowPlaying {
    bool playing = false;        // true when something is currently playing
    std::string album_art_url;   // empty when no art available
    std::string track_name;      // for logging
};

// Poll current playback. On a transient/network error, returns false and
// fills `error`; the caller should treat that as "unchanged" rather than
// "nothing playing".
bool current_playback(NowPlaying *out, std::string *error);

}  // namespace spotify
