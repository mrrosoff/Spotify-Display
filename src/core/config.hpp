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

// HTTP
inline constexpr long CONNECT_TIMEOUT_S = 5;
inline constexpr long READ_TIMEOUT_S = 10;

// Fail threshold before showing "Error" red on weather screen.
inline constexpr int FAIL_THRESHOLD = 5;

// Boot grace before matrix init (keeps SSH responsive on fresh boot).
inline constexpr auto BOOT_GRACE = std::chrono::seconds{60};

// After matrix init + loading screen, wait this long before the first network
// call. Gives WiFi / DHCP / DNS room to settle even when boot grace was
// already past on launch.
inline constexpr auto NETWORK_GRACE = std::chrono::seconds{30};

// Default Spotify app credentials. Override at runtime with
// SPOTIFY_CLIENT_ID / SPOTIFY_CLIENT_SECRET.
inline constexpr std::string_view DEFAULT_CLIENT_ID =
    "a9a84f65fc9f47568870f4c0c0185e3a";
inline constexpr std::string_view DEFAULT_CLIENT_SECRET =
    "7cb7fe064e1844c19e87a2d475573948";

}  // namespace cfg
