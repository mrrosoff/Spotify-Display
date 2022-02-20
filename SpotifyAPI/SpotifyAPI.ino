// Arduino Libraries
#include <ArduinoJson.h>

// Personal Libraries
#include <SpotifyAPIFunctionality.h>
#include <SpotifyWiFiClient.h>
#include <WiFiUtilities.h>

using namespace std;

// SETUP & LOOP
// TODO: Don't want in loop for redeclaring every time. Best Place??
String clientId = "a9a84f65fc9f47568870f4c0c0185e3a"; 
String clientSecret = "7cb7fe064e1844c19e87a2d475573948";
String refreshToken = "AQAQL12IRdZORwdVF8sJeSiTrSNRVKbv1yZNqUFcfT6-ztpwK1gDjRFQZ0gQn5kBzKVL_3veFiHku1m8aDFYtThJzY9kjj3oX8_juhXTA9bn5_VSPHdqoSDPnsotBaEMkck"; 

SpotifyWiFiClient spotifyClient(clientId, clientSecret, refreshToken);
typedef SpotifyWiFiClient::ClientType ClientType;

void setup() {
  Serial.begin(9600);
  while (!Serial); // Wait For Serial Port to Connect.

  String ssid = "The Force", password = "thisistheway";
  WiFiUtilities wifiCheck(ssid, password);
  wifiCheck.checkWiFiModule();
  wifiCheck.connectToWiFi();

  
}

// Checking Clients Functions
void oauthCheckAndRetrieve(SpotifyWiFiClient &spotifyClient) {
  if(static_cast<long>(millis()) - spotifyClient.oauthExpiryTime > 0) {
        Serial.println("Requesting OAuth Token");
        spotifyClient.getOAuthToken();
        spotifyClient.oauthExpiryTime = static_cast<long>(millis()) + 10000;
    }
}

void albumArtURLCheckAndRetrieve(SpotifyWiFiClient &spotifyClient) {
      if(spotifyClient.oauthToken.length() > 0 && static_cast<long>(millis()) - spotifyClient.nextAlbumArtRetrievalTime > 0) {
        Serial.println("Requesting Album Art");
        spotifyClient.getAlbumArtURL();
        spotifyClient.nextAlbumArtRetrievalTime = static_cast<long>(millis()) + 50000;
    }
}

void albumArtCheckAndRetrieve(SpotifyWiFiClient &spotifyClient) {
  if(spotifyClient.albumArtURL.length() > 0 && static_cast<long>(millis()) - spotifyClient.nextDisplayUpdate > 0) {
        Serial.println("Retrieving Album Art");
        spotifyClient.getAlbumArt();
        spotifyClient.nextDisplayUpdate = static_cast<long>(millis()) + 10000;
    }
}

// Client Loops
void oauthClientLoop(SpotifyWiFiClient &spotifyClient) {
  while(spotifyClient.currentClient == ClientType::OAUTH && spotifyClient.SSLClient.available()) {
        if(!spotifyClient.checkHTTPStatus()) return;
        spotifyClient.skipHTTPHeaders();

        DynamicJsonDocument doc(16384);
        DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient);
        if (error) {
            Serial.println(error.f_str());
            spotifyClient.SSLClient.stop();
            return;
        }

        spotifyClient.oauthToken = doc["access_token"].as<const char*>();
        spotifyClient.oauthExpiryTime = (doc["expires_in"].as<long>() * 1000) - (10 * 1000);
        spotifyClient.nextClient();
        spotifyClient.SSLClient.stop();
    }
}

void albumArtURLClientLoop(SpotifyWiFiClient &spotifyClient) {
  while(spotifyClient.currentClient == ClientType::ALBUM_URL && spotifyClient.SSLClient.available()) {
        if (!spotifyClient.checkHTTPStatus()) return;
        spotifyClient.skipHTTPHeaders();

        DynamicJsonDocument doc(16384);
        DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient);
        if (error) {
            Serial.println(error.f_str());
            spotifyClient.SSLClient.stop();
            return;
        }

        spotifyClient.albumArtURL = doc["item"]["album"]["images"][2]["url"].as<const char*>();
        spotifyClient.nextClient();
        spotifyClient.SSLClient.stop();
    }
}

void albumArtClientLoop(SpotifyWiFiClient &spotifyClient) {
  while(spotifyClient.currentClient == ClientType::ALBUM_ART && spotifyClient.SSLClient.available()) {
        // if (!checkHTTPStatus(albumArtClient)) return;
        // skipHTTPHeaders(albumArtClient);

        char c = spotifyClient.SSLClient.read();
        Serial.write(c);
        // DynamicJsonDocument doc(16384);
        // DeserializationError error = deserializeJson(doc, spotifyClient.SSLClient);
        // if (error) {
        //   Serial.println(error.f_str());
        //   spotifyClient.SSLClient.stop();
        //   return;
        // }

        // spotifyClient.nextClient();
        // spotifyClient.SSLClient.stop();
    }
}


void loop() {

    oauthCheckAndRetrieve(spotifyClient);
    
    albumArtURLCheckAndRetrieve(spotifyClient);

    albumArtCheckAndRetrieve(spotifyClient);

    // OAUTH Client Loop
    oauthClientLoop(spotifyClient);

    // Album URL Client Loop
    albumArtURLClientLoop(spotifyClient);

    // Album Art Client Loop
    albumArtClientLoop(spotifyClient);

}
