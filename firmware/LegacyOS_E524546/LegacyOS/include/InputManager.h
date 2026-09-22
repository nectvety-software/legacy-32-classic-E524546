#pragma once
#include <Arduino.h>
#include "HardwareConfig.h"

// ═══════════════════════════════════════════════════════════
//  InputManager - Debounced button input with events
// ═══════════════════════════════════════════════════════════

enum class KeyCode {
  NONE, UP, DOWN, LEFT, RIGHT,
  MENU, OPTION, SELECT, START, A, B
};

enum class KeyEvent { PRESSED, HELD, RELEASED };

struct KeyState {
  bool     current;
  bool     prev;
  bool     pressed;   // Edge: just pressed
  bool     released;  // Edge: just released
  bool     held;      // Held for long press
  uint32_t pressTime;
  uint32_t lastRepeat;
};

#define BTN_DEBOUNCE_MS    20
#define BTN_LONG_PRESS_MS 600   // SELECT giu >600ms: doi che do Game/T9
#define BTN_REPEAT_MS     150
#define BTN_COUNT          10

class InputManager {
public:
  // Key states accessible by index
  KeyState keys[BTN_COUNT];
  
  // Convenience: last key event
  KeyCode  lastKey;
  KeyEvent lastEvent;
  bool     anyKeyPressed;
  uint32_t lastActivityTime;
  
  void init() {
    const uint8_t pins[BTN_COUNT] = {
      KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
      KEY_MENU, KEY_OPTION, KEY_SELECT, KEY_START,
      KEY_A, KEY_B
    };
    for (int i = 0; i < BTN_COUNT; i++) {
      pinMode(pins[i], INPUT_PULLUP);
      memset(&keys[i], 0, sizeof(KeyState));
    }
    lastKey = KeyCode::NONE;
    lastEvent = KeyEvent::RELEASED;
    anyKeyPressed = false;
    lastActivityTime = millis();
    Serial.println(F("[Input] 10-button matrix initialized"));
  }
  
  void update() {
    const uint8_t pins[BTN_COUNT] = {
      KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
      KEY_MENU, KEY_OPTION, KEY_SELECT, KEY_START,
      KEY_A, KEY_B
    };
    
    anyKeyPressed = false;
    lastKey = KeyCode::NONE;
    lastEvent = KeyEvent::RELEASED;
    
    uint32_t now = millis();
    
    for (int i = 0; i < BTN_COUNT; i++) {
      bool raw = !digitalRead(pins[i]);  // Active LOW (pull-up)
      
      // Debounce
      keys[i].pressed  = false;
      keys[i].released = false;
      keys[i].held     = false;
      
      if (raw != keys[i].prev) {
        // State changed - debounce
        if (now - keys[i].pressTime >= BTN_DEBOUNCE_MS) {
          keys[i].current = raw;
          keys[i].prev    = raw;
          
          if (raw) {
            keys[i].pressed   = true;
            keys[i].pressTime = now;
            keys[i].lastRepeat = now;
            lastKey    = (KeyCode)(i+1);
            lastEvent  = KeyEvent::PRESSED;
            lastActivityTime = now;
            anyKeyPressed = true;
          } else {
            keys[i].released = true;
            lastKey   = (KeyCode)(i+1);
            lastEvent = KeyEvent::RELEASED;
          }
        }
      } else if (raw && keys[i].current) {
        // Key is held
        if (now - keys[i].pressTime >= BTN_LONG_PRESS_MS) {
          keys[i].held = true;
          
          // Auto-repeat for navigation keys
          if (i < 4 && now - keys[i].lastRepeat >= BTN_REPEAT_MS) {
            keys[i].pressed = true;  // Synthetic press for repeat
            keys[i].lastRepeat = now;
            lastKey   = (KeyCode)(i+1);
            lastEvent = KeyEvent::HELD;
            lastActivityTime = now;
            anyKeyPressed = true;
          }
        }
      }
      
      keys[i].prev = raw;
      if (keys[i].current) anyKeyPressed = true;
    }
  }
  
  // Convenience accessors
  bool up()     { return keys[0].pressed; }
  bool down()   { return keys[1].pressed; }
  bool left()   { return keys[2].pressed; }
  bool right()  { return keys[3].pressed; }
  bool menu()   { return keys[4].pressed; }
  bool option() { return keys[5].pressed; }
  bool select() { return keys[6].pressed; }
  bool start()  { return keys[7].pressed; }
  bool a()      { return keys[8].pressed; }
  bool b()      { return keys[9].pressed; }

  // Vai tro OS theo docs/system_prompt_phan_cung.md:
  // START=OK, A=Back, MENU=Home, B=Delete, OPTION=Context.
  bool ok()      { return start(); }
  bool back()    { return a(); }
  bool home()    { return menu(); }
  bool del()     { return b(); }
  bool context() { return option(); }
  // SELECT giu: doi che do Game/T9.
  bool modeToggleHeld() { return isHeld(KeyCode::SELECT); }
  
  bool upHeld()    { return keys[0].held; }
  bool downHeld()  { return keys[1].held; }
  bool leftHeld()  { return keys[2].held; }
  bool rightHeld() { return keys[3].held; }
  
  bool isHeld(KeyCode k) {
    if (k == KeyCode::NONE) return false;
    return keys[(int)k - 1].held;
  }
  
  bool isPressed(KeyCode k) {
    if (k == KeyCode::NONE) return false;
    return keys[(int)k - 1].pressed;
  }
  
  // Wait for any key (blocking, for dialogs)
  KeyCode waitForKey(uint32_t timeout = 10000) {
    uint32_t start = millis();
    while (millis() - start < timeout) {
      update();
      if (lastKey != KeyCode::NONE && lastEvent == KeyEvent::PRESSED) {
        return lastKey;
      }
      delay(10);
    }
    return KeyCode::NONE;
  }
  
  // Idle check
  bool isIdle(uint32_t idleMs = 30000) {
    return (millis() - lastActivityTime) > idleMs;
  }
  
  uint32_t idleTime() {
    return millis() - lastActivityTime;
  }
  
  void resetIdle() {
    lastActivityTime = millis();
  }
  
  // Key name for debug
  static const char* keyName(KeyCode k) {
    switch(k) {
      case KeyCode::UP:     return "UP";
      case KeyCode::DOWN:   return "DOWN";
      case KeyCode::LEFT:   return "LEFT";
      case KeyCode::RIGHT:  return "RIGHT";
      case KeyCode::MENU:   return "MENU";
      case KeyCode::OPTION: return "OPTION";
      case KeyCode::SELECT: return "SELECT";
      case KeyCode::START:  return "START";
      case KeyCode::A:      return "A";
      case KeyCode::B:      return "B";
      default:              return "NONE";
    }
  }
};
