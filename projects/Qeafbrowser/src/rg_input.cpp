// rg_input.cpp — cài đặt đọc keypad (xem rg_input.h).
// Tham chiếu hành vi: E524546-OS/src/main.cpp (2 chế độ Game/T9, hold SELECT)
// và retro-go rg_input.c (bitmask gamepad, repeat khi giữ phím cuộn).
#include "rg_input.h"
#include "pins.h"
#include <Arduino.h>

struct RgKeyPin { uint8_t gpio; uint32_t bit; char t9; };

// Bảng phím — thứ tự khớp bố cục vật lý keypad 3x3 + SELECT (docs/PROMPT.md §4)
static const RgKeyPin KEY_PINS[] = {
  { KEY_MENU,   RG_KEY_MENU,   '1' },
  { KEY_UP,     RG_KEY_UP,     '2' },
  { KEY_A,      RG_KEY_BACK,   '3' },
  { KEY_LEFT,   RG_KEY_LEFT,   '4' },
  { KEY_START,  RG_KEY_OK,     '5' },
  { KEY_RIGHT,  RG_KEY_RIGHT,  '6' },
  { KEY_OPTION, RG_KEY_OPTION, '7' },
  { KEY_DOWN,   RG_KEY_DOWN,   '8' },
  { KEY_B,      RG_KEY_DELETE, '9' },
  { KEY_SELECT, RG_KEY_MODE,   '0' },
};
#define KEY_COUNT (sizeof(KEY_PINS) / sizeof(KEY_PINS[0]))

static uint32_t s_held = 0;                 // mức đã debounce
static uint32_t s_raw_last = 0;             // mức thô lần đọc trước
static unsigned long s_raw_change_ms = 0;   // thời điểm mức thô đổi (debounce)
static unsigned long s_press_ms[KEY_COUNT]; // thời điểm bắt đầu nhấn
static unsigned long s_repeat_ms[KEY_COUNT];// lần repeat kế
static bool s_select_fired = false;         // SELECT đã phát sự kiện nhấn thường chưa
static bool s_select_hold = false;          // đã phát hiện hold >600ms
static bool s_t9 = false;                   // chế độ T9
static char s_t9_char = 0;                  // ký tự T9 chờ đọc

void rg_input_init() {
  for (size_t i = 0; i < KEY_COUNT; i++) pinMode(KEY_PINS[i].gpio, INPUT_PULLUP);
  s_held = s_raw_last = 0;
  s_t9 = false; s_t9_char = 0;
}

static uint32_t read_raw() {
  uint32_t m = 0;
  for (size_t i = 0; i < KEY_COUNT; i++)
    if (digitalRead(KEY_PINS[i].gpio) == LOW) m |= KEY_PINS[i].bit;
  return m;
}

uint32_t rg_input_held() { return s_held; }
uint32_t rg_input_raw() { return read_raw(); }
bool rg_input_t9_mode() { return s_t9; }
char rg_input_read_char() { char c = s_t9_char; s_t9_char = 0; return c; }

uint32_t rg_input_read() {
  uint32_t events = 0;
  unsigned long now = millis();
  uint32_t raw = read_raw();

  // debounce: chỉ chấp nhận mức thô ổn định RG_DEBOUNCE_MS
  if (raw != s_raw_last) { s_raw_last = raw; s_raw_change_ms = now; }
  if (now - s_raw_change_ms >= RG_DEBOUNCE_MS) {
    uint32_t pressed = raw & ~s_held;    // cạnh xuống mới
    uint32_t released = s_held & ~raw;   // cạnh nhả
    s_held = raw;

    for (size_t i = 0; i < KEY_COUNT; i++) {
      uint32_t bit = KEY_PINS[i].bit;
      if (pressed & bit) {
        s_press_ms[i] = now;
        s_repeat_ms[i] = now + RG_REPEAT_DELAY_MS;
        if (bit == RG_KEY_MODE) {
          s_select_fired = true; s_select_hold = false;
        } else if (s_t9) {
          s_t9_char = KEY_PINS[i].t9;            // chế độ T9: phím = ký tự số
        } else {
          events |= bit;                          // chế độ Game: phím = sự kiện
        }
      }
      // auto-repeat cho phím cuộn (chỉ chế độ Game)
      if (!s_t9 && (s_held & bit) && (RG_KEY_REPEAT_MASK & bit) && now >= s_repeat_ms[i]) {
        events |= bit;
        s_repeat_ms[i] = now + RG_REPEAT_RATE_MS;
      }
      // SELECT: giữ >600ms -> đổi Game/T9; nhấn thường -> '0' (T9) / RG_KEY_MODE (Game)
      if ((bit == RG_KEY_MODE)) {
        if ((s_held & bit) && s_select_fired && !s_select_hold &&
            now - s_press_ms[i] > RG_SELECT_HOLD_MS) {
          s_t9 = !s_t9;
          s_select_hold = true;
          events |= RG_KEY_MODE;                  // báo đổi chế độ (Serial cũng log)
          Serial.printf("[key] che do: %s\n", s_t9 ? "T9" : "GAME");
        }
        if ((released & bit)) {
          if (s_select_fired && !s_select_hold) {
            if (s_t9) s_t9_char = '0';
            else events |= RG_KEY_MODE;
          }
          s_select_fired = false; s_select_hold = false;
        }
      }
    }
  }
  return events;
}

void rg_input_wait_release(uint32_t mask) {
  unsigned long t0 = millis();
  while (millis() - t0 < 1000) {
    rg_input_read();
    if ((s_held & mask) == 0) return;
    delay(5);
  }
}
