#include "Keyboard.h"
#include "Config.h"

Keyboard keyboard;

namespace {
constexpr int KEYBOARD_TOP = 139;
constexpr int KEY_H = 24;
constexpr int KEY_GAP = 2;
constexpr int FOOTER_Y = 296;

const int inputPins[10] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SELECT,
                           KEY_OPTION, KEY_START, KEY_B, KEY_OPTION, KEY_A};
}

Keyboard::Keyboard() {}

void Keyboard::begin(bool passwordMode) {
  selectedRow = 1;
  selectedCol = 0;
  uppercase = false;
  symbols = false;
  masked = passwordMode;
  waitForInitialRelease = true;
  editingBuffer = nullptr;
  for (int i = 0; i < BUTTON_COUNT; ++i) lastRaw[i] = false;
}

const char *Keyboard::rowText(int row) const {
  static const char *lower[ROW_COUNT] = {
      "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm./-"};
  static const char *upper[ROW_COUNT] = {
      "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM./-"};
  static const char *symbol[ROW_COUNT] = {
      "1234567890", "!@#$%^&*()", "[]{}<>+=?", "_-\\/.,:;'"};
  if (row < 0 || row >= ROW_COUNT) return "";
  return symbols ? symbol[row] : (uppercase ? upper[row] : lower[row]);
}

int Keyboard::columnCount(int row) const {
  if (row == SPECIAL_ROW) return 4;
  return strlen(rowText(row));
}

int Keyboard::buttonIndexForPin(int pin) const {
  for (int i = 0; i < BUTTON_COUNT; ++i)
    if (inputPins[i] == pin) return i;
  return -1;
}

bool Keyboard::justPressed(int pin) {
  int index = buttonIndexForPin(pin);
  if (index < 0) return false;
  bool raw = digitalRead(pin) == LOW;
  bool edge = raw && !lastRaw[index];
  lastRaw[index] = raw;
  return edge;
}

bool Keyboard::allReleased() const {
  for (int pin : inputPins)
    if (digitalRead(pin) == LOW) return false;
  return true;
}

void Keyboard::moveSelection(int dx, int dy) {
  if (dy != 0) {
    selectedRow += dy;
    if (selectedRow < 0) selectedRow = SPECIAL_ROW;
    if (selectedRow > SPECIAL_ROW) selectedRow = 0;
  }
  int count = columnCount(selectedRow);
  if (dx != 0) {
    selectedCol += dx;
    if (selectedCol < 0) selectedCol = count - 1;
    if (selectedCol >= count) selectedCol = 0;
  } else if (selectedCol >= count) {
    selectedCol = count - 1;
  }
}

void Keyboard::insertCharacter(String &buffer, char value) {
  bool marker = buffer.endsWith("_");
  if (marker) buffer.remove(buffer.length() - 1);
  if (buffer.length() < 255) buffer += value;
  if (marker) buffer += '_';
}

void Keyboard::deleteCharacter(String &buffer) {
  bool marker = buffer.endsWith("_");
  if (marker) buffer.remove(buffer.length() - 1);
  if (buffer.length()) buffer.remove(buffer.length() - 1);
  if (marker) buffer += '_';
}

int Keyboard::activateSelection(String &buffer) {
  if (selectedRow < SPECIAL_ROW) {
    const char *row = rowText(selectedRow);
    insertCharacter(buffer, row[selectedCol]);
    return 0;
  }

  switch (selectedCol) {
    case 0:
      symbols = false;
      uppercase = !uppercase;
      return -1;
    case 1:
      insertCharacter(buffer, ' ');
      return 0;
    case 2:
      deleteCharacter(buffer);
      return 0;
    default:
      return 1;
  }
}

String Keyboard::visibleText(const String &buffer) const {
  String value = buffer;
  if (value.endsWith("_")) value.remove(value.length() - 1);
  if (masked) {
    String hidden;
    hidden.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i) hidden += '*';
    return hidden;
  }
  return value;
}

void Keyboard::drawInputArea() {
  tft.fillRect(0, 52, UiLayout::WIDTH, 85, SymbianUI::BG);
  tft.drawRect(4, 56, 232, 77, SymbianUI::DIVIDER);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);

  String value = editingBuffer ? visibleText(*editingBuffer) : "";
  constexpr int charsPerLine = 28;
  constexpr int visibleLines = 3;
  int first = max(0, static_cast<int>(value.length()) - charsPerLine * visibleLines);
  value = value.substring(first);
  for (int line = 0; line < visibleLines; ++line) {
    int from = line * charsPerLine;
    if (from >= static_cast<int>(value.length())) break;
    tft.drawString(value.substring(from, min(from + charsPerLine,
                                             static_cast<int>(value.length()))),
                   10, 63 + line * 20, 1);
  }

  int cursorChars = value.length() % charsPerLine;
  int cursorLine = min(visibleLines - 1,
                       static_cast<int>(value.length()) / charsPerLine);
  int cursorX = 10 + cursorChars * 6;
  int cursorY = 63 + cursorLine * 20;
  tft.drawFastVLine(min(cursorX, 229), cursorY, 14, SymbianUI::FG);
}

