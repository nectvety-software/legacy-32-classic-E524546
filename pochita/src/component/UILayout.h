#ifndef POCHITA_UI_LAYOUT_H
#define POCHITA_UI_LAYOUT_H

#include <Arduino.h>

// Portrait layout shared by every POCHITA OS screen.
namespace UiLayout {
constexpr int WIDTH = 240;
constexpr int HEIGHT = 320;
constexpr int CENTER_X = WIDTH / 2;
constexpr int CENTER_Y = HEIGHT / 2;
constexpr int MARGIN = 8;
constexpr int HEADER_H = 24;
constexpr int FOOTER_H = 24;
constexpr int FOOTER_Y = HEIGHT - FOOTER_H;
constexpr int CONTENT_Y = HEADER_H;
constexpr int CONTENT_H = FOOTER_Y - CONTENT_Y;
constexpr int CONTENT_W = WIDTH - (MARGIN * 2);

inline int clampX(int x) { return constrain(x, 0, WIDTH - 1); }
inline int clampY(int y) { return constrain(y, 0, HEIGHT - 1); }

inline String ellipsize(const String &text, size_t maxChars) {
  if (text.length() <= maxChars) return text;
  if (maxChars <= 3) return text.substring(0, maxChars);
  return text.substring(0, maxChars - 3) + "...";
}
}  // namespace UiLayout

#endif
