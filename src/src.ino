#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// Pin config
#define TFT_CS    22
#define TFT_RST   19
#define TFT_DC    21
#define TFT_MOSI  18
#define TFT_SCLK  5
#define BTN_PIN   23

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// Screen layout
const int SCR_W = 160;
const int SCR_H = 80;
const int GROUND_Y = 60;

// Dino parameters
const int DINO_X = 18;
const int DINO_W = 12;
const int DINO_H = 12;
float dinoY = GROUND_Y - DINO_H;
float dinoV = 0;
const float GRAVITY = 0.95f;
const float JUMP_V = -9.0f;
bool onGround = true;
int prevDinoY = -1;

// Obstacles
struct Ob {
  int x;
  int w;
  int h;
  bool active;
  int prevX;
};
const int MAX_OBS = 4;
Ob obs[MAX_OBS];
int baseSpeed = 3;
unsigned long lastSpawn = 0;
unsigned long spawnInterval = 1000;

// Game state
bool gameOver = false;
unsigned long lastFrame = 0;
const unsigned long FRAME_MS = 28;
int score = 0;
int displayScore = 0;

// Input debounce
int lastBtn = HIGH;
unsigned long lastBtnTime = 0;
const unsigned long DEBOUNCE = 40;

// Utility random
int rrand(int a, int b) { return a + (rand() % (b - a + 1)); }

// Drawing routines
inline void fillBGRect(int x, int y, int w, int h) {
  if (x + w < 0 || x > SCR_W || y + h < 0 || y > SCR_H) return;
  tft.fillRect(x, y, w, h, ST77XX_BLACK);
}

void drawDinoAt(int y, uint16_t color) {
  tft.fillRect(DINO_X, y + 4, 9, 6, color);
  tft.fillRect(DINO_X + 7, y + 1, 4, 4, color);
  tft.fillRect(DINO_X - 2, y + 3, 2, 3, color);
  tft.fillRect(DINO_X + 1, y + 9, 3, 2, color);
  tft.fillRect(DINO_X + 5, y + 9, 3, 2, color);
}

void eraseDino(int oldY) {
  fillBGRect(DINO_X - 3, oldY - 2, DINO_W + 8, DINO_H + 6);
}

void drawObstacle(const Ob &o, uint16_t color) {
  int oy = GROUND_Y - o.h;
  tft.fillRect(o.x, oy, o.w, o.h, color);
  if (o.w >= 6 && color != ST77XX_BLACK) {
    int sx = o.x + (o.w / 2) - 1;
    tft.fillRect(sx, oy - 2, 3, 2, color);
  }
}

void eraseObstacle(const Ob &o) {
  if (o.prevX != o.x) {
    int oy = GROUND_Y - o.h;
    fillBGRect(o.prevX, oy - 2, o.w, o.h + 4);
  }
}

void drawGround() {
  tft.drawFastHLine(0, GROUND_Y, SCR_W, ST77XX_WHITE);
  for (int x = 0; x < SCR_W; x += 8) {
    tft.drawPixel(x + 2, GROUND_Y + 2, ST77XX_WHITE);
  }
}

