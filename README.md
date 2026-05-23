# Spotify Display

A 64×64 LED matrix display that shows your current Spotify album art, and
falls back to a daily weather forecast (with a clock) when nothing is
playing.

![display](./display.jpg)

## Hardware

Raspberry Pi (Zero W works, anything newer is fine) driving a 64×64 RGB LED
matrix via [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix).
A 32×32 panel is not recommended — album art becomes unreadable.

## Install

Dependencies on the Pi:

```bash
sudo apt install build-essential libcurl4-openssl-dev nlohmann-json3-dev
git clone https://github.com/hzeller/rpi-rgb-led-matrix.git ~/rpi-rgb-led-matrix
make -C ~/rpi-rgb-led-matrix/lib
```

Clone this repo into `~/Spotify-Display`, then run the one-time setup:

```bash
cd ~/Spotify-Display
./scripts/setup.sh
```

`setup.sh` walks you through the Spotify OAuth flow, writes
`/etc/spotifydisplay.env` with the resulting refresh token, and installs the
systemd unit. After it finishes:

```bash
make
sudo systemctl enable --now spotifydisplay
sudo journalctl -u spotifydisplay -f
```

The Makefile downloads `stb_image.h` on first build for JPEG/PNG decode.
Override `RGB_LIB_DISTRIBUTION` if the matrix lib lives somewhere else.

## Behavior

- Polls Spotify every second; shows album art while a track plays.
- After 30 s with no playback, switches to the weather screen, which shows
  the date, a clock in the top-right, the day's high/low/precipitation, and
  the condition word.
- Weather refreshes every 30 min from open-meteo (San Diego coordinates).
- Between 10:30 PM and midnight, shows tomorrow's forecast; from midnight
  to 7 AM, shows the current day.
- Dims the panel from 100 → 40 between 10:30 PM and 7 AM.

## Deploy updates

```bash
sudo systemctl stop spotifydisplay
git pull && make
sudo systemctl start spotifydisplay
```
