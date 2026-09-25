// launcher.h — API launcher phong cach Retro-Go cho E524546.
//
// 2 lop:
//   * dev_display_* : lop thap — ghi vung hinh chu nhat RGB565 ra ST7789 qua
//     LovyanGFX (co DMA san), quan ly dirty rect.
//   * gui_*         : lop cao — text (proportional, flags), image, list, dialog,
//     tab. Immediate-mode: goi launcher_redraw() khi co su kien, KHONG refresh
//     theo timer.
//
// Theme .vqeaf (VQEAF Theme Studio @vqeaf 1.x) nap tu LittleFS
// /launcher/themes/<ten>.vqeaf — palette RGB565, khong chay code tu file.
// Fallback: theme.json cu neu khong co .vqeaf. Reload khi dang chay.
// Art PNG tu SD/LFS, decode bang PNGdec (da co trong platformio.ini) sang RGB565
// trong PSRAM, task rieng -> khong chan vong lap chinh.
#pragma once
#include <Arduino.h>
#include "launcher_config.h"

// ------------------------------------------------------------- theme (JSON)
struct LcTheme {
  // section dialog
  uint16_t dlg_background, dlg_foreground, dlg_border, dlg_header;
  uint16_t dlg_scrollbar, dlg_shadow, dlg_item_standard, dlg_item_disabled, dlg_item_message;
  // launcher_1..launcher_4 (mỗi tab một tông)
  struct LcTab {
    uint16_t background, foreground;
    uint16_t list_standard_bg, list_standard_fg;
    uint16_t list_selected_bg, list_selected_fg;
  } tab[4];
  char name[32];
};

// ------------------------------------------------------------- item / tab
struct LcItem {
  char title[LC_TITLE_MAX];
  char art[96];        // duong dan art PNG tuong doi /launcher ("" = icon mac dinh)
  bool favorite;
  uint8_t kind;        // 0 = app chay duoc, 1 = thong tin khong chay (item_message)
};

enum LcTabId { LC_TAB_APPS = 0, LC_TAB_GAMES = 1, LC_TAB_FAVORITES = 2, LC_TAB_SETTINGS = 3, LC_TAB_N = 4 };

// ------------------------------------------------------------- vong doi launcher
// mode = true: launcher dieu khien man hinh; false: Qeafbrowser chay binh thuong.
void  launcher_begin(bool start_now);
void  launcher_set_active(bool active);          // chuyen qua lai launcher <-> browser
bool  launcher_active();
void  launcher_key(const char *game_key);        // nhan phim tu keys_poll (ten game)
void  launcher_tick();                           // goi trong loop(): xu ly task art xong
void  launcher_redraw();                         // ve lai toan bo man hinh (immediate-mode)
void  launcher_exit_to_browser();                // chay muc dang chon (Browser) va dong launcher

// trang thai cho harness/sim
int   launcher_tab_index();
int   launcher_item_index();
const char *launcher_item_title(int idx);
void  launcher_set_net(bool up);                 // cap nhat trang thai wifi cho statusbar
void  launcher_set_clock(const char *hhmm);      // cap nhat gio (cache cua browser)

// Chuyen noi dung ve sang buffer khac (cli_shot dump frame_buf). g = lgfx::LovyanGFX* hoac nullptr.
void  launcher_set_target(void *g);

// ------------------------------------------------------------- lop dev_display_* (thap)
void dev_display_init();
void dev_display_write_rect(int x, int y, int w, int h, const uint16_t *pix); // pix = RGB565
void dev_display_fill_rect(int x, int y, int w, int h, uint16_t color);
void dev_display_dirty(int x, int y, int w, int h);   // danh dau vung can day
void dev_display_flush();                              // day cac vung dirty ra LCD

// ------------------------------------------------------------- lop gui_* (cao)
void gui_clear_rect(int x, int y, int w, int h, uint16_t color);
void gui_draw_text(int x, int y, uint16_t fg, uint16_t bg, const char *s, int size = 1, bool transparent_bg = false);
// chu bold S60 (ve 2 lan offset 1px) — dung cho focus label trong launcher grid
void gui_draw_text_bold(int x, int y, uint16_t fg, uint16_t bg, const char *s, int size = 1);
int  gui_text_width(const char *s, int size = 1);
int  gui_font_height(int size = 1);                 // chieu cao glyph (layout)
void gui_draw_image(int x, int y, int w, int h, const uint16_t *pix); // magenta = trong suot
void gui_draw_list(int x, int y, int w, int h, const struct LcItem *items, int n,
                   int selected, int top, const LcTheme::LcTab &t);
bool gui_dialog(const char *title, const char *const *lines, int nlines,
                const char *const *options, int noptions, int *sel);
void gui_statusbar(uint16_t fg, uint16_t bg);        // pin + wifi + gio (tai su dung clock_cache)

// ------------------------------------------------------------- theme / config / art
bool theme_load(const char *name);                   // /launcher/themes/<name>.vqeaf (LittleFS)
void theme_defaults(LcTheme &t);                     // fallback khi khong co file / file hong
const LcTheme &theme_current();
bool theme_next(int dir);                            // doi theme khi dang chay (Settings)
bool cfg_load();                                     // /launcher/launcher.ini
bool cfg_save();
int  cfg_brightness();
const char *cfg_theme_name();
const char *theme_display_name();                    // ten hien thi tu <theme name="...">

// art: khoi dong task + xin decode "path" (tuong doi /launcher). Ket qua ve o
// launcher_tick() de tranh luc lon dia chi.
void art_task_start();
void art_request(const char *rel_path);             // "" = icon mac dinh
bool art_ready(uint16_t **pix, int *w, int *h);      // true neu co anh moi ve duoc
