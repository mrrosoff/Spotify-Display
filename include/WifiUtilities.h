#ifndef WIFI_UTILITIES
#define WIFI_UTILITIES

#include <Arduino.h>
#include <WiFi.h>

class WiFiUtilities
{

public:
    WiFiUtilities() = delete;
    WiFiUtilities(const WiFiUtilities &) = delete;
    WiFiUtilities &operator=(const WiFiUtilities &) = delete;
    ~WiFiUtilities() = default;

    WiFiUtilities(const String &, const String &);

    void connectToWiFi();

private:
    String ssid;
    String password;
};

#endif // WIFI_UTILITIES