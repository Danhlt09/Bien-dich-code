#include <TFT_eSPI.h>
#include <SPI.h>

// Cấu hình chân cho ST7735S (định nghĩa tĩnh)
#define TFT_SCLK 5   // SCL
#define TFT_MOSI 18  // SDA
#define TFT_CS   22  // CS
#define TFT_DC   21  // DC
#define TFT_RST  19  // RES
#define TFT_BL   -1  // BLK nối 3V3, không điều khiển

// Cấu hình nút nhấn
#define BUTTON_PIN 23

TFT_eSPI tft = TFT_eSPI();

// Biến trò chơi
int dinoY = 120; // Vị trí Y của khủng long (đáy màn hình)
int dinoVelocity = 0; // Vận tốc nhảy
const int gravity = 1; // Trọng lực
const int jumpPower = -12; // Lực nhảy
int cactusX = 80; // Vị trí X của cây xương rồng
int cactusSpeed = 2; // Tốc độ di chuyển cây xương rồng
int score = 0; // Điểm số
bool gameOver = false;

// Hàm vẽ khủng long
void drawDino() {
  tft.fillRect(10, dinoY, 10, 10, TFT_GREEN);
}

// Hàm vẽ cây xương rồng
void drawCactus() {
  tft.fillRect(cactusX, 120, 5, 10, TFT_RED);
}

// Hàm kiểm tra va chạm
bool checkCollision() {
  if (cactusX >= 10 && cactusX <= 20 && dinoY >= 120) {
    return true;
  }
  return false;
}

void setup() {
  // Khởi tạo màn hình (chân đã được định nghĩa tĩnh)
  tft.init();
  tft.setRotation(1); // Xoay màn hình nếu cần
  tft.fillScreen(TFT_BLACK);
  
  // Cấu hình nút nhấn
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Vẽ đường đất
  tft.drawFastHLine(0, 130, 80, TFT_WHITE);
}

void loop() {
  if (!gameOver) {
    tft.fillRect(0, 0, 80, 130, TFT_BLACK);
    
    if (digitalRead(BUTTON_PIN) == LOW && dinoY == 120) {
      dinoVelocity = jumpPower;
    }
    
    dinoVelocity += gravity;
    dinoY += dinoVelocity;
    
    if (dinoY > 120) {
      dinoY = 120;
      dinoVelocity = 0;
    }
    
    cactusX -= cactusSpeed;
    if (cactusX < -5) {
      cactusX = 80;
      score += 10;
    }
    
    drawDino();
    drawCactus();
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.print("Score: ");
    tft.print(score);
    
    if (checkCollision()) {
      gameOver = true;
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(10, 70);
      tft.print("Game Over!");
      tft.setCursor(10, 80);
      tft.print("Score: ");
      tft.print(score);
    }
    
    delay(50);
  }
}