#ifndef SPOTIFY_DISPLAY
#define SPOTIFY_DISPLAY

// Arduino Libraries
#include <SPI.h>
#include <WiFiNINA.h>
#include "base64.hpp"
#include <ArduinoJson.h>

using namespace std;

class SpotifyAPIFunctionality {

    public:
        SpotifyAPIFunctionality(String clientId, String clientSecret, String refreshToken) : 
            clientId(clientId), clientSecret(clientSecret), refreshToken(refreshToken), builtClientId(clientId + ":" + clientSecret),
            postData("grant_type=refresh_token&refresh_token=" + refreshToken){
                encode_base64((unsigned char *) builtClientData.c_str(), strlen(builtClientData.c_str()), base64);
            };

        // Spotify Credientials
        String clientId;
        String builtClientId;
        String clientSecret;
        String refreshToken;
        String builtClientData;
        unsigned char base64[128] = {0};
        String postData;
        String spotifyAccounts = "accounts.spotify.com";
        String spotifyAPI = "api.spotify.com";

};

class SpotifyWiFiClient : public SpotifyAPIFunctionality {

    public:
        SpotifyWiFiClient(String clientId, String clientSecret, String refreshToken) : 
            SpotifyAPIFunctionality(clientId, clientSecret, refreshToken) {};

        // Oauth Methods
        void getOAuthToken() {
            SSLClient.stop();
            if(SSLClient.connectSSL(spotifyAccounts.c_str(), CONNECTION_PORT)) {
                SSLClient.println(POST + " /api/token " + HTTP_VERSION);
                SSLClient.println(HOST + spotifyAPI);
                SSLClient.println(AUTHORIZATION + "Basic " + (char *) base64);
                SSLClient.println(CONTENT_TYPE + "application/x-www-form-urlencoded");
                SSLClient.println(CONTENT_LENGTH + postData.length());
                SSLClient.println(CONNECTION_CLOSE);
                SSLClient.println();
                SSLClient.println(postData);
            }
        }

       // Album Art Methods
       void getAlbumArtURL() {
           SSLClient.stop();
           if(SSLClient.connectSSL(spotifyAPI.c_str(), CONNECTION_PORT)) {
               SSLClient.println(GET + " /v1/me/player/currently-playing " + HTTP_VERSION);
               SSLClient.println(HOST + spotifyAPI);
               SSLClient.println(AUTHORIZATION + "Bearer " + oauthToken);
               SSLClient.println(CONTENT_TYPE + "application/json");
               SSLClient.println(CONNECTION_CLOSE);
               SSLClient.println();
           }
       }

       void getAlbumArt() {
           SSLClient.stop();
           const int beginningIndex = albumArtURL.indexOf("//") + 2;
           String imageApi = albumArtURL.substring(beginningIndex, albumArtURL.indexOf("/", beginningIndex));
           String imageExtension = albumArtURL.substring(albumArtURL.indexOf("/", beginningIndex));

           if(SSLClient.connectSSL(imageApi.c_str(), CONNECTION_PORT)) {
               SSLClient.println(GET + imageExtension + " " + HTTP_VERSION);
               SSLClient.println(HOST + imageApi);
               SSLClient.println(AUTHORIZATION + "Bearer " + oauthToken);
               SSLClient.println(CONNECTION_CLOSE);
               SSLClient.println();
           }
       }

       // Checking methods
       bool checkHTTPStatus() {
           char status[32] = {0};
           SSLClient.readBytesUntil('\r', status, sizeof(status));
           if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
               Serial.print("Unexpected response: ");
               Serial.println(status);
               SSLClient.stop();
               return false;
           }
           return true;
       }

       bool skipHTTPHeaders() {
           char endOfHeaders[] = "\r\n\r\n";
           if (!SSLClient.find(endOfHeaders)) {
               Serial.println("Invalid response");
               SSLClient.stop();
               return false;
           }
       }

        enum ClientType {OAUTH, ALBUM_URL, ALBUM_ART, NUM_CLIENTS};
        void nextClient() {
            currentClient = static_cast<ClientType>((currentClient) + 1 % ClientType::NUM_CLIENTS);
        }

    
        WiFiSSLClient SSLClient;
        ClientType currentClient = ClientType::OAUTH;

        String oauthToken;
        long oauthExpiryTime = 0;

        long nextAlbumArtRetrievalTime = 0;
        long nextDisplayUpdate = 0;
        String albumArtURL;

        // Wifi Constants
        const uint16_t CONNECTION_PORT = 443;
        String GET = "GET";
        String POST = "POST";
        String HOST = "Host: ";
        String AUTHORIZATION = "Authorization: ";
        String HTTP_VERSION = "HTTP/1.1";
        String CONTENT_TYPE = "Content-Type: ";
        String CONTENT_LENGTH = "Content-Length: ";
        String CONNECTION_CLOSE = "Connection: close";
};

class WiFiUtilities {

    public:
        WiFiUtilities(String ssid, String password) : ssid(ssid), password(password) {};

        void checkWiFiModule() {
            if (WiFi.status() == WL_NO_MODULE) {
                Serial.println("Communication With WiFi Module Failed!");
                while (true);
            }

            String firmwareVersion = WiFi.firmwareVersion();
            if (firmwareVersion < WIFI_FIRMWARE_LATEST_VERSION) {
                Serial.println("Please Upgrade the Firmware");
            }
        }

        void connectToWiFi() {
            Serial.println("Connecting to Wifi");

            WiFi.begin(ssid.c_str(), password.c_str());
            while (WiFi.status() != WL_CONNECTED) {
                delay(200);
            }

            Serial.print("Connected to Wifi Network: ");
            Serial.println(ssid.c_str());
        }

        String ssid, password;
};


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

#endif // SPOTIFY_DISPLAY
