#pragma once

namespace fetch {

// Re-fetch the daily forecast for `day_index` (0 = today, 1 = tomorrow) if the
// cache is missing, stale, or for the wrong day. Returns true on success.
bool refresh_weather(int day_index);

// Pump function for the background weather thread. Sleeps between checks
// and refreshes when overdue.
void weather_loop();

}  // namespace fetch
