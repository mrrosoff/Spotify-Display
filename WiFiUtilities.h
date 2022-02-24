#ifndef WIFI_UTILITIES
#define WIFI_UTILITIES

#include <string>

#include <Arduino.h>
#include <SPI.h>
#include <WiFiNINA.h>

class WiFiUtilities {
    
  public:

    WiFiUtilities() = delete;
    WiFiUtilities(const WiFiUtilities &) = delete;
    WiFiUtilities &operator=(const WiFiUtilities &) = delete;
    ~WiFiUtilities() = default;

    WiFiUtilities(const std::string &, const std::string &);

    void checkWiFiModule();
    void connectToWiFi();

  private:

    std::string ssid;
    std::string password;
};

#endif // WIFI_UTILITIES