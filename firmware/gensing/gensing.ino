#include <SPI.h>
#include <TFT_eSPI.h>
#include "SD_MMC.h"

TFT_eSPI tft = TFT_eSPI(); 

// ================== CẤU HÌNH MÀN HÌNH ==================
const int SCREEN_W = 240;
const int SCREEN_H = 320;

#define TFT_BL 39 // Chân đèn nền LEDK

// Nút bấm
#define KEY_UP 7
#define KEY_DOWN 46
#define KEY_LEFT 45
#define KEY_RIGHT 6
#define KEY_MENU 18
#define KEY_OPTION 8
#define KEY_SELECT 16
#define KEY_START 17
#define KEY_A 15
#define KEY_B 5

// ================== BIẾN TRẠNG THÁI ==================
enum GameState {
  SPLASH_SCREEN,
  EXPLORE_MODE,
  BATTLE_MODE,
  MAIN_MENU
};
GameState currentState = SPLASH_SCREEN;

// Dữ liệu người chơi
int playerX = 112, playerY = 200;
int hp = 100, max_hp = 100;
int energy = 0, max_energy = 100;
int level = 1, exp_pts = 0, mora = 500;
int potions = 5;

String currentElement = "Anemo";
String characters[6] = {"Traveler", "Paimon", "Raven", "Mona", "Zhongli", "Venti"};
bool mapDrawn = false;

// Dữ liệu Battle
int enemyHP = 50;
int battleCursor = 0; 
bool playerTurn = true;

// Dữ liệu Menu
int menuCursor = 0; 

// Debounce chống rung phím
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200; 

// ================== HÀM HỖ TRỢ ==================

void setupButtons() {
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_LEFT, INPUT_PULLUP);
  pinMode(KEY_RIGHT, INPUT_PULLUP);
  pinMode(KEY_MENU, INPUT_PULLUP);
  pinMode(KEY_OPTION, INPUT_PULLUP);
  pinMode(KEY_SELECT, INPUT_PULLUP);
  pinMode(KEY_START, INPUT_PULLUP);
  pinMode(KEY_A, INPUT_PULLUP);
  pinMode(KEY_B, INPUT_PULLUP);
}

// Hàm kiểm tra nút nhấn có debounce
bool isPressed(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    if ((millis() - lastDebounceTime) > debounceDelay) {
      lastDebounceTime = millis();
      return true;
    }
  }
  return false;
}

void drawHUD() {
  tft.fillRect(0, 0, SCREEN_W, 24, TFT_DARKGREY);
  
  int hpBarWidth = 50;
  int hpPercent = (hp * 100) / max_hp;
  tft.drawRect(3, 3, hpBarWidth + 2, 8, TFT_BLACK);
  tft.fillRect(4, 4, (hpPercent * hpBarWidth) / 100, 6, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, 13);
  tft.printf("HP:%d/%d", hp, max_hp);
  
  int mpBarWidth = 50;
  int mpPercent = (energy * 100) / max_energy;
  tft.drawRect(60, 3, mpBarWidth + 2, 8, TFT_BLACK);
  tft.fillRect(61, 4, (mpPercent * mpBarWidth) / 100, 6, TFT_CYAN);
  tft.setCursor(60, 13);
  tft.printf("MP:%d/%d", energy, max_energy);
  
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(115, 4);
  tft.printf("LV:%d", level);
  tft.setCursor(115, 13);
  tft.printf("M:%d", mora);
}

// ================== HÀM SETUP CHÍNH ==================

void setup() {
  Serial.begin(115200);
  delay(500); // Chờ ổn định nguồn mạch sau khi boot

  // 1. Kích hoạt Đèn nền thủ công (Sửa lỗi màn hình tối đen)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // 2. Khởi tạo Nút bấm
  setupButtons();

  // 3. Khởi tạo SD Card qua chuẩn SDIO 1-bit mode (DAT0: 9, CMD: 11, CLK: 13)
  SD_MMC.setPins(13, 11, 9);
  if(!SD_MMC.begin("/sdcard", true)){ 
    Serial.println("Warning: SD Card Mount Failed!");
  } else {
    Serial.println("SD Card Initialized.");
  }

  // 4. Khởi tạo màn hình
  tft.init(); 
  tft.setRotation(0); 
  tft.fillScreen(TFT_BLACK);
  
  drawSplashScreen();
}

