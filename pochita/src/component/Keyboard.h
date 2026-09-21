#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Arduino.h>
#include "Display.h"

extern TFT_eSPI tft;
extern bool isSelectPressed();
extern bool isBackPressed();
extern bool isOptionPressed();
extern bool isStartPressed();

// Full-screen keyboard controlled entirely by the ten physical buttons.
class Keyboard {
public:
  Keyboard();
  void begin(bool passwordMode = false);
  void draw(bool fullRedraw = true);

  // -1: no text change, 0: text changed, 1: OK, 2: Cancel.
  int handleInput(String &buffer);

  bool active = false;

private:
  static constexpr int ROW_COUNT = 4;
  static constexpr int SPECIAL_ROW = 4;
  static constexpr int BUTTON_COUNT = 10;

  int selectedRow = 1;
  int selectedCol = 0;
  bool uppercase = false;
  bool symbols = false;
  bool masked = false;
  bool waitForInitialRelease = true;
  bool lastRaw[BUTTON_COUNT] = {};
  String *editingBuffer = nullptr;

  const char *rowText(int row) const;
  int columnCount(int row) const;
  int buttonIndexForPin(int pin) const;
  bool justPressed(int pin);
  bool allReleased() const;
  void moveSelection(int dx, int dy);
  int activateSelection(String &buffer);
  void insertCharacter(String &buffer, char value);
  void deleteCharacter(String &buffer);
  String visibleText(const String &buffer) const;
  void drawInputArea();
  void drawCharacterRow(int row, bool fullRedraw);
  void drawSpecialRow(bool fullRedraw);
  void drawFooter();
};

extern Keyboard keyboard;

#endif
