#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "LauncherTheme.h"

// Dedicated 240x320 Retro-Go-inspired launcher. This is an original
// portrait layout and original procedural art; no Retro-Go source/assets.
struct LauncherRow {
  const char *glyph;
  const char *title;
  const char *summary;
  const char *packageId; // used only for signed installed .qeapp entries
};

class LauncherView {
public:
  enum { W=240, H=320, STATUS_H=24, HEADER_Y=24, HEADER_H=58,
         LIST_Y=87, ROW_H=29, VISIBLE=5, PREVIEW_Y=237,
         PREVIEW_H=59, FOOTER_Y=297 };
  explicit LauncherView(TFT_eSPI &lcd) : tft(lcd) {}
  void status(const LauncherStyle &skin, const String &tm, bool wifi, bool sd);
  void full(const LauncherStyle &skin, const char *tabName, int tab, int tabCount,
            const LauncherRow *items, int total, int offset, int selected,
            const String &clock, bool wifi, bool sd, const uint16_t *icon565 = nullptr);
  void content(const LauncherStyle &skin, const LauncherRow *items, int total,int offset,int selected, const uint16_t *icon565 = nullptr);
  void row(const LauncherStyle &skin,const LauncherRow *item,int visibleRow,bool selected);
  void preview(const LauncherStyle &skin,const LauncherRow *item, const uint16_t *icon565 = nullptr);
  void scrollbar(const LauncherStyle &skin,int total,int offset);
  void dialog(const LauncherStyle &skin,const char *title,
              const char *const *choices,int count,int offset,int selected);
private:
  TFT_eSPI &tft;
  void clipped(const char *text,int x,int y,int maxWidth,uint16_t fg,uint16_t bg,int font,bool bold=false);
  void icon(const char *label,int x,int y,int size,const LauncherStyle &skin,bool selected);
};
