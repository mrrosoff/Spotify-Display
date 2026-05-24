#include "data/fetchers.hpp"

#include "core/config.hpp"
#include "core/log.hpp"
#include "data/caches.hpp"
#include "net/http.hpp"
#include "util/time_utils.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>

using namespace std;
using logx::log;
using nlohmann::json;

namespace fetch {

extern atomic<bool> g_interrupted;  // defined in main.cpp

namespace {

// Persistent session for open-meteo polls. Reused across refreshes so TCP+TLS
// stays warm between weather fetches.
http::Session http_session;

// Caller must hold caches::mtx.
void schedule_retry(double &last_fetch) {
    last_fetch = static_cast<double>(tu::now_unix()) -
                 (cfg::WEATHER_TTL.count() - cfg::WEATHER_RETRY_TTL.count());
}

string forecast_url() {
    char buf[256];
    snprintf(
        buf,
        sizeof(buf),
        "https://api.open-meteo.com/v1/forecast?"
        "latitude=%.7f&longitude=%.7f&"
        "daily=weather_code,temperature_2m_max,temperature_2m_min,"
        "precipitation_probability_max&"
        "temperature_unit=fahrenheit&"
        "timezone=America%%2FLos_Angeles&"
        "forecast_days=2",
        cfg::LAT,
        cfg::LON
    );
    return string(buf);
}

}  // namespace

bool refresh_weather(int day_index) {
    const double t0 = tu::monotonic();
    string body, err;
    if (!http_session.get(
            forecast_url(),
            cfg::CONNECT_TIMEOUT_S,
            cfg::READ_TIMEOUT_S,
            &body,
            &err
        )) {
        lock_guard lg(caches::mtx);
        auto &c = caches::weather;
        c.consecutive_failures++;
        schedule_retry(c.last_fetch);
        log("weather FAILED in ", tu::monotonic() - t0, "s (#",
            c.consecutive_failures, "): ", err);
        return false;
    }
    try {
        const auto data = json::parse(body);
        const auto &d = data.at("daily");
        WeatherData wd;
        wd.code = d.at("weather_code").at(day_index).get<int>();
        wd.hi = static_cast<int>(
            round(d.at("temperature_2m_max").at(day_index).get<double>())
        );
        wd.lo = static_cast<int>(
            round(d.at("temperature_2m_min").at(day_index).get<double>())
        );
        const auto &precip = d.at("precipitation_probability_max").at(day_index);
        wd.precip = precip.is_null() ? 0 : static_cast<int>(precip.get<double>());

        {
            lock_guard lg(caches::mtx);
            auto &c = caches::weather;
            c.have = true;
            c.data = wd;
            c.day_index = day_index;
            c.last_fetch = static_cast<double>(tu::now_unix());
            c.consecutive_failures = 0;
        }
        log("weather day=", day_index, ": code=", wd.code, " hi=", wd.hi,
            " lo=", wd.lo, " precip=", wd.precip,
            " (", tu::monotonic() - t0, "s)");
        return true;
    } catch (const exception &e) {
        lock_guard lg(caches::mtx);
        auto &c = caches::weather;
        c.consecutive_failures++;
        schedule_retry(c.last_fetch);
        log("weather parse FAILED in ", tu::monotonic() - t0, "s (#",
            c.consecutive_failures, "): ", e.what());
        return false;
    }
}

void weather_loop() {
    while (!g_interrupted.load()) {
        const int desired = tu::desired_day_index();
        double age;
        int cached_day;
        {
            lock_guard lg(caches::mtx);
            age = static_cast<double>(tu::now_unix()) - caches::weather.last_fetch;
            cached_day = caches::weather.day_index;
        }
        if (cached_day != desired || age >= cfg::WEATHER_TTL.count()) {
            refresh_weather(desired);
        }
        // Sleep in small chunks so SIGTERM exits promptly.
        const auto deadline = chrono::steady_clock::now() + cfg::WEATHER_LOOP_INTERVAL;
        while (!g_interrupted.load() && chrono::steady_clock::now() < deadline) {
            this_thread::sleep_for(chrono::milliseconds{200});
        }
    }
}

}  // namespace fetch
