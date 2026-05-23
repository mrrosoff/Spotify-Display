#include "util/time_utils.hpp"
#include "core/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

using namespace std;

namespace tu {

bool is_night() {
    const int m = now_minute_of_day();
    return m >= cfg::NIGHT_START_MIN || m < cfg::NIGHT_END_MIN;
}

int desired_day_index() {
    // Only bump to tomorrow during the evening half of the night window;
    // after midnight, day 0 is already the day the user cares about.
    return now_minute_of_day() >= cfg::NIGHT_START_MIN ? 1 : 0;
}

string month_day_for(int day_offset) {
    const auto t = now_unix() + day_offset * 86400;
    tm tmv{};
    localtime_r(&t, &tmv);
    char buf[16];
    strftime(buf, sizeof(buf), "%b %d", &tmv);
    string s(buf);
    to_upper(s);
    return s;
}

string clock_hhmm() {
    const auto t = now_unix();
    tm tmv{};
    localtime_r(&t, &tmv);
    int hour = tmv.tm_hour % 12;
    if (hour == 0) hour = 12;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d:%02d", hour, tmv.tm_min);
    return string(buf);
}

void to_upper(string &s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return toupper(c); });
}

}  // namespace tu
