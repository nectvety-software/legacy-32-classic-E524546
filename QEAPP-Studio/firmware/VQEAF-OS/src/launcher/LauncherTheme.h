#pragma once
#include <stdint.h>
#include "../core/Theme.h"

// Theme Studio .vqeaf palette -> 240x320 launcher skin. No heap; no copied assets.
// Optional launcher { ... } overrides are applied by ThemeFileService.
struct LauncherStyle {
  uint16_t background, foreground, headerBg, headerFg, tabAccent;
  uint16_t listBg, listFg, selectedBg, selectedFg;
  uint16_t previewBg, previewFg, scrollbar, footerBg, footerFg, border;

  static uint16_t legibleOn(uint16_t rgb) {
    const int r = (rgb >> 11) & 31;
    const int g = (rgb >> 5) & 63;
    const int b = rgb & 31;
    return (r * 3 + g * 3 + b) > 170 ? 0x0000 : 0xFFFF;
  }
  static LauncherStyle fromPalette(const ThemeColors &c) {
    LauncherStyle s = {c.bg, c.text, c.chrome, c.chromeText, c.accent,
                       c.bg, c.text, c.selected, legibleOn(c.selected),
                       c.panel, c.text, c.accent, c.chrome, c.chromeText, c.border};
    return s;
  }
};
