// ui_s60.h — Symbian S60 (3rd Edition / Nokia feature-phone) look & feel.
//
// The firmware UI and the host simulator must render the exact same theme, so
// every colour and metric lives here as a plain constant. No dependency other
// than <stdint.h>: this header is included by src/main.cpp after <Arduino.h>.
//
// Style notes (why these colours):
//   * The application pane (status pane + title bar) is one vertical gradient,
//     light steel blue at the top fading to near-black — the S60 3rd Edition
//     default theme.
//   * List highlight is the S60 "blue bar": a light-to-dark blue gradient with
//     a 1px light rule on top and white bold text.
//   * The softkey bar is a near-black gradient strip with a 1px grey rule and
//     white bold left/right softkey labels.
#pragma once
#include <stdint.h>

// ---- RGB888 -> RGB565 (compile-time constant in both firmware and simulator)
#define S60_RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

// ---------------------------------------------------------------- application pane
#define S60_PANE_TOP   S60_RGB(0x4A, 0x7E, 0xB4)   // status pane top
#define S60_PANE_MID   S60_RGB(0x1E, 0x3E, 0x68)   // mid / flat fill fallback
#define S60_PANE_BOT   S60_RGB(0x14, 0x2A, 0x4A)   // bottom of the pane
#define S60_PANE_LINE  S60_RGB(0x9C, 0xC2, 0xE8)   // 1px light rule under the pane
#define S60_PANE_TEXT  S60_RGB(0xFF, 0xFF, 0xFF)
#define S60_PANE_DIM   S60_RGB(0x8F, 0xA8, 0xC4)   // unsupported signal/battery slots

// ---------------------------------------------------------------- content
#define S60_BG         S60_RGB(0xFF, 0xFF, 0xFF)
#define S60_FG         S60_RGB(0x0E, 0x0E, 0x0E)
#define S60_LINK       S60_RGB(0x0B, 0x5C, 0xA8)
#define S60_DIM        S60_RGB(0x6A, 0x6A, 0x6A)
#define S60_RULE       S60_RGB(0xC8, 0xCE, 0xD6)   // list separator / light border
#define S60_WHITE      S60_RGB(0xFF, 0xFF, 0xFF)
#define S60_RED        S60_RGB(0xD8, 0x1B, 0x1B)
#define S60_SHADOW     S60_RGB(0x70, 0x70, 0x70)

// ---------------------------------------------------------------- list highlight
// Focus block S60: gradient xanh + khung 2px S60_SEL_LINE (như focus ring).
#define S60_SEL_TOP    S60_RGB(0x4B, 0x96, 0xDC)
#define S60_SEL_BOT    S60_RGB(0x0C, 0x45, 0x86)
#define S60_SEL_LINE   S60_RGB(0xA8, 0xCF, 0xF0)
#define S60_SEL_TEXT   S60_RGB(0xFF, 0xFF, 0xFF)

// ---------------------------------------------------------------- softkey bar
#define S60_SOFT_TOP   S60_RGB(0x44, 0x44, 0x44)
#define S60_SOFT_BOT   S60_RGB(0x08, 0x08, 0x08)
#define S60_SOFT_LINE  S60_RGB(0x9C, 0x9C, 0x9C)
#define S60_SOFT_TEXT  S60_RGB(0xFF, 0xFF, 0xFF)

// ---------------------------------------------------------------- fields / keys
#define S60_FIELD      S60_RGB(0xEC, 0xF1, 0xF7)
#define S60_FIELD_LINE S60_RGB(0x7E, 0x97, 0xB4)
#define S60_KEY_TOP    S60_RGB(0xFA, 0xFA, 0xFA)
#define S60_KEY_BOT    S60_RGB(0xC4, 0xC9, 0xD0)
#define S60_KEY_LINE   S60_RGB(0x76, 0x7C, 0x86)
#define S60_KEY_TEXT   S60_RGB(0x10, 0x10, 0x10)
#define S60_KEY_DIM    S60_RGB(0x9A, 0x9A, 0x9A)

// ---------------------------------------------------------------- icon tiles
#define S60_TILE_TOP   S60_RGB(0xFF, 0xFF, 0xFF)
#define S60_TILE_BOT   S60_RGB(0xC2, 0xD1, 0xE4)
#define S60_TILE_LINE  S60_RGB(0x6E, 0x86, 0xA4)
#define S60_TILE_GLYPH S60_RGB(0x1C, 0x4C, 0x86)

// ---------------------------------------------------------------- accents
#define S60_ACCENT     S60_RGB(0x35, 0x8C, 0xDE)   // progress fill / focus frame
#define S60_TRACK      S60_RGB(0x14, 0x14, 0x14)   // progress track on dark bars
#define S60_SCROLL_BG  S60_RGB(0xE4, 0xE9, 0xEF)
#define S60_SCROLL_FG  S60_RGB(0x6E, 0x8A, 0xAC)

// ---------------------------------------------------------------- screen layout
// Dung chung cho firmware va simulator: main.cpp include header nay ngay sau
// phan khai bao palettes.
#define UI_HDR_H   22         // application pane: status pane + title bar
#define UI_FTR_H   18         // softkey bar
#define UI_PAD     8          // le trai/phai chuan
#define UI_ROWS_Y  (UI_HDR_H + 6)

// ---------------------------------------------------------------- metrics
#define S60_PANE_H     UI_HDR_H         // status pane + title bar share one band
#define S60_SOFT_H     UI_FTR_H
#define S60_ROW_H      22               // list row height (icon tile 18x18 + padding)
#define S60_TILE       (S60_ROW_H - 4)  // icon tile side inside a list row
#define S60_BOLD_PX    1                // bold stem thickening, in pixels
