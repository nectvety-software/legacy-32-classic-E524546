#include "TextKeyboard.h"
#include "BoardConfig.h"

// C++11 out-of-class definitions for odr-used static constexpr members.
constexpr int TextKeyboard::COLS;
constexpr int TextKeyboard::COUNT;
constexpr int TextKeyboard::MAX_TEXT;

void TextKeyboard::open(const String &cap, const String &initial, bool password) {
  caption = cap; text = initial; masked = password; cursor = 0;
  shown = true; done = cancel = false; upper = false; firstDraw = true;
  lastNumeric = Key::None; numericTap=0; numericAt=0;
}

void TextKeyboard::moveVertical(int direction) {
  int col = cursor % COLS;
  int candidate = cursor + direction * COLS;
  if (candidate >= 0 && candidate < COUNT) {
    cursor = candidate;
    return;
  }
  if (direction < 0) {
    candidate = col;
    while (candidate + COLS < COUNT) candidate += COLS;
    cursor = candidate;
  } else {
    cursor = min(col, COUNT - 1);
  }
}

// Numeric multi-tap is enabled by SELECT hold; it does not replace the
// virtual keyboard or require a heap-allocated dictionary on this MCU.
bool TextKeyboard::typeDigit(Key key) {
  static const char *const map[]={" 0",".,?1","abc2","def3","ghi4",
                                   "jkl5","mno6","pqrs7","tuv8","wxyz9"};
  const int digit=int(key)-int(Key::Num0);
  if(digit<0||digit>9)return false;
  const char *sequence=map[digit];
  const int len=strlen(sequence);
  const uint32_t now=millis();
  if(lastNumeric==key && (uint32_t)(now-numericAt)<650 && text.length()) {
    numericTap=(numericTap+1)%len;
    char c=sequence[numericTap];
    if(upper && c>='a' && c<='z') c=char(c-32);
    text.remove(text.length()-1); text+=c;
  } else if(text.length()<MAX_TEXT) {
    numericTap=0;
    char c=sequence[numericTap];
    if(upper && c>='a' && c<='z')c=char(c-32);
    text+=c;
  }
  lastNumeric=key;numericAt=now;
  return true;
}

bool TextKeyboard::handle(const KeyEvent &e) {
  if (!shown || !e.pressed || e.longPress) return false;
  if(e.key>=Key::Num0 && e.key<=Key::Num9)return typeDigit(e.key);
  lastNumeric=Key::None;
  switch (e.key) {
    case Key::Left: cursor = (cursor + COUNT - 1) % COUNT; return true;
    case Key::Right: cursor = (cursor + 1) % COUNT; return true;
    case Key::Up: moveVertical(-1); return true;
    case Key::Down: moveVertical(1); return true;
    case Key::Start: {
      char ch = layout[cursor];
      if (upper && ch >= 'a' && ch <= 'z') ch = char(ch - 'a' + 'A');
      if (text.length() < MAX_TEXT) text += ch;
      return true;
    }
    case Key::B: if (text.length()) text.remove(text.length()-1); return true;
    case Key::Option: upper = !upper; return true;
    case Key::Select: if (text.length() < MAX_TEXT) text += ' '; return true;
    case Key::Menu: done = true; shown = false; return true;
    case Key::A: cancel = true; shown = false; return true;
    default: return false;
  }
}

void TextKeyboard::draw(SymbianUI &ui, bool wifi, bool ble, bool sd, bool hour12) {
  auto &tft = ui.display(); auto c = ui.c();
  if (firstDraw) { ui.clearContent(); firstDraw = false; }
  ui.chrome(caption, wifi, ble, sd, hour12);
  String visible;
  if (masked) { for (size_t i = 0; i < text.length(); ++i) visible += '*'; }
  else visible = text;
  tft.fillRect(7, 39, Board::SCREEN_W-14, 29, c.panel);
  tft.drawRect(7, 39, Board::SCREEN_W-14, 29, c.dim);
  tft.setTextColor(c.text, c.panel); tft.setTextSize(1); tft.setCursor(12, 49);
  if (visible.length() > 34) visible = visible.substring(visible.length()-34);
  tft.print(visible);

  for (int i = 0; i < COUNT; ++i) {
    int col = i % COLS, row = i / COLS;
    int x = 7 + col*38, y = 76 + row*23;
    uint16_t bg = (i == cursor) ? c.selected : c.panel;
    tft.fillRect(x, y, 34, 20, bg);
    tft.drawRect(x, y, 34, 20, (i == cursor) ? c.border : c.dim);
    char ch = layout[i];
    if (upper && ch >= 'a' && ch <= 'z') ch = char(ch - 'a' + 'A');
    tft.setTextColor(c.text, bg); tft.setTextSize(1); tft.setCursor(x+14, y+7); tft.print(ch);
  }
  ui.softkeys("Done", upper ? "ABC" : "abc", "Cancel");
}
