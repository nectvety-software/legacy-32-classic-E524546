// launcher_config.h — MOI hang so cua launcher trong MOT file duy nhat.
// Khong hard-code chan/kich thuoc/mau/phim o noi khac. Doi phan cung -> sua file nay.
// Chi dung thu vien co san trong repo (LovyanGFX/PNGdec da pin trong platformio.ini),
// khong them dependency moi.
#pragma once
#include <Arduino.h>
#include "ui_s60.h"   // bang mau S60 dung chung browser <-> launcher

// ------------------------------------------------- bo cuc man hinh doc 240x320
// Layout UC-style (240x320): status | title | url/tab | icons | section | grid | softkeys
// Mau chrome theo S60 (ui_s60.h) — dong bo voi Qeafbrowser browser UI.
#define LC_W                 240
#define LC_H                 320
#define LC_STATUS_H          14      // thanh treong: wifi/gio/pin
#define LC_TITLE_H           22      // title "Qeafbrowser" + Help
#define LC_URL_H             24      // tab/URL bar
#define LC_ICON_H            28      // dong icon toolbar
#define LC_SECT_H            16      // header "Top Sites" / ten tab
#define LC_FTR_H             24      // softkeys: Menu | gio | Switch
#define LC_HDR_H             (LC_STATUS_H + LC_TITLE_H + LC_URL_H + LC_ICON_H + LC_SECT_H)
#define LC_ART_H             0       // khong con banner art lon (UC layout)
#define LC_GRID_ITEM_H       56      // 1 o trong grid 2 cot (icon 30 + label Tahoma16)
#define LC_ROW_H             LC_GRID_ITEM_H
#define LC_ROWS              3       // hang hien thi duoc (198px / 56)
#define LC_ART_W             LC_W

// ------------------------------------------------- mau chrome = S60 (ui_s60.h)
#define UC_STATUS_BG         S60_SOFT_BOT    // thanh treong gan den (giong softkey)
#define UC_STATUS_FG         S60_SOFT_TEXT
#define UC_TITLE_TOP         S60_PANE_TOP    // gradient title giong application pane
#define UC_TITLE_BOT         S60_PANE_BOT
#define UC_TITLE_FG          S60_PANE_TEXT
#define UC_URL_BG            S60_FIELD       // o URL/field xanh nhat
#define UC_URL_FG            S60_FG
#define UC_URL_BORDER        S60_FIELD_LINE
#define UC_ICON_TOP          S60_KEY_TOP     // toolbar = key gradient S60
#define UC_ICON_BOT          S60_KEY_BOT
#define UC_SECT_BG           S60_PANE_MID    // header section
#define UC_SECT_FG           S60_PANE_TEXT
#define UC_CONTENT_BG        S60_BG
#define UC_ITEM_FG           S60_LINK        // chu muc xanh link
#define UC_ITEM_DIM          S60_DIM
#define UC_SEL_BG            S60_SEL_BOT     // o dang chon (gradient trong draw_list_area)
#define UC_SEL_FG            S60_SEL_TEXT
#define UC_SEL_FRAME         S60_SEL_LINE    // khung focus 2px
#define UC_SOFT_BG           S60_SOFT_BOT
#define UC_SOFT_FG           S60_SOFT_TEXT
#define UC_SOFT_MID          S60_SOFT_LINE
#define UC_TILE_BG           S60_TILE_TOP
#define UC_TILE_SEL_BG       S60_ACCENT
#define UC_TILE_GLYPH        S60_TILE_GLYPH
#define UC_TILE_LINE         S60_TILE_LINE
#define UC_RULE              S60_RULE
#define UC_ICON_SEL_BG       S60_ACCENT

// ------------------------------------------------- anh art
#define LC_ART_MAX_W         240     // art lon nhat chap nhan (lon hon -> tu bo qua, dung icon)
#define LC_ART_MAX_H         200
#define LC_ART_BYTES         (LC_ART_MAX_W * LC_ART_MAX_H * 2)  // RGB565
#define LC_ART_KEY_MAGENTA   0xF81F  // mau trong suốt cho art (nhu Retro-Go)

// ------------------------------------------------- duong dan SD/LFS
#define LC_DIR_ROOT          "/launcher"
#define LC_DIR_THEMES        "/launcher/themes"
#define LC_DIR_ART           "/launcher/art"
#define LC_CFG_INI           "/launcher/launcher.ini"
#define LC_THEME_DEFAULT     "retrogo_dark"
#define LC_THEME_EXT         ".vqeaf"   // VQEAF Theme Studio (@vqeaf 1.x)
#define LC_VQEAF_MAX_BYTES   4096       // gioi han doc theme (giong SymbianS3)

// ------------------------------------------------- phim (keypad 3x3 + SELECT)
// Ten "game" trung voi bang KEYS[] cua src/main.cpp de dung lai keys_poll().
#define LK_UP                "up"       // phim 2
#define LK_DOWN              "down"     // phim 8
#define LK_LEFT              "left"     // phim 4  -> tab truoc
#define LK_RIGHT             "right"    // phim 6  -> tab sau
#define LK_OK                "ok"       // phim 5  -> mo muc
#define LK_BACK              "back"     // phim 3  -> quay lai / dong hop thoai
#define LK_CONTEXT           "option"   // phim 7  -> menu ngu canh
#define LK_FAV               "delete"   // phim 9  -> yeu thich / bo yeu thich
#define LK_MENU              "menu"     // phim 1  -> tab dau / ve launcher
#define LK_SETTINGS          "mode"     // SELECT giu >600ms -> tab Settings
// Phim so (khi muon dan ten bang so thay vi ten game) — xem bang KEYS[]:
//   1=menu 2=up 3=back 4=left 5=ok 6=right 7=option 8=down 9=delete SELECT=mode

// ------------------------------------------------- auto-repeat khi cuon danh sach
#define LC_REPEAT_DELAY_MS   450     // giu phim lau bat dau lap
#define LC_REPEAT_RATE_MS    110     // toc do lap sau do

// ------------------------------------------------- debounce phím
#define LC_DEBOUNCE_MS       25

// ------------------------------------------------- mau mac dinh (RGB565) khi khong co theme.json
// Retro-Go: nen toi, giong EmulationStation thu nho.
#define LC_BG_DEFAULT        0x0010  // xanh dam
#define LC_FG_DEFAULT        0xFFFF  // trang
#define LC_BORDER_DEFAULT    0x6B4D  // xam (dialog border mac dinh theo de bai)
#define LC_DIALOG_BG_DEFAULT 0x0010
#define LC_SEL_BG_DEFAULT    0x39E7  // vang dam — dong dang chon
#define LC_SEL_FG_DEFAULT    0x0000
#define LC_DIM_DEFAULT       0x8C71  // muc disabled / chu xam

// ------------------------------------------------- task tai art
#define LC_ART_TASK_STACK    6144    // stack task tai art (FreeRTOS)
#define LC_ART_QUEUE_LEN     4
#define LC_ART_MAX_FILE      (96 * 1024)   // file PNG lon nhat chap nhan (o PSRAM)

// ------------------------------------------------- khac
#define LC_TITLE_MAX         40      // ten muc dai nhat trong danh sach
#define LC_ITEMS_MAX         32      // so muc toi da 1 tab
#define LC_FAV_MAX           24      // so yeu thich toi da
#define LC_CFG_LINE_MAX      160
