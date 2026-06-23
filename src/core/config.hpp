#pragma once

#include <chrono>
#include <string_view>

namespace cfg {

// Location (San Diego)
inline constexpr double LAT = 32.7992898;
inline constexpr double LON = -117.1922940;

// Spotify polling
inline constexpr auto POLL_INTERVAL = std::chrono::milliseconds{1000};
inline constexpr auto IDLE_TIMEOUT = std::chrono::seconds{30};

// Token refresh: refresh ~5 min before expiry
inline constexpr auto TOKEN_REFRESH_SLACK = std::chrono::seconds{300};

// Weather
inline constexpr auto WEATHER_TTL = std::chrono::seconds{1800};
inline constexpr auto WEATHER_RETRY_TTL = std::chrono::seconds{120};
inline constexpr auto WEATHER_REDRAW = std::chrono::seconds{5};
inline constexpr auto WEATHER_LOOP_INTERVAL = std::chrono::seconds{30};

// Daypart schedule (minutes from midnight, local).
inline constexpr int NIGHT_START_MIN = 22 * 60 + 30;  // 10:30 PM
inline constexpr int NIGHT_END_MIN = 7 * 60;          // 7:00 AM

inline constexpr int DAY_BRIGHTNESS = 100;
inline constexpr int NIGHT_BRIGHTNESS = 40;

// HTTP. The matrix's realtime render thread starves the kernel and
// userspace work on a Pi Zero W, so TLS handshakes and reads that
// normally take ms can stretch into the 10–20s range. These match
// Muni-Display's effective tolerance; tighter values made first-call
// handshakes time out under matrix load.
inline constexpr long CONNECT_TIMEOUT_S = 60;
inline constexpr long READ_TIMEOUT_S = 120;

// Fail threshold before showing "Error" red on weather screen.
inline constexpr int FAIL_THRESHOLD = 5;

// Boot grace before matrix init (keeps SSH responsive on fresh boot).
inline constexpr auto BOOT_GRACE = std::chrono::seconds{60};

// Token broker: the website mints short-lived Spotify access tokens for this
// device. Override the endpoint at runtime with SPOTIFY_TOKEN_URL; the
// per-device secret comes from SPOTIFY_DEVICE_TOKEN (no default).
inline constexpr std::string_view DEFAULT_TOKEN_URL =
    "https://api.maxrosoff.com/spotify/token";

// Cooldown before re-asking the broker after a failed token fetch, so a
// pending reauth (broker 246) doesn't get hammered every poll.
inline constexpr auto TOKEN_RETRY_COOLDOWN = std::chrono::seconds{30};

}  // namespace cfg
