#ifndef SPOTIFY_WIFI_CLIENT
#define SPOTIFY_WIFI_CLIENT

#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <ctime>
#include <string>
#include <vector>

class SpotifyWiFiClient
{

public:
    SpotifyWiFiClient() = delete;
    SpotifyWiFiClient(const SpotifyWiFiClient &) = delete;
    SpotifyWiFiClient &operator=(const SpotifyWiFiClient &) = delete;
    ~SpotifyWiFiClient() = default;

    SpotifyWiFiClient(const String &, const String &);

    const String getInitialAuthorizationToken(const String &, const String &);
    const String getRefreshAuthorizationToken();
    const String getCurrentlyPlayingTrackUrl(const String &);
    const std::vector<std::vector<std::vector<int>>> getPixels(const String &, int, int);

    long oauthExpiryTime = 0;

private:
    bool checkHTTPStatus();
    const std::vector<String> splitHTTPStatus(const String &, const String &);
    bool skipHTTPHeaders();

    WiFiClientSecure httpClient;

    unsigned char applicationIdentifierBase64[128] = {0};
    String oauthRefreshToken = "";

    const uint16_t CONNECTION_PORT = 443;
    const String GET = "GET";
    const String POST = "POST";
    const String HOST = "Host: ";
    const String AUTHORIZATION = "Authorization: ";
    const String HTTP_VERSION = "HTTP/1.1";
    const String CONTENT_TYPE = "Content-Type: ";
    const String CONTENT_LENGTH = "Content-Length: ";
    const String CONNECTION_CLOSE = "Connection: close";
};

#endif // SPOTIFY_WIFI_CLIENT