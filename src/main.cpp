#include <Arduino.h>

#include "main.h"

// HUB75E pinout
// R1 B1  R2  B2  A C CLK OE
// --------------------------
// G1 GND G2  E   B D LAT GND

#define R1 25
#define G1 26
#define BL1 27
#define R2 14
#define G2 12
#define BL2 13
#define CH_A 23
#define CH_B 19
#define CH_C 5
#define CH_D 22
#define CH_E 33
#define CLK 18
#define LAT 2
#define OE 15

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64
#define PANELS_NUMBER 1

#define PANE_WIDTH PANEL_WIDTH *PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH *PANE_HEIGHT

using namespace std;

const String spotifyClientId = "a9a84f65fc9f47568870f4c0c0185e3a";
const String spotifyClientSecret = "7cb7fe064e1844c19e87a2d475573948";
const String authCode = "AQBmgjiOlJXdftOoDXVcZy9lydaemQB55bzlCuGSdn273gQ6DelImeePPUtW2LMXNmsK6cHY8-hC4ut0b8ppK3DFrVXpLnAmmxOjT9GWAGKjjSwCNItYF5bSvjyx7gNVExd0h_VtFjUTdy6Ku7U_hf-vugAyUgKZO7QD2_U";
const String callBackUrl = "http://127.0.0.1/callback";

SpotifyWiFiClient spotifyClient(spotifyClientId, spotifyClientSecret);
String oauthToken = "";
String albumArtUrl = "";

MatrixPanel_I2S_DMA *matrix = nullptr;

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);

  HUB75_I2S_CFG::i2s_pins _pins = {R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER, _pins);

  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  matrix->setBrightness8(255);

  WiFi.mode(WIFI_STA);
  WiFiUtilities wifi("Salsa", "Guacamole");
  wifi.connectToWiFi();

  try {
    spotifyClient.getInitialAuthorizationToken(authCode, callBackUrl);
  } catch (const char *error) {
    Serial.println(error);
  }
}

void loop()
{
  try {
    if (time(NULL) > spotifyClient.oauthExpiryTime) {
      oauthToken = spotifyClient.getRefreshAuthorizationToken();
    }
    const String trackUrl = spotifyClient.getCurrentlyPlayingTrackUrl(oauthToken);
    if (albumArtUrl != trackUrl) {
      const vector<vector<vector<int>>> pixels = spotifyClient.getPixels(trackUrl, PANEL_WIDTH, PANE_HEIGHT);
      for (int y = 0; y != PANE_HEIGHT; ++y) {
        for (int x = 0; x != PANE_WIDTH; ++x) {
          matrix->drawPixelRGB888(x, y, pixels[x][y][0], pixels[x][y][1], pixels[x][y][2]);
        }
      }
      albumArtUrl = trackUrl;
    }
  } catch (const char *error) {
    Serial.println(error);
  }
  delay(5000);
}
