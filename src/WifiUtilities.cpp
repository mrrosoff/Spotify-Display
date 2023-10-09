#include "WiFiUtilities.h"

WiFiUtilities::WiFiUtilities(const String &ssid, const String &password) : ssid(ssid), password(password) {};

void WiFiUtilities::connectToWiFi() {
    Serial.print("Connecting to Wifi Network: ");
    Serial.println(ssid.c_str());

    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
    }

    Serial.println("Connected to Wifi Network.");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());
}