void drawScore() {
  int sx = SCR_W - 46;
  tft.fillRect(sx, 2, 44, 12, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(sx + 2, 3);
  tft.print("S:");
  tft.print(displayScore);
}

void resetGame() {
  tft.fillScreen(ST77XX_BLACK);
  drawGround();
  dinoY = GROUND_Y - DINO_H;
  dinoV = 0;
  onGround = true;
  prevDinoY = (int)dinoY;
  for (int i = 0; i < MAX_OBS; i++) {
    obs[i].active = false;
    obs[i].prevX = SCR_W + 10;
  }
  lastSpawn = millis();
  spawnInterval = 1000;
  baseSpeed = 3;
  score = 0;
  displayScore = 0;
  gameOver = false;
  drawDinoAt((int)dinoY, ST77XX_WHITE);
  drawScore();
}

void spawnOne() {
  int idx = -1;
  for (int i = 0; i < MAX_OBS; i++) if (!obs[i].active) { idx = i; break; }
  if (idx == -1) return;
  int kind = rrand(0, 2);
  if (kind == 0) { obs[idx].w = 6 + rrand(0, 2); obs[idx].h = 12 + rrand(0, 2); }
  else if (kind == 1) { obs[idx].w = 10 + rrand(0, 4); obs[idx].h = 8 + rrand(0, 3); }
  else { obs[idx].w = 8 + rrand(0, 4); obs[idx].h = 10 + rrand(0, 4); }
  obs[idx].x = SCR_W + rrand(0, 30);
  obs[idx].active = true;
  obs[idx].prevX = obs[idx].x;
}

bool collideWith(const Ob &o) {
  int dx1 = DINO_X + 1;
  int dx2 = DINO_X + DINO_W - 1;
  int dy1 = (int)dinoY + 2;
  int dy2 = (int)dinoY + DINO_H - 1;
  int ox1 = o.x;
  int ox2 = o.x + o.w;
  int oy1 = GROUND_Y - o.h;
  int oy2 = GROUND_Y;
  if (dx1 < ox2 && dx2 > ox1 && dy1 < oy2 && dy2 > oy1) {
    Serial.printf("Collision detected! Dino: (%d,%d)-(%d,%d), Obs: (%d,%d)-(%d,%d)\n",
                  dx1, dy1, dx2, dy2, ox1, oy1, ox2, oy2);
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(40000000);

  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setSPISpeed(40000000);

  if (!tft.initR(INITR_MINI160x80)) {
    Serial.println("TFT init failed!");
    while (1);
  }

  randomSeed(analogRead(34));
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 8);
  tft.print("Dino (Optimized)");
  tft.setCursor(10, 20);
  tft.print("BTN: GPIO23, Press to start");
  delay(800);

  resetGame();
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame += FRAME_MS;

  int b = digitalRead(BTN_PIN);
  bool pressed = false;
  static bool wasPressed = false;
  if (b != lastBtn) {
    lastBtnTime = now;
    lastBtn = b;
  }
  if (now - lastBtnTime > DEBOUNCE && b == LOW && !wasPressed) {
    pressed = true;
    wasPressed = true;
  }
  if (b == HIGH) wasPressed = false;

  if (gameOver) {
    if (pressed) {
      resetGame();
    }
    return;
  }

  if (pressed && onGround) {
    dinoV = JUMP_V;
    onGround = false;
  }

  int curDinoY = (int)dinoY;
  if (prevDinoY != curDinoY) {
    eraseDino(prevDinoY);
  }

  dinoV += GRAVITY;
  dinoY += dinoV;
  if (dinoY >= GROUND_Y - DINO_H) {
    dinoY = GROUND_Y - DINO_H;
    dinoV = 0;
    onGround = true;
  }
  curDinoY = (int)dinoY;

  for (int i = 0; i < MAX_OBS; i++) {
    if (!obs[i].active) continue;
    eraseObstacle(obs[i]);
    obs[i].prevX = obs[i].x;
    obs[i].x -= baseSpeed;
    if (obs[i].x + obs[i].w < -10) {
      obs[i].active = false;
    }
  }

  if (now - lastSpawn >= spawnInterval) {
    spawnOne();
    lastSpawn = now;
    spawnInterval = 700 + rrand(0, 700);
  }

  for (int i = 0; i < MAX_OBS; i++) if (obs[i].active) drawObstacle(obs[i], ST77XX_WHITE);

  tft.drawFastHLine(0, GROUND_Y, SCR_W, ST77XX_WHITE);

  drawDinoAt(curDinoY, ST77XX_WHITE);

  static int lastDisplayScore = -1;
  score += 1;
  displayScore = score / 4;
  if (displayScore != lastDisplayScore) {
    drawScore();
    lastDisplayScore = displayScore;
  }

  for (int i = 0; i < MAX_OBS; i++) {
    if (obs[i].active && collideWith(obs[i])) {
      tft.fillRect(DINO_X - 2, (int)dinoY - 2, DINO_W + 6, DINO_H + 6, ST77XX_RED);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(1);
      tft.setCursor(42, 28);
      tft.print("GAME OVER");
      tft.setCursor(36, 40);
      tft.print("Score:");
      tft.print(displayScore);
      gameOver = true;
      return;
    }
  }

  static unsigned long lastRamp = 0;
  if (millis() - lastRamp > 6000) {
    lastRamp = millis();
    if (baseSpeed < 9) baseSpeed++;
    if (spawnInterval > 500) spawnInterval = max(500UL, spawnInterval - 60);
  }

  prevDinoY = curDinoY;
  SPI.endTransaction();
}