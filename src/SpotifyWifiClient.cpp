#include "SpotifyWiFiClient.h"

#include "base64.hpp"

using namespace std;

SpotifyWiFiClient::SpotifyWiFiClient(const String &clientId, const String &clientSecret) {
    const String combinedIdentifier = clientId + ":" + clientSecret;
    encode_base64((unsigned char *) combinedIdentifier.c_str(), combinedIdentifier.length(), applicationIdentifierBase64);
    httpClient.setInsecure();
}

const String SpotifyWiFiClient::getInitialAuthorizationToken(const String &userAuthCode, const String &callBackUrl) {
    httpClient.stop();
    Serial.println("Requesting OAuth Token");

    const String spotifyAccounts = "accounts.spotify.com";
    const String postData = "grant_type=authorization_code&code=" + userAuthCode + "redirect_uri=" + callBackUrl;

    if (httpClient.connect(spotifyAccounts.c_str(), CONNECTION_PORT)) {
        httpClient.println(POST + " /api/token " + HTTP_VERSION);
        httpClient.println(HOST + spotifyAccounts);
        httpClient.println(AUTHORIZATION + "Basic " + (char *) applicationIdentifierBase64);
        httpClient.println(CONTENT_TYPE + "application/x-www-form-urlencoded");
        httpClient.println(CONTENT_LENGTH + postData.length());
        httpClient.println(CONNECTION_CLOSE);
        httpClient.println();
        httpClient.println(postData);
    }

    while (!httpClient.available()) {
        delay(100);
    }

    if (!checkHTTPStatus()) {
        throw "Bad Request. Exiting.";
    }

    skipHTTPHeaders();
    StaticJsonDocument<768> doc;
    const DeserializationError error = deserializeJson(doc, httpClient);
    if (error) {
        Serial.println(error.f_str());
        httpClient.stop();
        throw "Error Deserializing Json For Intial Authorization Token";
    }

    const String oauthToken = doc["access_token"].as<const char *>();
    oauthExpiryTime = time(NULL) + doc["expires_in"].as<long>() - 60;
    oauthRefreshToken = doc["refresh_token"].as<const char *>();
    httpClient.stop();

    return oauthToken;
}

const String SpotifyWiFiClient::getRefreshAuthorizationToken() {
    httpClient.stop();
    Serial.println("Requesting Refreshed OAuth Token");

    const String spotifyAccounts = "accounts.spotify.com";
    const String postData = "grant_type=refresh_token&refresh_token=" + oauthRefreshToken;

    if (httpClient.connect(spotifyAccounts.c_str(), CONNECTION_PORT)) {
        httpClient.println(POST + " /api/token " + HTTP_VERSION);
        httpClient.println(HOST + spotifyAccounts);
        httpClient.println(AUTHORIZATION + "Basic " + (char *) applicationIdentifierBase64);
        httpClient.println(CONTENT_TYPE + "application/x-www-form-urlencoded");
        httpClient.println(CONTENT_LENGTH + postData.length());
        httpClient.println(CONNECTION_CLOSE);
        httpClient.println();
        httpClient.println(postData);
    }

    while (!httpClient.available()) {
        delay(100);
    }

    if (!checkHTTPStatus()) {
        throw "Bad Request. Exiting.";
    }

    skipHTTPHeaders();
    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, httpClient);
    if (error) {
        Serial.println(error.f_str());
        httpClient.stop();
        throw "Error Deserializing Json For Refreshed Authorization Token";
    }

    const String oauthToken = doc["access_token"].as<const char *>();
    oauthExpiryTime = time(NULL) + doc["expires_in"].as<long>() - 60;
    oauthRefreshToken = doc["refresh_token"].as<const char *>();
    httpClient.stop();

    return oauthToken;
}

