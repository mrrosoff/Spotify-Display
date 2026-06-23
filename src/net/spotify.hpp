#pragma once

#include <string>

namespace spotify {

// Initialize from environment. Reads (required) SPOTIFY_DEVICE_TOKEN and
// optional SPOTIFY_TOKEN_URL (the broker endpoint). Returns true if a device
// token was present.
bool init();

// True when the broker last reported that the Spotify account needs to be
// reconnected (HTTP 246). Cleared once a token fetch succeeds again.
bool reauth_required();

struct NowPlaying {
    bool active = false;         // an item is loaded in the player (playing or paused)
    bool paused = false;         // active && !is_playing
    std::string album_art_url;   // empty when no art available
    std::string track_name;      // for logging
};

// Poll current playback. On a transient/network error, returns false and
// fills `error`; the caller should treat that as "unchanged" rather than
// "nothing playing".
bool current_playback(NowPlaying *out, std::string *error);

}  // namespace spotify
