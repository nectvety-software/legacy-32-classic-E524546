#include "InputManager.h"
#include "BoardConfig.h"

// C++11 out-of-class definitions for odr-used static constexpr members.
constexpr uint32_t InputManager::DEBOUNCE_MS;
constexpr uint32_t InputManager::LONG_MS;
constexpr uint32_t InputManager::REPEAT_DELAY_MS;
constexpr uint32_t InputManager::REPEAT_INTERVAL_MS;

void InputManager::begin() {
  buttons[0] = BtnState(Board::KEY_MENU, Key::Menu);
  buttons[1] = BtnState(Board::KEY_UP, Key::Up);
  buttons[2] = BtnState(Board::KEY_A, Key::A);
  buttons[3] = BtnState(Board::KEY_LEFT, Key::Left);
  buttons[4] = BtnState(Board::KEY_START, Key::Start);
  buttons[5] = BtnState(Board::KEY_RIGHT, Key::Right);
  buttons[6] = BtnState(Board::KEY_OPTION, Key::Option);
  buttons[7] = BtnState(Board::KEY_DOWN, Key::Down);
  buttons[8] = BtnState(Board::KEY_B, Key::B);
  buttons[9] = BtnState(Board::KEY_SELECT, Key::Select);

  for (auto &b : buttons) {
    pinMode(b.pin, INPUT_PULLUP);
    b.rawHigh = digitalRead(b.pin);
    b.stableHigh = b.rawHigh;
    b.changedAt = millis();
  }
}

Key InputManager::dispatchKey(Key key) const {
  if (!numericMode || !editorActive) return key;
  switch(key) {
    case Key::Menu:return Key::Num1;case Key::Up:return Key::Num2;
    case Key::A:return Key::Num3;case Key::Left:return Key::Num4;
    case Key::Start:return Key::Num5;case Key::Right:return Key::Num6;
    case Key::Option:return Key::Num7;case Key::Down:return Key::Num8;
    case Key::B:return Key::Num9;case Key::Select:return Key::Num0;
    default:return key;
  }
}

KeyEvent InputManager::poll() {
  const uint32_t now = millis();
  for (auto &b : buttons) {
    bool raw = digitalRead(b.pin);
    if (raw != b.rawHigh) {
      b.rawHigh = raw;
      b.changedAt = now;
    }
    if ((now - b.changedAt) >= DEBOUNCE_MS && raw != b.stableHigh) {
      b.stableHigh = raw;
      if (!raw) {
        b.pressedAt = now;
        b.repeatedAt = now;
        b.longSent = false;
        // MENU and SELECT are the only system-wide dual-purpose keys. Defer
        // their short click until release; holding MENU may open Tasks, while
        // SELECT hold switches T9. START/OPTION/A/B remain immediate clicks,
        // never followed by a second global long-press navigation.
        if(b.key==Key::Select || b.key==Key::Menu)return KeyEvent();
        return KeyEvent(dispatchKey(b.key), true, false, false);
      }
      if ((b.key==Key::Select || b.key==Key::Menu) && !b.longSent)
        return KeyEvent(dispatchKey(b.key),true,false,false);
    }

    if (!b.stableHigh && repeatable(b.key) &&
        (now - b.pressedAt) >= REPEAT_DELAY_MS &&
        (now - b.repeatedAt) >= REPEAT_INTERVAL_MS) {
      b.repeatedAt = now;
      if (numericMode && editorActive) continue; // no key-repeat on numeric multitap
      return KeyEvent(b.key, true, false, true);
    }

    if (!b.stableHigh && (b.key==Key::Select || b.key==Key::Menu) &&
        !b.longSent && (now - b.pressedAt) >= LONG_MS) {
      b.longSent = true;
      if(b.key==Key::Select)numericMode=!numericMode;
      return KeyEvent(b.key, true, true, false);
    }
  }
  return KeyEvent();
}