// ================== HÀM VÒNG LẶP ==================

void loop() {
  switch (currentState) {
    case SPLASH_SCREEN:
      handleSplashScreen();
      break;
    case EXPLORE_MODE:
      handleExploreMode();
      break;
    case BATTLE_MODE:
      handleBattleMode();
      break;
    case MAIN_MENU:
      handleMainMenu();
      break;
  }
}

// ================== LOGIC CÁC TRẠNG THÁI ==================

void drawSplashScreen() {
  tft.fillScreen(TFT_BLACK);
  
  for (int i = 0; i < 30; i++) {
    tft.drawPixel(random(SCREEN_W), random(SCREEN_H), TFT_DARKGREY);
  }
  
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(30, 50);
  tft.print("GENSHIN");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(40, 75);
  tft.print("IMPACT");
  
  tft.drawRect(30, 95, 180, 2, TFT_CYAN);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(80, 110);
  tft.print("miHoYo");
  
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setCursor(60, 130);
  tft.print("InnovaBoard");
  
  tft.fillRect(30, 160, 180, 25, TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(45, 168);
  tft.print("Press START");
  
  for (int i = 0; i < 8; i++) {
    tft.fillCircle(random(SCREEN_W), random(30, 150), random(1, 3), TFT_WHITE);
  }
}

void handleSplashScreen() {
  if (isPressed(KEY_START)) {
    currentState = EXPLORE_MODE;
    mapDrawn = false;
    playerX = SCREEN_W / 2 - 6;
    playerY = SCREEN_H / 2;
    tft.fillScreen(TFT_DARKGREEN);
    drawMap();
    drawHUD();
  }
}

void handleExploreMode() {
  static int lastX = SCREEN_W / 2 - 6, lastY = SCREEN_H / 2;
  
  if (!mapDrawn) {
    drawMap();
    mapDrawn = true;
  }
  
  tft.fillRect(lastX, lastY, 12, 12, TFT_DARKGREEN);

  if (isPressed(KEY_UP)) playerY -= 8;
  if (isPressed(KEY_DOWN)) playerY += 8;
  if (isPressed(KEY_LEFT)) playerX -= 8;
  if (isPressed(KEY_RIGHT)) playerX += 8;
  
  if (playerX < 0) playerX = 0;
  if (playerX > SCREEN_W - 12) playerX = SCREEN_W - 12;
  if (playerY < 26) playerY = 26; 
  if (playerY > SCREEN_H - 14) playerY = SCREEN_H - 14;

  drawPlayer(playerX, playerY);
  lastX = playerX;
  lastY = playerY;

  if (isPressed(KEY_UP) || isPressed(KEY_DOWN) || isPressed(KEY_LEFT) || isPressed(KEY_RIGHT)) {
    if (random(0, 20) == 0) { 
      currentState = BATTLE_MODE;
      enemyHP = 50 + (level * 10);
      drawBattleScreen();
      return;
    }
  }

  if (isPressed(KEY_MENU)) {
    currentState = MAIN_MENU;
    menuCursor = 0;
    drawMainMenu();
  }
}

void drawMap() {
  tft.fillScreen(TFT_DARKGREEN);
  
  for (int i = 0; i < 10; i++) {
    tft.fillCircle(random(0, SCREEN_W), random(30, SCREEN_H - 20), random(3, 6), TFT_DARKGREY);
  }
  for (int i = 0; i < 15; i++) {
    int x = random(0, SCREEN_W);
    int y = random(30, SCREEN_H - 20);
    tft.fillRect(x, y, 3, 6, TFT_GREEN);
    tft.fillRect(x-1, y+1, 5, 3, TFT_GREEN);
  }
  for (int i = 0; i < 5; i++) {
    tft.fillCircle(random(0, SCREEN_W), random(40, SCREEN_H - 30), random(5, 10), TFT_BLUE);
  }
  for (int i = 0; i < 4; i++) {
    int x = random(10, SCREEN_W - 20);
    int y = random(50, SCREEN_H - 40);
    tft.fillRect(x, y, 10, 8, TFT_DARKGREY);
    tft.fillRect(x+1, y-3, 8, 3, TFT_DARKGREY);
  }
}

void drawPlayer(int x, int y) {
  tft.fillCircle(x + 6, y + 4, 4, TFT_WHITE);
  tft.fillCircle(x + 6, y + 2, 2, TFT_PINK);
  tft.fillRect(x + 2, y + 6, 8, 6, TFT_BLUE);
  tft.fillCircle(x + 3, y + 8, 2, TFT_DARKGREY);
  tft.fillCircle(x + 9, y + 8, 2, TFT_DARKGREY);
}

void restoreMapBackground(int x, int y) {
  tft.fillRect(x, y, 12, 12, TFT_DARKGREEN);
  for (int i = 0; i < 3; i++) {
    int rx = x + random(0, 12);
    int ry = y + random(0, 12);
    if (rx >= x && rx < x + 12 && ry >= y && ry < y + 12) {
      tft.drawPixel(rx, ry, TFT_DARKGREY);
    }
  }
}

// --- LOGIC MENU ---
void drawMainMenu() {
  tft.fillScreen(TFT_NAVY);
  
  tft.fillRect(8, 8, SCREEN_W - 16, 28, TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setCursor(15, 14);
  tft.print("PAIMON MENU");
  
  tft.fillRect(10, 40, SCREEN_W - 20, SCREEN_H - 50, TFT_BLACK);
  tft.drawRect(10, 40, SCREEN_W - 20, SCREEN_H - 50, TFT_CYAN);
  
  drawPaimon(SCREEN_W - 50, 60);
  
  updateMainMenu();
}

void drawPaimon(int x, int y) {
  tft.fillCircle(x + 10, y + 5, 10, TFT_WHITE);
  tft.fillCircle(x + 7, y + 7, 3, TFT_BLACK);
  tft.fillCircle(x + 13, y + 7, 3, TFT_BLACK);
  tft.fillCircle(x + 10, y + 9, 2, TFT_PINK);
  tft.fillRect(x + 3, y + 12, 14, 18, TFT_WHITE);
  tft.fillRect(x + 8, y + 30, 4, 10, TFT_WHITE);
  tft.fillCircle(x + 5, y + 38, 3, TFT_WHITE);
  tft.fillCircle(x + 15, y + 38, 3, TFT_WHITE);
  tft.fillRect(x + 7, y + 16, 6, 5, TFT_PINK);
}

void updateMainMenu() {
  tft.fillRect(15, 55, SCREEN_W - 30, 130, TFT_BLACK); 
  String menuOptions[4] = {"1.Char", "2.Item", "3.Set", "4.Back"};
  tft.setTextSize(1);
  
  for (int i = 0; i < 4; i++) {
    if (i == menuCursor) {
      tft.setTextColor(TFT_YELLOW);
      tft.setCursor(20, 60 + (i * 28));
      tft.print(">" + menuOptions[i]);
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(20, 60 + (i * 28));
      tft.print(" " + menuOptions[i]);
    }
  }
}

void handleMainMenu() {
  if (isPressed(KEY_UP)) {
    menuCursor--;
    if (menuCursor < 0) menuCursor = 3;
    updateMainMenu();
  }
  if (isPressed(KEY_DOWN)) {
    menuCursor++;
    if (menuCursor > 3) menuCursor = 0;
    updateMainMenu();
  }
  if (isPressed(KEY_START)) {
    if (menuCursor == 3) {
      currentState = EXPLORE_MODE;
      mapDrawn = false;
      tft.fillScreen(TFT_DARKGREEN);
      drawMap();
      drawHUD();
    } else {
      // In ra thông báo tính năng đang phát triển
      tft.fillRect(180, 60, 120, 150, TFT_BLACK);
      tft.setTextColor(TFT_RED);
      tft.setTextSize(1);
      tft.setCursor(180, 60);
      tft.print("Work In Progress...");
    }
  }
}

// --- LOGIC BATTLE ---
void drawBattleScreen() {
  tft.fillScreen(TFT_NAVY);
  
  tft.fillRect(0, 0, SCREEN_W, 22, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 5);
  tft.print("BATTLE!");
  
  tft.drawRect(5, 26, 70, 60, TFT_WHITE);
  tft.fillRect(10, 30, 25, 25, TFT_GREEN);
  tft.fillCircle(22, 40, 10, TFT_LIGHTGREY);
  tft.fillRect(10, 50, 20, 10, TFT_GREEN);
  tft.fillCircle(14, 60, 3, TFT_DARKGREY);
  tft.fillCircle(30, 60, 3, TFT_DARKGREY);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(8, 90);
  tft.printf("Slime Lv%d", level);
  
  int enemyHpBarWidth = 60;
  int enemyMaxHp = 50 + level * 10;
  int enemyHpPercent = (enemyHP * 100) / enemyMaxHp;
  tft.drawRect(5, 95, enemyHpBarWidth + 2, 8, TFT_WHITE);
  tft.fillRect(6, 96, (enemyHpPercent * enemyHpBarWidth) / 100, 6, TFT_RED);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(8, 106);
  tft.printf("HP:%d", enemyHP);
  
  tft.drawRect(SCREEN_W - 80, 26, 75, 75, TFT_WHITE);
  tft.fillRect(SCREEN_W - 75, 35, 20, 20, TFT_BLUE);
  tft.fillCircle(SCREEN_W - 65, 40, 5, TFT_WHITE);
  tft.fillCircle(SCREEN_W - 67, 38, 2, TFT_BLACK);
  tft.fillRect(SCREEN_W - 77, 55, 24, 18, TFT_NAVY);
  tft.fillCircle(SCREEN_W - 73, 73, 3, TFT_DARKGREY);
  tft.fillCircle(SCREEN_W - 57, 73, 3, TFT_DARKGREY);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(SCREEN_W - 75, 108);
  tft.print(characters[0]);
  
  int playerHpBarWidth = 65;
  int playerHpPercent = (hp * 100) / max_hp;
  tft.drawRect(SCREEN_W - 78, 115, playerHpBarWidth + 2, 8, TFT_WHITE);
  tft.fillRect(SCREEN_W - 77, 116, (playerHpPercent * playerHpBarWidth) / 100, 6, TFT_GREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(SCREEN_W - 75, 126);
  tft.printf("HP:%d/%d", hp, max_hp);
  
  int playerMpBarWidth = 65;
  int playerMpPercent = (energy * 100) / max_energy;
  tft.drawRect(SCREEN_W - 78, 132, playerMpBarWidth + 2, 8, TFT_WHITE);
  tft.fillRect(SCREEN_W - 77, 133, (playerMpPercent * playerMpBarWidth) / 100, 6, TFT_CYAN);
  tft.setCursor(SCREEN_W - 75, 143);
  tft.printf("MP:%d/%d", energy, max_energy);

  updateBattleMenu();
}

void updateBattleMenu() {
  tft.fillRect(5, 150, SCREEN_W - 10, 45, TFT_DARKGREY);
  tft.setTextSize(1);
  String options[4] = {"Atk", "Sk", "Bt", "Pot"};
  for (int i = 0; i < 4; i++) {
    int xPos = 10 + (i * 55);
    if (i == battleCursor) {
      tft.setTextColor(TFT_YELLOW);
      tft.fillRect(xPos - 2, 155, 50, 20, TFT_BLACK);
      tft.setCursor(xPos, 158);
      tft.print(">" + options[i]);
    } else {
      tft.setTextColor(TFT_WHITE);
      tft.setCursor(xPos, 158);
      tft.print(" " + options[i]);
    }
  }
}

void handleBattleMode() {
  if (playerTurn) {
    if (isPressed(KEY_UP)) {
      battleCursor--;
      if (battleCursor < 0) battleCursor = 3;
      updateBattleMenu();
    }
    if (isPressed(KEY_DOWN)) {
      battleCursor++;
      if (battleCursor > 3) battleCursor = 0;
      updateBattleMenu();
    }

    if (isPressed(KEY_START)) {
      tft.fillRect(5, 150, SCREEN_W - 10, 45, TFT_BLACK); 
      tft.setTextColor(TFT_YELLOW);
      tft.setTextSize(1);
      tft.setCursor(10, 155);

      if (battleCursor == 0) { 
        int dmg = 10 + random(0, 5);
        enemyHP -= dmg;
        energy += 10;
        tft.printf("ATK! -%d HP", dmg);
      } 
      else if (battleCursor == 1) { 
        int dmg = 25;
        enemyHP -= dmg;
        energy += 20;
        tft.setTextColor(TFT_CYAN);
        tft.printf("%s SKILL!", currentElement.c_str());
        tft.setTextColor(TFT_YELLOW);
        tft.setCursor(10, 170);
        tft.printf("-%d HP", dmg);
      }
      else if (battleCursor == 2) { 
        if (energy >= 50) {
          int dmg = 60;
          enemyHP -= dmg;
          energy -= 50;
          tft.setTextColor(TFT_MAGENTA);
          tft.print("BURST!");
          tft.setTextColor(TFT_YELLOW);
          tft.setCursor(10, 170);
          tft.printf("-%d HP", dmg);
        } else {
          tft.setTextColor(TFT_RED);
          tft.print("No MP!");
          delay(1000);
          return; 
        }
      }
      else if (battleCursor == 3) { 
        if (potions > 0) {
          hp += 50;
          if (hp > max_hp) hp = max_hp;
          potions--;
          tft.setTextColor(TFT_GREEN);
          tft.print("Potion!");
          tft.setTextColor(TFT_YELLOW);
          tft.setCursor(10, 170);
          tft.printf("+50 HP (%d)", potions);
        } else {
          tft.setTextColor(TFT_RED);
          tft.print("No Pot!");
          delay(1000);
          return;
        }
      }

      if (energy > max_energy) energy = max_energy;
      delay(1000); 
      
      if (enemyHP <= 0) {
        int wonMora = random(20, 50);
        int wonExp = 25;
        tft.fillScreen(TFT_DARKGREEN);
        tft.setTextColor(TFT_YELLOW);
        tft.setTextSize(2);
        tft.setCursor(50, 50);
        tft.print("VICTORY!");
        
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(1);
        tft.setCursor(40, 90);
        tft.printf("+%d Mora", wonMora);
        tft.setCursor(40, 110);
        tft.printf("+%d EXP", wonExp);
        
        if (exp_pts >= 100) {
          tft.setTextColor(TFT_CYAN);
          tft.setCursor(50, 140);
          tft.print("LEVEL UP!");
        }
        
        mora += wonMora;
        exp_pts += wonExp;
        if (exp_pts >= 100) { 
           level++;
           max_hp += 20;
           hp = max_hp;
           exp_pts = 0;
        }
        delay(2000);
        currentState = EXPLORE_MODE;
        mapDrawn = false;
        tft.fillScreen(TFT_DARKGREEN);
        drawMap();
        drawHUD();
        return;
      }
      playerTurn = false;
    }
  } else {
    tft.fillRect(5, 150, SCREEN_W - 10, 45, TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1);
    tft.setCursor(10, 155);
    int enemyDmg = 5 + random(0, 10);
    hp -= enemyDmg;
    tft.print("ENEMY ATK!");
    tft.setCursor(10, 170);
    tft.printf("-%d HP", enemyDmg);
    
    if (hp <= 0) {
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.setCursor(60, 80);
      tft.print("GAME OVER");
      tft.setTextSize(1);
      tft.setCursor(50, 120);
      tft.print("Press START");
      delay(3000);
      hp = max_hp;
      potions = 5;
      level = 1;
      exp_pts = 0;
      mora = 500;
      currentState = SPLASH_SCREEN;
      return;
    }
    
    delay(1000);
    playerTurn = true;
    drawBattleScreen(); 
  }
}