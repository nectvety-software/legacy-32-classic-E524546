#ifndef UI_UTILS_H
#define UI_UTILS_H

#include "Display.h"
#include "Themes.h"
#include "SymbianUI.h"

extern TFT_eSPI tft;

// Draw a vertical scrollbar
// x, y: Top-left corner of the scroll area
// w, h: Width and Height of the scroll area
// total: Total number of items
// current: Current selected item index
// visible: Number of items visible on screen at once
// color: Color of the scroll thumb (optional, uses currentTheme if not specified)
inline void drawScrollBar(int x, int y, int w, int h, int total, int current,
                          int visible, uint16_t color = SymbianUI::ACCENT) {
    if (total <= visible) return;

    // Rounded rail track (dim amber) with a solid amber thumb, matching the
    // POCHITA OS pixel look.
    tft.fillRect(x - 1, y, w + 2, h, SymbianUI::BG);
    tft.drawRoundRect(x, y, w, h, w / 2, SymbianUI::ACCENT_DK);

    float ratio = (float)visible / total;
    if (ratio > 1.0) ratio = 1.0;

    int thumbHeight = (int)(h * ratio);
    if (thumbHeight < 14) thumbHeight = 14;

    int maxScroll = total - visible;
    float scrollRatio = (maxScroll > 0) ? (float)current / maxScroll : 0;
    int thumbY = y + (int)(scrollRatio * (h - thumbHeight));

    if (thumbY < y) thumbY = y;
    if (thumbY + thumbHeight > y + h) thumbY = y + h - thumbHeight;

    tft.fillRoundRect(x, thumbY, w, thumbHeight, w / 2, color);
}


#endif