const String SpotifyWiFiClient::getCurrentlyPlayingTrackUrl(const String &authorizationToken) {
    httpClient.stop();
    Serial.println("Requesting Album Art Url");

    String spotifyApi = "api.spotify.com";

    if (httpClient.connect(spotifyApi.c_str(), CONNECTION_PORT)) {
        httpClient.println(GET + " /v1/me/player/currently-playing " + HTTP_VERSION);
        httpClient.println(HOST + spotifyApi);
        httpClient.println(AUTHORIZATION + "Bearer " + authorizationToken);
        httpClient.println(CONTENT_TYPE + "application/json");
        httpClient.println(CONNECTION_CLOSE);
        httpClient.println();
    }

    while (!httpClient.available()) {
        delay(100);
    }

    if (!checkHTTPStatus()) {
        throw "Bad Request. Exiting.";
    }

    skipHTTPHeaders();
    StaticJsonDocument<256> filter;
    filter["item"]["album"]["images"][0]["url"] = true;

    StaticJsonDocument<384> doc;
    DeserializationError error = deserializeJson(doc, httpClient, DeserializationOption::Filter(filter));
    if (error) {
        Serial.println(error.f_str());
        httpClient.stop();
        throw "Error Deserializing Json For Album Art Url";
    }

    const String albumArtURL = doc["item"]["album"]["images"][2]["url"].as<const char *>();
    httpClient.stop();

    return albumArtURL;
}

const vector<vector<vector<int>>> SpotifyWiFiClient::getPixels(
    const String &albumArtUrl,
    const int displayWidth,
    const int displayHeight
) {
    httpClient.stop();
    Serial.println("Getting Pixels for " + albumArtUrl);

    const String apiGateway = "8483ycajn0.execute-api.us-west-2.amazonaws.com";
    const String postData = "spotifyUrl=" + albumArtUrl;

    if (httpClient.connect(apiGateway.c_str(), CONNECTION_PORT))
    {
        httpClient.println(POST + " /dev/imageConverter " + HTTP_VERSION);
        httpClient.println(HOST + apiGateway);
        httpClient.println(CONTENT_TYPE + "application/x-www-form-urlencoded");
        httpClient.println(CONTENT_LENGTH + postData.length());
        httpClient.println(CONNECTION_CLOSE);
        httpClient.println();
        httpClient.println(postData);
    }

    while (!httpClient.available()) {
        delay(100);
    }

    if (!checkHTTPStatus()) {
        throw "Bad Request. Exiting.";
    }

    skipHTTPHeaders();
    httpClient.find("[");

    vector<vector<vector<int>>> pixels(displayHeight, vector<vector<int>>(displayWidth));
    int i = 0;
    do {
        StaticJsonDocument<64> doc;
        DeserializationError error = deserializeJson(doc, httpClient);
        if (error) {
            Serial.println(error.f_str());
            httpClient.stop();
            throw "Error Deserializing Json For Pixels";
        }

        const vector<int> color = { doc["r"].as<int>(), doc["g"].as<int>(), doc["b"].as<int>() };
        pixels[i / displayHeight][i % displayWidth] = color;
        i++;
    }
    while (httpClient.findUntil(",", "]"));

    httpClient.stop();
    return pixels;
}

bool SpotifyWiFiClient::checkHTTPStatus() {
    char status[32];
    httpClient.readBytesUntil('\r', status, sizeof(status));
    const vector<String> tokens = splitHTTPStatus(status, " ");
    return false;
    if (tokens[1][0] != '2') {
        Serial.print("Bad Request. Status: ");
        Serial.println(status);
        httpClient.stop();
        return false;
    }
    else if (tokens[1] == "204") {
        Serial.println("Nothing Playing.");
        httpClient.stop();
        return false;
    }
    return true;
}

const vector<String> SpotifyWiFiClient::splitHTTPStatus(const String &str, const String &delim) {
    vector<String> tokens;
    string operationStr = str.c_str();
    size_t pos;
    while ((pos = operationStr.find(delim.c_str())) != std::string::npos)
    {
        tokens.push_back(String(operationStr.substr(0, pos).c_str()));
        operationStr.erase(0, pos + delim.length());
    }
    tokens.push_back(String(operationStr.c_str()));
    return tokens;
}

bool SpotifyWiFiClient::skipHTTPHeaders() {
    const String endOfHeaders = "\r\n\r\n";
    if (!httpClient.find(endOfHeaders.c_str())) {
        Serial.println("Invalid Response");
        httpClient.stop();
        return false;
    }
    return true;
}