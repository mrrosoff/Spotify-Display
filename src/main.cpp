#include <Arduino.h>

#include "main.h"

// HUB75E pinout
// R1 B1  R2  B2  A C CLK OE
// --------------------------
// G1 GND G2  E   B D LAT GND

#define CH_D 18
#define CLK 22

#define PANEL_WIDTH 32
#define PANEL_HEIGHT 32
#define PANELS_NUMBER 1

#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS PANE_WIDTH * PANE_HEIGHT
#define ONBOARD_LED 2

using namespace std;

const String spotifyClientId = "a9a84f65fc9f47568870f4c0c0185e3a";
const String spotifyClientSecret = "7cb7fe064e1844c19e87a2d475573948";
const String authCode = "AQBcHxqn6hZ04AHTKTeDXQNWq9OxmFY_Yp92jkzc_RI60Di43FiIoFXVwz0mGsl_43VYc-0d4ZgkCgsQspkQZwXKRLYWs3oN6pSjZgpeqmMiqkMdT6OqoGfhpozbFAqZSJ67cO1tibywZc05OOBJujLb-zNp7NHbqEU8MKh-BE1kZwIWlFYxPDLoskeQYdZ8enqOFqVpENwf0A";
const String callBackUrl = "http://127.0.0.1/callback";

SpotifyWiFiClient spotifyClient(spotifyClientId, spotifyClientSecret);
String oauthToken = "";
String albumArtUrl = "";

MatrixPanel_I2S_DMA *matrix = nullptr;

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);

  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);
  mxconfig.gpio.d = CH_D;
  mxconfig.gpio.clk = CLK;
  mxconfig.gpio.b2 = 32;

  matrix = new MatrixPanel_I2S_DMA(mxconfig);

  matrix->begin();

  WiFi.mode(WIFI_STA);
  WiFiUtilities wifi("Glacier Cabin", "Cabin6587");
  wifi.connectToWiFi();

  try {
    oauthToken = spotifyClient.getInitialAuthorizationToken(authCode, callBackUrl);
  } catch (const char *error) {
    Serial.println(error);
  }
}

void loop() {
  try {
    if (time(NULL) > spotifyClient.oauthExpiryTime) {
      oauthToken = spotifyClient.getRefreshAuthorizationToken();
    }
    const String trackUrl = spotifyClient.getCurrentlyPlayingTrackUrl(oauthToken);
    if (albumArtUrl != trackUrl) {
      spotifyClient.printPixels(matrix, PANEL_WIDTH, PANE_HEIGHT, trackUrl);
      albumArtUrl = trackUrl;
    }
  } catch (const char *error) {
    Serial.println(error);
  }
}
