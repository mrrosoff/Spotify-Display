#pragma once

#include <chrono>
#include <ctime>
#include <string>

namespace tu {

inline double monotonic() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

inline std::time_t now_unix() {
    return std::time(nullptr);
}

inline int now_minute_of_day() {
    const auto t = now_unix();
    std::tm tm{};
    localtime_r(&t, &tm);
    return tm.tm_hour * 60 + tm.tm_min;
}

bool is_night();

// 0 for today's forecast, 1 for tomorrow's. After 10:30 PM (before midnight)
// returns 1 so the user sees the next day they'll wake up to.
int desired_day_index();

// "MAY 22" header for today() + day_offset days.
std::string month_day_for(int day_offset);

// 12-hour clock with no leading zero, no am/pm — e.g. "10:42", "2:08".
std::string clock_hhmm();

void to_upper(std::string &s);

}  // namespace tu
