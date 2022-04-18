#ifndef SPOTIFY_WIFI_CLIENT
#define SPOTIFY_WIFI_CLIENT

#include <string>
#include <vector>

#include <SPI.h>
#include <WiFiNINA.h>

class SpotifyWiFiClient {
  
  public:

    SpotifyWiFiClient() = delete;
    SpotifyWiFiClient(const SpotifyWiFiClient &) = delete;
    SpotifyWiFiClient &operator=(const SpotifyWiFiClient &) = delete;
    ~SpotifyWiFiClient() = default;

    SpotifyWiFiClient(const std::string &, const std::string &, const std::string &);

    void getOAuthToken();
    void getAlbumArtURL();
    void getPixels();

    bool checkHTTPStatus();
    std::vector<std::string> splitHTTPStatus(const std::string &, const std::string &);
    bool skipHTTPHeaders();

    void nextClient();

    enum ClientType {OAUTH, ALBUM_URL, PIXELS};
    ClientType currentClient = ClientType::OAUTH;

    std::string oauthToken;
    std::string albumArtURL;

    long oauthExpiryTime = 0;
    long nextAlbumArtRetrievalTime = 0;
    long nextDisplayUpdate = 0;

    WiFiSSLClient SSLClient;

  private:

    std::string spotifyApplicationIdentifier;
    std::string oauthPostData;
    unsigned char base64[128] = {0};
    
    const uint16_t CONNECTION_PORT = 443;
    const std::string GET = "GET";
    const std::string POST = "POST";
    const std::string HOST = "Host: ";
    const std::string AUTHORIZATION = "Authorization: ";
    const std::string HTTP_VERSION = "HTTP/1.1";
    const std::string CONTENT_TYPE = "Content-Type: ";
    const std::string CONTENT_LENGTH = "Content-Length: ";
    const std::string CONNECTION_CLOSE = "Connection: close";
};


#endif // SPOTIFY_WIFI_CLIENT
