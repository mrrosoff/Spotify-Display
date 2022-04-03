#include "WiFiUtilities.h"

using namespace std;

WiFiUtilities::WiFiUtilities(const string &ssid, const string &password) : ssid(ssid), password(password) {};

void WiFiUtilities::checkWiFiModule() {
    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("Communication With WiFi Module Failed!");
        while (true);
    }

    String firmwareVersion = WiFi.firmwareVersion();
    if (firmwareVersion < WIFI_FIRMWARE_LATEST_VERSION) {
        Serial.println("Please Upgrade the Firmware");
    }
}

void WiFiUtilities::connectToWiFi() {
    Serial.println("Connecting to Wifi");

    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println(WiFi.status());
        delay(200);
    }

    Serial.print("Connected to Wifi Network: ");
    Serial.println(ssid.c_str());
}
