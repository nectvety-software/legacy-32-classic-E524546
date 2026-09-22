// main.cpp — E524546-OS: Arduino setup/loop chạy Lua (lõi LuaS30) trên ESP32-S3 + ST7789 + 10 nút + SD.
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <SPI.h>

#include "pins.h"
#include "LGFX_ESP32S3_ST7789.h"
#include "engine_esp32.h"

static LGFX_ESP32S3_ST7789 lcd;

// Keypad Symbian 3x3 + SELECT theo system_prompt_phan_cung.md.
// Che do Game (mac dinh): ten hanh dong. Che do T9: so "1".."9","0".
// Giu KEY_SELECT > 600ms: doi che do + phat su kien "mode".
struct KeyDef { uint8_t pin; const char* game; const char* t9; };
static const KeyDef KEYS[] = {
  { KEY_MENU,   "menu",   "1" },  // Top Left    -> Home
  { KEY_UP,     "up",     "2" },  // Top Center  -> Cursor Up
  { KEY_A,      "back",   "3" },  // Top Right   -> Back / Cancel
  { KEY_LEFT,   "left",   "4" },  // Mid Left    -> Cursor Left
  { KEY_START,  "ok",     "5" },  // Center      -> Select / Enter
  { KEY_RIGHT,  "right",  "6" },  // Mid Right   -> Cursor Right
  { KEY_OPTION, "option", "7" },  // Bot Left    -> Options / Context
  { KEY_DOWN,   "down",   "8" },  // Mid Bot     -> Cursor Down
  { KEY_B,      "delete", "9" },  // Bot Right   -> Delete / Backspace
  { KEY_SELECT, "mode",   "0" },  // Bottom      -> Giu: doi Game/T9
};
static const int NKEYS = sizeof(KEYS) / sizeof(KEYS[0]);
static bool last_state[NKEYS];
static unsigned long last_change[NKEYS];
static unsigned long press_start[NKEYS];
static bool t9mode = false;
static const unsigned long MODE_HOLD_MS = 600;

static void keys_init() {
  for (int i = 0; i < NKEYS; i++) {
    pinMode(KEYS[i].pin, INPUT_PULLUP);
    last_state[i] = HIGH;
    last_change[i] = 0;
    press_start[i] = 0;
  }
}

// Debounce 25ms, nhấn = LOW.
static void keys_poll() {
  unsigned long now = millis();
  for (int i = 0; i < NKEYS; i++) {
    bool v = digitalRead(KEYS[i].pin);
    if (v == last_state[i]) continue;
    if (now - last_change[i] <= 25) continue;
    last_state[i] = v;
    last_change[i] = now;
    bool is_select = (KEYS[i].pin == KEY_SELECT);
    if (v == LOW) {
      press_start[i] = now;  // bat dau nhan
      if (!is_select) {
        engine_key_event(t9mode ? KEYS[i].t9 : KEYS[i].game, true);
      }
      // KEY_SELECT chi phat khi nha (can do thoi gian giu)
    } else {
      unsigned long held = now - press_start[i];
      if (is_select) {
        if (held >= MODE_HOLD_MS) {
          t9mode = !t9mode;
          Serial.printf("[key] che do: %s\n", t9mode ? "T9" : "GAME");
          engine_key_event("mode", true);
          engine_key_event("mode", false);
        } else if (t9mode) {
          engine_key_event("0", true);
          engine_key_event("0", false);
        } else {
          engine_key_event("mode", true);
          engine_key_event("mode", false);
        }
      } else {
        engine_key_event(t9mode ? KEYS[i].t9 : KEYS[i].game, false);
      }
    }
  }
}

static void sd_init() {
  // SDMMC 1-bit: CLK=13 CMD=11 D0=9 (đúng BOM của user)
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_DAT0);
  if (!SD_MMC.begin("/sd", true)) {
    Serial.println("[sys] SD_MMC mount failed (chay tiep khong SD)");
  } else {
    Serial.printf("[sys] SD OK type=%d size=%lluMB\n", SD_MMC.cardType(), SD_MMC.cardSize() / 1048576ULL);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nE524546-OS boot (LuaS30-core on ESP32-S3)");

  keys_init();

  lcd.init();
  lcd.setRotation(0);          // 240x320 dọc
  lcd.setBrightness(255);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextSize(1);
  lcd.setCursor(4, 4);
  lcd.print("E524546-OS booting...");

  if (!LittleFS.begin(true)) {
    Serial.println("[sys] LittleFS mount failed");
    lcd.setCursor(4, 20);
    lcd.print("LittleFS FAIL");
    delay(2000);
  }

  sd_init();

  engine_init(&lcd);
  engine_open_vm();  // đọc conf.lua + main.lua, gọi engine.load(), vẽ frame đầu
  Serial.println("[sys] engine ready. Viet code Lua trong data/main.lua");
}

void loop() {
  keys_poll();
  engine_draw_frame(millis());  // gọi engine.update(dt)/draw() đúng fps
  delay(1);
}