void Keyboard::drawCharacterRow(int row, bool fullRedraw) {
  (void)fullRedraw;
  const char *keys = rowText(row);
  int count = strlen(keys);
  int keyW = (UiLayout::WIDTH - 2 - (count - 1) * KEY_GAP) / count;
  int usedW = keyW * count + (count - 1) * KEY_GAP;
  int startX = (UiLayout::WIDTH - usedW) / 2;
  int y = KEYBOARD_TOP + row * (KEY_H + KEY_GAP);

  for (int col = 0; col < count; ++col) {
    int x = startX + col * (keyW + KEY_GAP);
    bool selected = row == selectedRow && col == selectedCol;
    uint16_t bg = selected ? SymbianUI::SELECT : SymbianUI::BG;
    tft.fillRect(x, y, keyW, KEY_H, bg);
    tft.drawRect(x, y, keyW, KEY_H,
                 selected ? SymbianUI::SELECT_BORDER : SymbianUI::MUTED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(SymbianUI::FG, bg);
    char label[2] = {keys[col], 0};
    tft.drawString(label, x + keyW / 2, y + KEY_H / 2, 1);
  }
}

void Keyboard::drawSpecialRow(bool fullRedraw) {
  (void)fullRedraw;
  const int x[4] = {3, 57, 157, 196};
  const int w[4] = {51, 97, 36, 41};
  const char *labels[4] = {symbols ? "abc" : (uppercase ? "ABC" : "abc"),
                           "space", "<", "OK"};
  int y = KEYBOARD_TOP + ROW_COUNT * (KEY_H + KEY_GAP) + 1;
  int h = 36;
  for (int col = 0; col < 4; ++col) {
    bool selected = selectedRow == SPECIAL_ROW && selectedCol == col;
    uint16_t bg = selected ? SymbianUI::SELECT : SymbianUI::BG;
    tft.fillRect(x[col], y, w[col], h, bg);
    tft.drawRect(x[col], y, w[col], h,
                 selected ? SymbianUI::SELECT_BORDER : SymbianUI::DIM);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(SymbianUI::FG, bg);
    tft.drawString(labels[col], x[col] + w[col] / 2, y + h / 2, 2);
  }
}

void Keyboard::drawFooter() {
  tft.fillRect(0, FOOTER_Y, UiLayout::WIDTH, 24, SymbianUI::BG);
  tft.drawFastHLine(0, FOOTER_Y, UiLayout::WIDTH, SymbianUI::DIVIDER);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SymbianUI::ACCENT, SymbianUI::BG);
  tft.drawString("Menu: symbols", 57, FOOTER_Y + 12, 1);
  tft.drawString("Option: cancel", 181, FOOTER_Y + 12, 1);
}

void Keyboard::draw(bool fullRedraw) {
  if (fullRedraw) {
    tft.fillScreen(SymbianUI::BG);
    SymbianUI::drawStatusBar();
    SymbianUI::drawTitle(masked ? "Enter Password" : "Keyboard");
  }
  drawInputArea();
  for (int row = 0; row < ROW_COUNT; ++row) drawCharacterRow(row, fullRedraw);
  drawSpecialRow(fullRedraw);
  drawFooter();
}

int Keyboard::handleInput(String &buffer) {
  if (editingBuffer != &buffer) {
    editingBuffer = &buffer;
    draw(true);
  }

  if (waitForInitialRelease) {
    if (allReleased()) {
      waitForInitialRelease = false;
      for (int i = 0; i < BUTTON_COUNT; ++i) lastRaw[i] = false;
    }
    return -1;
  }

  // Refresh every raw state once per frame and consume only rising edges.
  bool up = justPressed(KEY_UP);
  bool down = justPressed(KEY_DOWN);
  bool left = justPressed(KEY_LEFT);
  bool right = justPressed(KEY_RIGHT);
  bool menu = justPressed(KEY_MENU);
  bool cancel = justPressed(KEY_A);
  bool select = justPressed(KEY_START);
  (void)justPressed(KEY_MENU); // Reserved globally for Home/Power.
  bool action = justPressed(KEY_OPTION);
  bool backspace = justPressed(KEY_B);

  if (cancel) return 2;
  if (backspace) {
    deleteCharacter(buffer);
    draw(false);
    return 0;
  }
  if (menu) {
    symbols = !symbols;
    selectedRow = min(selectedRow, SPECIAL_ROW);
    selectedCol = min(selectedCol, columnCount(selectedRow) - 1);
    draw(false);
    return -1;
  }
  if (up || down || left || right) {
    moveSelection(right ? 1 : (left ? -1 : 0), down ? 1 : (up ? -1 : 0));
    draw(false);
    return -1;
  }
  if (select || action) {
    int result = activateSelection(buffer);
    draw(false);
    return result;
  }
  return -1;
}
