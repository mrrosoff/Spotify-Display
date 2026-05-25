#pragma once

namespace fetch {

// Re-fetch the daily forecast for `day_index` (0 = today, 1 = tomorrow) if the
// cache is missing, stale, or for the wrong day. Returns true on success.
bool refresh_weather(int day_index);

// Inspect the weather cache and, if it's missing/stale/wrong-day, do a
// refresh. Otherwise no-op. Called from the main loop so weather and
// Spotify never make concurrent network calls on this single-core box.
void weather_tick();

}  // namespace fetch
