#include "WiFiClient.h"

#include "base64.hpp"

using namespace std;

SpotifyWiFiClient::SpotifyWiFiClient(const string &clientId, const string &clientSecret, const string &refreshToken) : 
    
spotifyApplicationIdentifier(clientId + ":" + clientSecret), oauthPostData("grant_type=refresh_token&refresh_token=" + refreshToken) 

{
    encode_base64((unsigned char *) spotifyApplicationIdentifier.c_str(), strlen(spotifyApplicationIdentifier.c_str()), base64);
}

void SpotifyWiFiClient::getOAuthToken() {
    SSLClient.stop();
    const string spotifyAccounts = "accounts.spotify.com";
    currentClient = ClientType::OAUTH;
    Serial.println("Requesting OAuth Token");

    if(SSLClient.connectSSL(spotifyAccounts.c_str(), CONNECTION_PORT)) {
        SSLClient.println((POST + " /api/token " + HTTP_VERSION).c_str());
        SSLClient.println((HOST + spotifyAccounts).c_str());
        SSLClient.println((AUTHORIZATION + "Basic " + (char *) base64).c_str());
        SSLClient.println((CONTENT_TYPE + "application/x-www-form-urlencoded").c_str());
        SSLClient.println((CONTENT_LENGTH + to_string(oauthPostData.size())).c_str());
        SSLClient.println(CONNECTION_CLOSE.c_str());
        SSLClient.println();
        SSLClient.println(oauthPostData.c_str());
    }
}

void SpotifyWiFiClient::getAlbumArtURL() {
    SSLClient.stop();
    string spotifyApi = "api.spotify.com";
    currentClient = ClientType::ALBUM_URL;
    Serial.println("Requesting Album Art Url");

    if(SSLClient.connectSSL(spotifyApi.c_str(), CONNECTION_PORT)) {
        SSLClient.println((GET + " /v1/me/player/currently-playing " + HTTP_VERSION).c_str());
        SSLClient.println((HOST + spotifyApi).c_str());
        SSLClient.println((AUTHORIZATION + "Bearer " + oauthToken).c_str());
        SSLClient.println((CONTENT_TYPE + "application/json").c_str());
        SSLClient.println(CONNECTION_CLOSE.c_str());
        SSLClient.println();
    }
}

void SpotifyWiFiClient::getPixels() {
    SSLClient.stop();
    const string apiGateway = "pe575u0znd.execute-api.us-west-2.amazonaws.com";
    currentClient = ClientType::PIXELS;
    const string postData = "{\"spotifyUrl\": \"" + albumArtURL + "\"}";
    Serial.println("Getting Pixels");

    if (SSLClient.connectSSL(apiGateway.c_str(), CONNECTION_PORT)){
        SSLClient.println((POST + " /dev/imageConverter " + HTTP_VERSION).c_str());
        SSLClient.println((HOST + apiGateway).c_str());
        SSLClient.println((CONTENT_TYPE + "application/json").c_str());
        SSLClient.println((CONTENT_LENGTH + to_string(postData.size())).c_str());
        SSLClient.println(CONNECTION_CLOSE.c_str());
        SSLClient.println();
        SSLClient.println(postData.c_str());
    }
}

bool SpotifyWiFiClient::checkHTTPStatus() {
    char status[32];
    SSLClient.readBytesUntil('\r', status, sizeof(status));
    vector<string> tokens = splitHTTPStatus(status, " ");
    if (tokens[1][0] != '2') {
       Serial.print("Bad Request. Status: ");
       Serial.println(status);
       SSLClient.stop();
       return false;
    } else if (tokens[1] == "204") {
       Serial.println("Nothing Playing.");
       SSLClient.stop();
       return false;
    }
    return true;
}

vector<string> SpotifyWiFiClient::splitHTTPStatus(const string &str, const string &delim) {
  vector<string> tokens;
  string operationStr = str;
  size_t pos;
  while ((pos = operationStr.find(delim)) != std::string::npos) {
    tokens.push_back(operationStr.substr(0, pos));
    operationStr.erase(0, pos + delim.length());
  }
  tokens.push_back(operationStr);
  return tokens;
}

bool SpotifyWiFiClient::skipHTTPHeaders() {
    string endOfHeaders = "\r\n\r\n";
    if (!SSLClient.find(endOfHeaders.c_str())) {
        Serial.println("Invalid response");
        SSLClient.stop();
        return false;
    }
}
