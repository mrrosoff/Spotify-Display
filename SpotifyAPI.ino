#include <SPI.h>
#include <WiFiNINA.h>

#include "base64.hpp"
#include <ArduinoJson.h>

const String GET = "GET";
const String POST = "POST";
const String HTTP_VERSION = "HTTP/1.1";
const String HOST = "Host: ";
const String AUTHORIZATION = "Authorization: ";
const String CONTENT_TYPE = "Content-Type: ";
const String CONTENT_LENGTH = "Content-Length: ";
const String CONNECTION_CLOSE = "Connection: close";

String oauthToken = "";
long oauthExpiryTime = 0;
long nextAlbumArtRetrievalTime = 0;
long nextDisplayUpdate = 0;

WiFiSSLClient oauthClient;
WiFiSSLClient albumArtUrlClient;
WiFiSSLClient albumArtClient;

String albumArtUrl = "";

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;  // Wait For Serial Port to Connect.
  }
  checkWiFiModule();
  connectToWiFi();
}

void checkWiFiModule() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication With WiFi Module Failed!");
    while (true)
      ;
  }

  const String firmwareVersion = WiFi.firmwareVersion();
  if (firmwareVersion < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please Upgrade the Firmware");
  }
}

void connectToWiFi() {
  const String ssid = "The Force";
  const String pass = "thisistheway";

  Serial.println("Connecting to Wifi");

  WiFi.begin(ssid.c_str(), pass.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
  }

  Serial.print("Connected to Wifi Network: ");
  Serial.println(ssid.c_str());
}

void loop() {
  if (static_cast<long>(millis()) - oauthExpiryTime > 0) {
    Serial.println("Requesting OAuth Token");
    getOAuthToken();
    oauthExpiryTime = static_cast<long>(millis()) + 10000;
  }

  if (oauthToken.length() > 0 && static_cast<long>(millis()) - nextAlbumArtRetrievalTime > 0) {
     Serial.println("Requesting Album Art Url");
     getAlbumArtUrl();
     nextAlbumArtRetrievalTime = static_cast<long>(millis()) + 50000;
  }

  if (albumArtUrl.length() > 0 && static_cast<long>(millis()) - nextDisplayUpdate > 0) {
    Serial.println("Requesting Album Art");
    getAlbumArt();
    nextDisplayUpdate = static_cast<long>(millis()) + 10000;
  }

  while (oauthClient.available()) {
    if (!checkHTTPStatus(oauthClient)) return;
    skipHTTPHeaders(oauthClient);

    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, oauthClient);
    if (error) {
      Serial.println(error.f_str());
      oauthClient.stop();
      return;
    }

    oauthToken = doc["access_token"].as<char*>();
    oauthExpiryTime = (doc["expires_in"].as<long>() * 1000) - (10 * 1000);
    oauthClient.stop();
  }

  while (albumArtUrlClient.available()) {
    if (!checkHTTPStatus(albumArtUrlClient)) return;
    skipHTTPHeaders(albumArtUrlClient);

    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, albumArtUrlClient);
    if (error) {
      Serial.println(error.f_str());
      albumArtUrlClient.stop();
      return;
    }

    albumArtUrl = doc["item"]["album"]["images"][2]["url"].as<char*>();
    albumArtUrlClient.stop();
  }

  while (albumArtClient.available()) {
    // if (!checkHTTPStatus(albumArtClient)) return;
    // skipHTTPHeaders(albumArtClient);

    char c = albumArtClient.read();
    Serial.write(c);
    // DynamicJsonDocument doc(16384);
    // DeserializationError error = deserializeJson(doc, albumArtClient);
    // if (error) {
    //   Serial.println(error.f_str());
    //   albumArtClient.stop();
    //   return;
    // }

    // albumArtClient.stop();
  }
}

void getOAuthToken()
{
  String client_id = "a9a84f65fc9f47568870f4c0c0185e3a";
  String client_secret = "7cb7fe064e1844c19e87a2d475573948";
  String spotifyRefreshToken = "AQAQL12IRdZORwdVF8sJeSiTrSNRVKbv1yZNqUFcfT6-ztpwK1gDjRFQZ0gQn5kBzKVL_3veFiHku1m8aDFYtThJzY9kjj3oX8_juhXTA9bn5_VSPHdqoSDPnsotBaEMkck";

  String builtClientData = client_id + ":" + client_secret;
  unsigned char base64[128];
  encode_base64((unsigned char *) builtClientData.c_str(), strlen(builtClientData.c_str()), base64);

  String postData = "grant_type=refresh_token&refresh_token=" + spotifyRefreshToken;
    
  oauthClient.stop();
  const String spotifyApi = "accounts.spotify.com";

  if (oauthClient.connectSSL(spotifyApi.c_str(), 443)) {
    oauthClient.println(POST + " /api/token " + HTTP_VERSION);
    oauthClient.println(HOST + spotifyApi);
    oauthClient.println(AUTHORIZATION + "Basic " + (char *) base64);
    oauthClient.println(CONTENT_TYPE + "application/x-www-form-urlencoded");
    oauthClient.println(CONTENT_LENGTH + postData.length());
    oauthClient.println(CONNECTION_CLOSE);
    oauthClient.println();
    oauthClient.println(postData);
  }
}

void getAlbumArtUrl() {
  albumArtUrlClient.stop();
  const String spotifyApi = "api.spotify.com";

  if (albumArtUrlClient.connectSSL(spotifyApi.c_str(), 443)) {
    albumArtUrlClient.println(GET + " /v1/me/player/currently-playing " + HTTP_VERSION);
    albumArtUrlClient.println(HOST + spotifyApi);
    albumArtUrlClient.println(AUTHORIZATION + "Bearer " + oauthToken);
    albumArtUrlClient.println(CONTENT_TYPE + "application/json");
    albumArtUrlClient.println(CONNECTION_CLOSE);
    albumArtUrlClient.println();
  }
}

void getAlbumArt() {
  albumArtClient.stop();
  const int beginningIndex = albumArtUrl.indexOf("//") + 2;
  const String imageApi = albumArtUrl.substring(beginningIndex, albumArtUrl.indexOf("/", beginningIndex));
  const String imageExtension = albumArtUrl.substring(albumArtUrl.indexOf("/", beginningIndex));

  if (albumArtClient.connectSSL(imageApi.c_str(), 443)) {
    albumArtClient.println(GET + " " + imageExtension + " " + HTTP_VERSION);
    albumArtClient.println(HOST + imageApi);
    albumArtClient.println(AUTHORIZATION + "Bearer " + oauthToken);
    albumArtClient.println(CONNECTION_CLOSE);
    albumArtClient.println();
  }
}

bool checkHTTPStatus(WiFiSSLClient &client) {
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print("Unexpected response: ");
    Serial.println(status);
    client.stop();
    return false;
  }
  return true;
}

bool skipHTTPHeaders(WiFiSSLClient &client) {
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println("Invalid response");
    client.stop();
    return false;
  }
}
