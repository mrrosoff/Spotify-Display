
#include <string>
#include <vector>

#include <ArduinoJson.h>
#include <RGBmatrixPanel.h>

#include "WiFiClient.h"
#include "WiFiUtilities.h"

#define CLK 10
#define OE   11
#define LAT 12
#define A   A0
#define B   A2
#define C   A3
#define D   A6

using namespace std;

string clientId = "a9a84f65fc9f47568870f4c0c0185e3a"; 
string clientSecret = "7cb7fe064e1844c19e87a2d475573948";
string refreshToken = "AQAQL12IRdZORwdVF8sJeSiTrSNRVKbv1yZNqUFcfT6-ztpwK1gDjRFQZ0gQn5kBzKVL_3veFiHku1m8aDFYtThJzY9kjj3oX8_juhXTA9bn5_VSPHdqoSDPnsotBaEMkck"; 

SpotifyWiFiClient spotifyClient(clientId, clientSecret, refreshToken);
typedef SpotifyWiFiClient::ClientType ClientType;

uint8_t RGBPins[6] = { 4, 5, 6, 7, 8, 9 };
RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 32, RGBPins);

void setup() {
  Serial.begin(9600);
  while (!Serial); // Wait For Serial Port to Connect.

  WiFiUtilities wifiCheck("Salsa", "Guacamole");
  wifiCheck.checkWiFiModule();
  wifiCheck.connectToWiFi();

  matrix.begin();
}

void loop() {
  oauthCheckAndRetrieve(spotifyClient);
  albumArtURLCheckAndRetrieve(spotifyClient);
  
  oauthClientLoop(spotifyClient);
  albumArtURLClientLoop(spotifyClient);
  pixelsClientLoop(spotifyClient);

  delay(500);
}

void oauthCheckAndRetrieve(SpotifyWiFiClient &spotifyClient) {
  if(static_cast<long>(millis()) - spotifyClient.oauthExpiryTime > 0) {
    spotifyClient.getOAuthToken();
    spotifyClient.oauthExpiryTime = static_cast<long>(millis()) + 10000;
  }
}

void albumArtURLCheckAndRetrieve(SpotifyWiFiClient &spotifyClient) {
  if(spotifyClient.oauthToken.length() > 0 && static_cast<long>(millis()) - spotifyClient.nextAlbumArtRetrievalTime > 0) {
    spotifyClient.getAlbumArtURL();
    spotifyClient.nextAlbumArtRetrievalTime = static_cast<long>(millis()) + 30000;
  }
}

void oauthClientLoop(SpotifyWiFiClient &spotifyClient) {
  while(spotifyClient.currentClient == ClientType::OAUTH && spotifyClient.SSLClient.available()) {
    if(!spotifyClient.checkHTTPStatus()) return;
    spotifyClient.skipHTTPHeaders();

    Serial.println("Got OAuth Token Response");

    DynamicJsonDocument doc(10000);
    DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient);
    if (error) {
      Serial.println(error.f_str());
      spotifyClient.SSLClient.stop();
      return;
    }

    spotifyClient.oauthToken = doc["access_token"].as<const char*>();
    spotifyClient.oauthExpiryTime = (doc["expires_in"].as<long>() * 1000) - (10 * 1000);
    spotifyClient.SSLClient.stop();

    Serial.println("Retreived OAuth Token");
    Serial.println(spotifyClient.oauthToken.c_str());
  }
}

void albumArtURLClientLoop(SpotifyWiFiClient &spotifyClient) {
  while(spotifyClient.currentClient == ClientType::ALBUM_URL && spotifyClient.SSLClient.available()) {
    if (!spotifyClient.checkHTTPStatus()) return;
    spotifyClient.skipHTTPHeaders();

    Serial.println("Got Album Art Url Response");
    
    DynamicJsonDocument doc(20000);
    DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient);

    if (error) {
      Serial.println(error.f_str());
      spotifyClient.SSLClient.stop();
      return;
    }

    const string albumArtUrl = doc["item"]["album"]["images"][0]["url"].as<const char *>();
    spotifyClient.SSLClient.stop();

    Serial.println("Retreived Album Art Url");
    Serial.println(spotifyClient.albumArtURL.c_str());

    if (spotifyClient.albumArtURL != albumArtUrl) {
      spotifyClient.albumArtURL = albumArtUrl;
      spotifyClient.getPixels();
    }
  }
}

void pixelsClientLoop(SpotifyWiFiClient &spotifyClient) {
  while(spotifyClient.currentClient == ClientType::PIXELS && spotifyClient.SSLClient.available()) {
    if (!spotifyClient.checkHTTPStatus()) return;
    spotifyClient.skipHTTPHeaders();

    Serial.println("Got Pixels Response");

    DynamicJsonDocument doc(40000);
    DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient);
    if (error)
    {
      Serial.println(error.f_str());
      spotifyClient.SSLClient.stop();
      return;
    }

    const JsonArray pixels = doc["pixels"];
    spotifyClient.SSLClient.stop();

    Serial.println("Retreived Pixels");

    writeDisplay(pixels);
  }
} 

void writeDisplay(const JsonArray &pixels) {
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 32; j++)
    {
      matrix.drawPixel(i, j, matrix.Color888(i * 4, j * 4, (millis() / 1000) % 256));
    }
  }
  matrix.updateDisplay();
}
