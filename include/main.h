#include <ctime>
#include <vector>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>
#include "xtensa/core-macros.h"

#include "SpotifyWiFiClient.h"
#include "WifiUtilities.h"

#define BAUD_RATE 115200
#define SPOTIFY_CLIENT_ID "a9a84f65fc9f47568870f4c0c0185e3a"
#define SPOTIFY_CLIENT_SECRET "7cb7fe064e1844c19e87a2d475573948"
#define SPOTIFY_AUTH_TOKEN "AQAWPsNyWWYl7no8UYC2LNFACqoQhEW6Eca2a2WL8XRWRqIpmLx5AbQiHdobm_xw58nh_vXvBcSQ5lqzGsSMQQrKJfr2PnIljx3KTDjr3MRa5SmvaRua6IXsFbqlDvPNrg8tjB62dvzvQiQ7Unmk6xox8K1pNnuuhXAs-ZI"

void buffclear(CRGB *buf);

