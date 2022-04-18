
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
  
  clientResponseChecker(spotifyClient);
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
    spotifyClient.nextAlbumArtRetrievalTime = static_cast<long>(millis()) + 10000;
  }
}

void clientResponseChecker(SpotifyWiFiClient &spotifyClient) {
  if (spotifyClient.SSLClient.available()) {
    if(!spotifyClient.checkHTTPStatus()) return;
    spotifyClient.skipHTTPHeaders();
    
    switch(spotifyClient.currentClient) {
      case ClientType::OAUTH:
        Serial.println("OAuth Token Response");
        getOAuthTokenResponse(spotifyClient);
        return;
      case ClientType::ALBUM_URL:
        Serial.println("Album Art Url Response");
        getAlbumArtUrlResponse(spotifyClient);
        return;
      case ClientType::PIXELS:
        Serial.println("Pixels Response");
        getPixelsResponse(spotifyClient);
        return;
    }
  }
}

void getOAuthTokenResponse(SpotifyWiFiClient &spotifyClient) { 
  StaticJsonDocument<768> doc;
  DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient);
  if (error) {
    Serial.println(error.f_str());
    spotifyClient.SSLClient.stop();
    return;
  }

  spotifyClient.oauthToken = doc["access_token"].as<const char*>();
  spotifyClient.oauthExpiryTime = (doc["expires_in"].as<long>() * 1000) - (10 * 1000);
  spotifyClient.SSLClient.stop();

  Serial.println(("OAuth Token: " + spotifyClient.oauthToken).c_str());
}

void getAlbumArtUrlResponse(SpotifyWiFiClient &spotifyClient) {
  StaticJsonDocument<256> filter;
  filter["item"]["album"]["images"][0]["url"] = true;

  StaticJsonDocument<384> doc;
  DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient, DeserializationOption::Filter(filter));
  if (error) {
    Serial.println(error.f_str());
    spotifyClient.SSLClient.stop();
    return;
  }

  const string albumArtURL = doc["item"]["album"]["images"][2]["url"].as<const char *>();
  spotifyClient.SSLClient.stop();

  Serial.println(("Album Art Url: " + albumArtURL).c_str());

  if (spotifyClient.albumArtURL != albumArtURL) {
    spotifyClient.albumArtURL = albumArtURL;
    spotifyClient.getPixels();
  }
}

void getPixelsResponse(SpotifyWiFiClient &spotifyClient) {
  spotifyClient.SSLClient.find("[");
  int i = 0;
  
  do {
    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient);
    if (error)
    {
      Serial.println(error.f_str());
      spotifyClient.SSLClient.stop();
      return;
    }

    matrix.drawPixel(i / 32, i % 32, matrix.Color888(doc["r"].as< int>(), doc["g"].as< int>(), doc["b"].as< int>()));
    i++;
  } while (spotifyClient.SSLClient.findUntil(",", "]"));

  spotifyClient.SSLClient.stop();

  Serial.println("Wrote Pixels");
  matrix.updateDisplay();
}
