#pragma once
#include <Arduino.h>
#include "Types.h"

class InputManager {
public:
  void begin();
  KeyEvent poll();
  // SELECT hold >650ms toggles logical Game/T9. Only text editors consume
  // numeric events; normal OS menus keep their navigational keymap.
  bool t9Mode() const { return numericMode; }
  void setTextInputActive(bool active) { editorActive=active; }
private:
  struct BtnState {
    int pin;
    Key key;
    bool stableHigh;
    bool rawHigh;
    bool longSent;
    uint32_t changedAt;
    uint32_t pressedAt;
    uint32_t repeatedAt;

    BtnState()
        : pin(-1), key(Key::None), stableHigh(true), rawHigh(true),
          longSent(false), changedAt(0), pressedAt(0), repeatedAt(0) {}

    BtnState(int p, Key k)
        : pin(p), key(k), stableHigh(true), rawHigh(true),
          longSent(false), changedAt(0), pressedAt(0), repeatedAt(0) {}
  };
  BtnState buttons[10];
  bool numericMode=false;
  bool editorActive=false;
  Key dispatchKey(Key k) const;
  static constexpr uint32_t DEBOUNCE_MS = 28;
  static constexpr uint32_t LONG_MS = 650;
  static constexpr uint32_t REPEAT_DELAY_MS = 380;
  static constexpr uint32_t REPEAT_INTERVAL_MS = 120;

  bool repeatable(Key key) const {
    return key == Key::Up || key == Key::Down || key == Key::Left || key == Key::Right;
  }
};
