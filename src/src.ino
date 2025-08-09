#include <WiFi.h>

// Tên và mật khẩu Wi-Fi mà ESP32 sẽ phát

const char* ssid = "ESP32-WiFi";

const char* password = "12345678";

// Tạo web server ở cổng 80

WiFiServer server(80);

void setup() {

  Serial.begin(115200);

  // Bật chế độ Access Point

  WiFi.softAP(ssid, password);

  Serial.println("Wi-Fi Access Point đã bật!");

  Serial.print("Tên mạng: ");

  Serial.println(ssid);

  Serial.print("IP Address: ");

  Serial.println(WiFi.softAPIP());

  // Khởi động server

  server.begin();

}

void loop() {

  WiFiClient client = server.available();   // Chờ client kết nối

  if (client) {

    Serial.println("Có thiết bị kết nối.");

    String request = client.readStringUntil('\r');

    Serial.println(request);

    // Gửi phản hồi HTML

    client.println("HTTP/1.1 200 OK");

    client.println("Content-type:text/html");

    client.println();

    client.println("<h1>ESP32 đang phát Wi-Fi!</h1>");

    client.println("<p>Kết nối thành công!</p>");

    client.println();

    client.stop();

  }

}