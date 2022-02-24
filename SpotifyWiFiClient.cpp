#include "SpotifyWiFiClient.h"

using namespace std;

SpotifyWiFiClient::SpotifyWiFiClient(const string &clientId, const string &clientSecret, const string &refreshToken) : 
    
spotifyApplicationIdentifier(clientId + ":" + clientSecret), postData("grant_type=refresh_token&refresh_token=" + refreshToken) 

{
    encode_base64((unsigned char *) spotifyApplicationIdentifier.c_str(), strlen(spotifyApplicationIdentifier.c_str()), base64);
}

void SpotifyWiFiClient::getOAuthToken() {
    SSLClient.stop();
    const string spotifyAccounts = "accounts.spotify.com";   
     
    if(SSLClient.connectSSL(spotifyAccounts.c_str(), CONNECTION_PORT)) {
        SSLClient.println((POST + " /api/token " + HTTP_VERSION).c_str());
        SSLClient.println((HOST + spotifyAccounts).c_str());
        SSLClient.println((AUTHORIZATION + "Basic " + (char *) base64).c_str());
        SSLClient.println((CONTENT_TYPE + "application/x-www-form-urlencoded").c_str());
        SSLClient.println((CONTENT_LENGTH + to_string(postData.size())).c_str());
        SSLClient.println(CONNECTION_CLOSE.c_str());
        SSLClient.println();
        SSLClient.println(postData.c_str());
    }
}

void SpotifyWiFiClient::getAlbumArtURL() {
    SSLClient.stop();
    string spotifyApi = "api.spotify.com";

    if(SSLClient.connectSSL(spotifyApi.c_str(), CONNECTION_PORT)) {
        SSLClient.println((GET + " /v1/me/player/currently-playing " + HTTP_VERSION).c_str());
        SSLClient.println((HOST + spotifyApi).c_str());
        SSLClient.println((AUTHORIZATION + "Bearer " + oauthToken).c_str());
        SSLClient.println((CONTENT_TYPE + "application/json").c_str());
        SSLClient.println(CONNECTION_CLOSE.c_str());
        SSLClient.println();
    }
}

void SpotifyWiFiClient::getAlbumArt() {
    SSLClient.stop();
    const int beginningIndex = albumArtURL.find("//") + 2;
    string imageApi = albumArtURL.substr(beginningIndex, albumArtURL.find("/", beginningIndex));
    string imageExtension = albumArtURL.substr(albumArtURL.find("/", beginningIndex));

    if(SSLClient.connectSSL(imageApi.c_str(), CONNECTION_PORT)) {
        SSLClient.println((GET + imageExtension + " " + HTTP_VERSION).c_str());
        SSLClient.println((HOST + imageApi).c_str());
        SSLClient.println((AUTHORIZATION + "Bearer " + oauthToken).c_str());
        SSLClient.println(CONNECTION_CLOSE.c_str());
        SSLClient.println();
    }
}

bool SpotifyWiFiClient::checkHTTPStatus() {
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

bool SpotifyWiFiClient::skipHTTPHeaders() {
    string endOfHeaders = "\r\n\r\n";
    if (!SSLClient.find(endOfHeaders.c_str())) {
        Serial.println("Invalid response");
        SSLClient.stop();
        return false;
    }
}

void SpotifyWiFiClient::nextClient() {
    currentClient = static_cast<ClientType>((currentClient) + 1 % ClientType::NUM_CLIENTS);
}
