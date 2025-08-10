/*
  ESP32 + ST7735S (80x160) — Dino Runner (single-file .ino)
  Pins (the ones you gave):
    SCL  -> GPIO5
    SDA  -> GPIO18
    RES  -> GPIO19
    DC   -> GPIO21
    CS   -> GPIO22
    BLK  -> 3V3 (backlight always on)
    BTN  -> GPIO23 (active LOW, use INPUT_PULLUP)

  Libraries required:
    Adafruit GFX Library
    Adafruit ST7735 and ST7789 Library

  Notes:
    - This is a compact but complete Dino-like runner.
    - Adjust frame timing or speeds if needed.
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ----------------- Pin configuration -----------------
#define TFT_CS     22
#define TFT_RST    19
#define TFT_DC     21
#define TFT_MOSI   18
#define TFT_SCLK   5
#define BTN_PIN    23

// ----------------- Screen object -----------------
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ----------------- Screen layout -----------------
const int SCREEN_W = 160; // width when rotation=1
const int SCREEN_H = 80;
const int GROUND_Y  = 60; // y coordinate of ground baseline

// ----------------- Dino parameters -----------------
const int DINO_X = 18;     // fixed X position
const int DINO_W = 12;
const int DINO_H = 12;
float dinoY;               // top-left Y
float dinoVY;
const float GRAVITY = 0.9f;
const float JUMP_V  = -9.5f;

// ----------------- Obstacles -----------------
struct Obstacle {
  int x;
  int w;
  int h;
  bool active;
};
const int MAX_OBS = 5;
Obstacle obs[MAX_OBS];
int baseSpeed = 3;                 // pixels per frame
unsigned long lastSpawnTime = 0;
unsigned long spawnInterval = 1200; // ms, will decrease over time

// ----------------- Game state -----------------
bool onGround = true;
bool gameOver = false;
unsigned long lastFrame = 0;
const unsigned long FRAME_MS = 30; // ~33 FPS
unsigned long startTime = 0;
unsigned long lastSpeedup = 0;
int score = 0;
int displayScore = 0;

// ----------------- Button / input -----------------
unsigned long lastButtonChange = 0;
int lastButtonState = HIGH;
const unsigned long DEBOUNCE_MS = 40;

// ----------------- Forward declarations -----------------
void resetGame();
void spawnObstacle();
void updatePhysics();
void updateObstacles();
void renderFrame();
void drawDino(int x, int y, bool erase=false);
void drawObstacle(const Obstacle &o, bool erase=false);
void drawGround();
void drawScore();
bool checkCollision(const Obstacle &o);
void eraseDino(int x, int y);
void eraseObstacle(const Obstacle &o);

// ----------------- Helper: random range int -----------------
int rrand(int a, int b) { return a + (rand() % (b - a + 1)); }

// ----------------- Setup -----------------
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BTN_PIN, INPUT_PULLUP);

  // init SPI pins explicitly (MISO not used)
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.initR(INITR_MINI160x80); // for 80x160 displays
  tft.setRotation(1);          // landscape
  tft.fillScreen(ST77XX_BLACK);

  // seed random
  randomSeed(analogRead(34));

  // initial draw
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 8);
  tft.println("Dino Runner");
  tft.setTextSize(1);
  tft.setCursor(10, 22);
  tft.println("Button: GPIO23");
  tft.setCursor(10, 34);
  tft.println("Press to start...");
  delay(800);

  // prepare game
  resetGame();
}

// ----------------- Reset game -----------------
void resetGame() {
  tft.fillScreen(ST77XX_BLACK);
  drawGround();

  // init dino on ground
  dinoY = GROUND_Y - DINO_H;
  dinoVY = 0;
  onGround = true;

  // obstacles clear
  for (int i=0;i<MAX_OBS;i++) obs[i].active = false;

  // timing
  lastSpawnTime = millis();
  spawnInterval = 1200;
  baseSpeed = 3;
  score = 0;
  displayScore = 0;
  startTime = millis();
  lastSpeedup = startTime;
  gameOver = false;

  // initial dino draw
  drawDino(DINO_X, (int)dinoY, false);
  drawScore();
}

// ----------------- Main loop -----------------
void loop() {
  unsigned long now = millis();

  // frame limiter
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;

  // read button (with simple debounce)
  int rawBtn = digitalRead(BTN_PIN);
  if (rawBtn != lastButtonState) {
    lastButtonChange = now;
    lastButtonState = rawBtn;
  }
  bool jumpPressed = false;
  if (now - lastButtonChange > DEBOUNCE_MS) {
    if (rawBtn == LOW) jumpPressed = true;
  }

  // if game over: wait for press to restart
  if (gameOver) {
    if (jumpPressed) {
      resetGame();
    }
    return;
  }

  // start jumping if pressed and on ground
  if (jumpPressed && onGround) {
    dinoVY = JUMP_V;
    onGround = false;
  }

  // update physics, obstacles, render
  updatePhysics();
  updateObstacles();
  renderFrame();

  // collisions
  for (int i=0;i<MAX_OBS;i++) {
    if (obs[i].active) {
      if (checkCollision(obs[i])) {
        // show explosion / game over
        tft.fillRect(DINO_X-2, (int)dinoY-2, DINO_W+4, DINO_H+4, ST77XX_RED);
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(40, 28);
        tft.print("GAME OVER");
        tft.setCursor(36, 40);
        tft.print("Score:");
        tft.print(score/10);
        gameOver = true;
        return;
      }
    }
  }

  // score increment (slower shown)
  score += 1; // internal ticks
  if (score % 10 == 0) displayScore = score / 10;

  // gradually increase difficulty every X ms
  if (now - lastSpeedup > 6000) {
    lastSpeedup = now;
    if (baseSpeed < 8) baseSpeed++;
    if (spawnInterval > 500) spawnInterval = max(500UL, spawnInterval - 80);
  }
}

// ----------------- Update physics -----------------
void updatePhysics() {
  // erase previous dino
  drawDino(DINO_X, (int)dinoY, true);

  // apply physics
  dinoVY += GRAVITY;
  dinoY += dinoVY;
  if (dinoY >= GROUND_Y - DINO_H) {
    dinoY = GROUND_Y - DINO_H;
    dinoVY = 0;
    onGround = true;
  }

  // draw new dino (done in renderFrame)
}

// ----------------- Obstacles update -----------------
void updateObstacles() {
  unsigned long now = millis();

  // spawn new obstacle occasionally
  if (now - lastSpawnTime >= spawnInterval) {
    spawnObstacle();
    lastSpawnTime = now;
  }

  // move obstacles
  for (int i=0;i<MAX_OBS;i++) {
    if (!obs[i].active) continue;
    // erase old
    drawObstacle(obs[i], true);
    obs[i].x -= baseSpeed;
    if (obs[i].x + obs[i].w < 0) {
      obs[i].active = false;
    }
  }
}

// ----------------- Render everything -----------------
void renderFrame() {
  // ground & horizon (quick)
  drawGround();

  // draw obstacles
  for (int i=0;i<MAX_OBS;i++) if (obs[i].active) drawObstacle(obs[i], false);

  // draw dino
  drawDino(DINO_X, (int)dinoY, false);

  // draw score
  drawScore();
}

// ----------------- Spawn obstacle -----------------
void spawnObstacle() {
  int idx = -1;
  for (int i=0;i<MAX_OBS;i++) {
    if (!obs[i].active) { idx = i; break; }
  }
  if (idx == -1) return;

  // randomize size: small cactus (tall thin) or wide low block
  int kind = rrand(0, 2);
  if (kind == 0) { // small cactus
    obs[idx].w = 6 + rrand(0,2);
    obs[idx].h = 12 + rrand(0,4);
  } else if (kind == 1) { // wide rock
    obs[idx].w = 10 + rrand(0,6);
    obs[idx].h = 8 + rrand(0,4);
  } else { // double stack
    obs[idx].w = 8 + rrand(0,4);
    obs[idx].h = 10 + rrand(0,8);
  }
  obs[idx].x = SCREEN_W + rrand(0, 40); // spawn off-screen right
  obs[idx].active = true;
}

// ----------------- Draw/erase dino (small pixel sprite) -----------------
void drawDino(int x, int y, bool erase) {
  uint16_t col = erase ? ST77XX_BLACK : ST77XX_WHITE;

  // simple blocky dino 12x12
  // body
  tft.fillRect(x, y+4, 9, 6, col); // main body
  // head
  tft.fillRect(x+7, y+1, 4, 4, col);
  // tail
  if (!erase && !onGround) {
    // mid-air tail up
    tft.fillRect(x-2, y+3, 2, 3, col);
  } else {
    tft.fillRect(x-1, y+6, 2, 3, col);
  }
  // legs
  if (!erase) {
    if (onGround) {
      tft.fillRect(x+1, y+9, 3, 2, col);
      tft.fillRect(x+5, y+9, 3, 2, col);
    } else {
      // tucked legs
      tft.fillRect(x+3, y+10, 3, 1, col);
    }
  } else {
    // when erasing ensure entire 12x12 cleared
    tft.fillRect(x, y, DINO_W+2, DINO_H+2, col);
  }
}

// ----------------- Draw/erase obstacle -----------------
void drawObstacle(const Obstacle &o, bool erase) {
  uint16_t col = erase ? ST77XX_BLACK : ST77XX_WHITE;
  int oy = GROUND_Y - o.h;
  // basic rectangle obstacle
  tft.fillRect(o.x, oy, o.w, o.h, col);

  // small top spikes when drawing
  if (!erase) {
    if (o.h >= 10) {
      int spikeW = max(2, o.w/3);
      int spikeX = o.x + (o.w - spikeW)/2;
      tft.fillRect(spikeX, oy-2, spikeW, 2, col);
    }
  } else {
    // clear a slightly larger area to avoid artifacts
    tft.fillRect(o.x-1, oy-3, o.w+2, o.h+4, ST77XX_BLACK);
  }
}

// ----------------- Draw ground -----------------
void drawGround() {
  // clear bottom area
  tft.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, ST77XX_BLACK);
  // ground line
  tft.drawFastHLine(0, GROUND_Y, SCREEN_W, ST77XX_WHITE);
  // small dotted pattern
  for (int x = 0; x < SCREEN_W; x += 8) {
    tft.drawPixel(x+2, GROUND_Y+2, ST77XX_WHITE);
  }
}

// ----------------- Draw score -----------------
void drawScore() {
  // top-right small box
  int sx = SCREEN_W - 44;
  int sy = 2;
  tft.fillRect(sx, sy, 42, 12, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(sx+2, sy+1);
  tft.print("Score:");
  tft.print(displayScore);
}

// ----------------- Collision detection (AABB) -----------------
bool checkCollision(const Obstacle &o) {
  int ox1 = o.x;
  int ox2 = o.x + o.w;
  int oy1 = GROUND_Y - o.h;
  int oy2 = GROUND_Y;

  int dx1 = DINO_X;
  int dx2 = DINO_X + DINO_W;
  int dy1 = (int)dinoY;
  int dy2 = (int)dinoY + DINO_H;

  // AABB overlap
  if (dx1 < ox2 && dx2 > ox1 && dy1 < oy2 && dy2 > oy1) return true;
  return false;
}