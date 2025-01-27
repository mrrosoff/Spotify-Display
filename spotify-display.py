#!/usr/bin/env python

import time
import sys
import os

from PIL import Image, JpegImagePlugin, Jpeg2KImagePlugin
from rgbmatrix import RGBMatrix, RGBMatrixOptions
import spotipy
from spotipy.oauth2 import SpotifyOAuth
import requests
from requests import RequestException

options = RGBMatrixOptions()
options.rows = 64
options.cols = 64
options.chain_length = 1
options.parallel = 1
options.pwm_dither_bits = 1
options.limit_refresh_rate_hz = 120
options.hardware_mapping = 'regular'

matrix = RGBMatrix(options = options)

sp = spotipy.Spotify(
    auth_manager=SpotifyOAuth(
        client_id="a9a84f65fc9f47568870f4c0c0185e3a",
        client_secret="7cb7fe064e1844c19e87a2d475573948",
        redirect_uri="http://127.0.0.1/callback",
        scope="user-read-currently-playing",
        open_browser=False,
        cache_path="/home/root/cache.json"
    )
)

try:
    print("Press CTRL-C to stop.")
    prev_track_url = None
    sleep_time = 1.0
    while True:
        info = sp.current_user_playing_track()
        if info is not None:
            url = info["item"]["album"]["images"][-1]["url"]
            if url != prev_track_url:
                prev_track_url = url
                im = Image.open(requests.get(url, stream=True).raw)
                matrix.SetImage(im.convert('RGB'))
        time.sleep(sleep_time)
except RequestException:
    sleep_time += 0.5
except KeyboardInterrupt:
    sys.exit(0)
