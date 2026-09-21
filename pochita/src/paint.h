#ifndef PAINT_H
#define PAINT_H

#include "component/Config.h"
#include "component/ButtonManager.h"
#include "component/ui_utils.h"  // Display + SymbianUI + UILayout

extern TFT_eSPI tft;
extern SystemMode currentMode;
extern void drawLauncherContent();

// A compact, self-contained pixel canvas. The D-pad moves a cursor over a
// fixed grid, A/SELECT paints the current cell, MENU erases it, OPTION cycles
// the active colour and B returns to the launcher.
namespace PaintApp {
constexpr int COLS = 20;
constexpr int ROWS = 21;
constexpr int CELL = 12;
constexpr int CANVAS_X = 0;
constexpr int PALETTE_Y = SymbianUI::HEADER_H;      // amber band ends here
constexpr int PALETTE_H = 20;
constexpr int CANVAS_Y = PALETTE_Y + PALETTE_H;

constexpr int NCOLORS = 8;
constexpr int ERASE_IDX = 7;  // last palette slot is black == erase
const uint16_t PALETTE[NCOLORS] = {
    0xFD80,      // amber
    TFT_WHITE,   // white
    0xF800,      // red
    0x07E0,      // green
    0x001F,      // blue
    0x07FF,      // cyan
    0xFFE0,      // yellow
    0x0000,      // black / erase
};

uint8_t grid[ROWS][COLS];
int curR = ROWS / 2;
int curC = COLS / 2;
int colorIdx = 0;
}  // namespace PaintApp

inline void drawPaintCell(int r, int c) {
  int x = PaintApp::CANVAS_X + c * PaintApp::CELL;
  int y = PaintApp::CANVAS_Y + r * PaintApp::CELL;
  tft.fillRect(x, y, PaintApp::CELL, PaintApp::CELL,
               PaintApp::PALETTE[PaintApp::grid[r][c]]);
}

inline void drawPaintCursor() {
  int x = PaintApp::CANVAS_X + PaintApp::curC * PaintApp::CELL;
  int y = PaintApp::CANVAS_Y + PaintApp::curR * PaintApp::CELL;
  // Nested light/dark outline stays visible over any cell colour.
  tft.drawRect(x, y, PaintApp::CELL, PaintApp::CELL, TFT_WHITE);
  tft.drawRect(x + 1, y + 1, PaintApp::CELL - 2, PaintApp::CELL - 2, TFT_BLACK);
}

inline void drawPaintPalette() {
  int sw = UiLayout::WIDTH / PaintApp::NCOLORS;
  for (int i = 0; i < PaintApp::NCOLORS; i++) {
    int x = i * sw;
    tft.fillRect(x, PaintApp::PALETTE_Y, sw, PaintApp::PALETTE_H,
                 PaintApp::PALETTE[i]);
    if (PaintApp::PALETTE[i] == TFT_BLACK)
      tft.drawRect(x, PaintApp::PALETTE_Y, sw, PaintApp::PALETTE_H,
                   SymbianUI::DIM);
    if (i == PaintApp::colorIdx) {
      tft.drawRect(x, PaintApp::PALETTE_Y, sw, PaintApp::PALETTE_H, TFT_WHITE);
      tft.drawRect(x + 1, PaintApp::PALETTE_Y + 1, sw - 2, PaintApp::PALETTE_H - 2,
                   TFT_WHITE);
    }
  }
}

inline void drawPaintApp() {
  SymbianUI::drawChrome("Paint", "Color", "Exit");
  drawPaintPalette();
  tft.fillRect(PaintApp::CANVAS_X, PaintApp::CANVAS_Y,
               PaintApp::COLS * PaintApp::CELL,
               PaintApp::ROWS * PaintApp::CELL, TFT_BLACK);
  for (int r = 0; r < PaintApp::ROWS; r++)
    for (int c = 0; c < PaintApp::COLS; c++)
      if (PaintApp::grid[r][c] != PaintApp::ERASE_IDX) drawPaintCell(r, c);
  drawPaintCursor();
}

inline void initPaintApp() {
  for (int r = 0; r < PaintApp::ROWS; r++)
    for (int c = 0; c < PaintApp::COLS; c++)
      PaintApp::grid[r][c] = PaintApp::ERASE_IDX;
  PaintApp::curR = PaintApp::ROWS / 2;
  PaintApp::curC = PaintApp::COLS / 2;
  PaintApp::colorIdx = 0;
  drawPaintApp();
}

inline void loopPaint() {
  int oldR = PaintApp::curR, oldC = PaintApp::curC;
  bool moved = false;
  if (buttonManager.isJustPressed(KEY_UP) && PaintApp::curR > 0) {
    PaintApp::curR--; moved = true;
  }
  if (buttonManager.isJustPressed(KEY_DOWN) && PaintApp::curR < PaintApp::ROWS - 1) {
    PaintApp::curR++; moved = true;
  }
  if (buttonManager.isJustPressed(KEY_LEFT) && PaintApp::curC > 0) {
    PaintApp::curC--; moved = true;
  }
  if (buttonManager.isJustPressed(KEY_RIGHT) && PaintApp::curC < PaintApp::COLS - 1) {
    PaintApp::curC++; moved = true;
  }
  if (moved) {
    drawPaintCell(oldR, oldC);
    drawPaintCursor();
  }

  if (buttonManager.isJustPressed(KEY_START)) {
    PaintApp::grid[PaintApp::curR][PaintApp::curC] = PaintApp::colorIdx;
    drawPaintCell(PaintApp::curR, PaintApp::curC);
    drawPaintCursor();
  }
  if (buttonManager.isJustPressed(KEY_B)) {
    PaintApp::grid[PaintApp::curR][PaintApp::curC] = PaintApp::ERASE_IDX;
    drawPaintCell(PaintApp::curR, PaintApp::curC);
    drawPaintCursor();
  }
  if (buttonManager.isJustPressed(KEY_OPTION)) {
    PaintApp::colorIdx = (PaintApp::colorIdx + 1) % PaintApp::NCOLORS;
    drawPaintPalette();
  }
  if (buttonManager.isJustPressed(KEY_A)) {
    currentMode = MODE_LAUNCHER;
    drawLauncherContent();
  }
}

#endif
