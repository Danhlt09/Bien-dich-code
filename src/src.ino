#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Chân theo dữ liệu cậu cho
#define TFT_SCLK  5    // SCL = D5
#define TFT_MOSI  18   // SDA = D18
#define TFT_RST   19   // RES = D19
#define TFT_DC    21   // DC = D21
#define TFT_CS    22   // CS = D22
// BLK đã kéo lên 3.3V, không điều khiển bằng GPIO

SPIClass spi = SPIClass(VSPI); // hoặc SPIClass spi = SPIClass(HSPI) tùy board; VSPI thường ổn

Adafruit_ST7735 tft = Adafruit_ST7735(&spi, TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  // Khởi tạo SPI: SCLK, MISO (không dùng -> -1), MOSI, SS (không dùng -> -1)
  spi.begin(TFT_SCLK, -1, TFT_MOSI, -1);

  delay(100);
  // Khởi màn (dùng INITR_MINI160x80 cho 80x160)
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1); // xoay nếu cần
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(2, 2);
  tft.println("TEST");
  Serial.println("Display init done");
}

void loop() {
  // fill vài màu để kiểm tra
  tft.fillScreen(ST77XX_RED);
  delay(600);
  tft.fillScreen(ST77XX_GREEN);
  delay(600);
  tft.fillScreen(ST77XX_BLUE);
  delay(600);
  // Viết 1 dòng debug
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(4,30);
  tft.println("OK?");
  delay(800);
}