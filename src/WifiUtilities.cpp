#include "WiFiUtilities.h"

WiFiUtilities::WiFiUtilities(const String &ssid, const String &password) : ssid(ssid), password(password) {};

void WiFiUtilities::connectToWiFi() {
    Serial.print("Connecting to Wifi Network: ");
    Serial.println(ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    int timeout_counter = 0;

    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        timeout_counter++;
        if (timeout_counter >= 50) {
            ESP.restart();
        }
    }

    Serial.print("Connected to Wifi Network. ");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());
}