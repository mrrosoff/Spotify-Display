#pragma once

#include <mutex>

struct WeatherData {
    int code = 0;
    int hi = 0;
    int lo = 0;
    int precip = 0;
};

struct WeatherCache {
    bool have = false;
    WeatherData data;
    double last_fetch = 0.0;  // unix seconds
    int day_index = -1;
    int consecutive_failures = 0;
};

namespace caches {

// Weather is written by the fetcher thread and read by the render thread.
inline std::mutex mtx;
inline WeatherCache weather;

}  // namespace caches
