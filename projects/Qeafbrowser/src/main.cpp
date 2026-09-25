// main.cpp — Qeafbrowser G2: test lớp hiển thị + input + font + theme.
// Màn hình test bố trí ĐÚNG gui.c của Retro-Go (thích ứng dọc 240x320):
//   header 50px: logo 46x50 (trái), banner (phải logo, +8), status (y=34)
//   list căn giữa dọc phần còn lại; footer gợi ý phím.
// Phím: 2/8 cuộn, 4/6 đổi tông launcher_1..4, 7 mở dialog demo, 18(MENU) vẽ lại.
#include <Arduino.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include "rg_config.h"
#include "rg_display.h"
#include "rg_input.h"
#include "rg_gui.h"
#include "rg_theme.h"

static int s_variant = 0;   // launcher_1..4 -> index 0..3
static int s_sel = 0;
static uint32_t s_redraws = 0;   // chẩn đoán: đếm số lần vẽ lại (xem heartbeat)

static const char *DEMO_ITEMS[] = {
  "Đánh dấu trang",
  "Lịch sử duyệt web",
  "Yêu thích của tôi",
  "Đã xem gần đây",
  "Tệp đã tải về",
  "Cài đặt hệ thống",
  "Bàn phím T9 tiếng Việt",
  "Thẻ nhớ SD 32GB",
};
#define DEMO_COUNT 8

// ------------------------------------------------- vẽ màn test (port draw_header + gui_draw_list)
static void draw_screen() {
  s_redraws++;
  const rg_theme_t *th = rg_theme_current();
  const rg_theme_launcher_t &t = th->launcher[s_variant];
  rg_gui_apply_theme();

  // nền toàn màn (nếu background != transparent/none)
  if (t.background != C_TRANSPARENT && t.background != C_NONE)
    rg_display_fill_rect(0, 0, RG_SCREEN_W, RG_SCREEN_H, t.background);

  // ---- header 50px (gui.c draw_header) ----
  // logo 46x50: placeholder có viền (G3 sẽ decode logo_<tab>.png từ theme)
  rg_gui_draw_rect(0, 0, RG_LOGO_WIDTH, RG_LOGO_HEIGHT, 1, t.foreground, C_NONE);
  rg_gui_draw_text(0, 17, RG_LOGO_WIDTH, "Q", t.foreground, C_TRANSPARENT,
                   RG_TEXT_ALIGN_CENTER | RG_TEXT_BIGGER | RG_TEXT_NO_PADDING);
  // banner: tên app phóng to trong vùng banner (G3 dùng banner_<tab>.png 272x24)
  rg_gui_draw_text(RG_BANNER_X, RG_BANNER_Y, RG_BANNER_W, "Qeafbrowser",
                   t.foreground, C_TRANSPARENT,
                   RG_TEXT_ALIGN_CENTER | RG_TEXT_BIGGER);
  // status trái + phải (gui.c: status_x=58, y=34)
  rg_gui_draw_text(RG_STATUS_X, RG_STATUS_Y, RG_SCREEN_W / 2 - RG_STATUS_X, "1 / 8",
                   t.foreground, C_TRANSPARENT, RG_TEXT_ALIGN_LEFT);
  rg_gui_draw_text(RG_SCREEN_W / 2, RG_STATUS_Y, RG_SCREEN_W / 2 - 4,
                   "WiFi  --:--", t.foreground, C_TRANSPARENT, RG_TEXT_ALIGN_RIGHT);

  // ---- list căn giữa dọc (gui_draw_list) ----
  const int fh = rg_gui_font_height(RG_FONT_SMALL);
  const int lh = fh + 2;                 // line_height = font + padding*2
  const int area_h = RG_SCREEN_H - RG_LIST_TOP - fh - 4;
  int list_h = DEMO_COUNT * lh;
  int y = RG_LIST_TOP + (area_h - list_h) / 2;
  if (y < RG_LIST_TOP) y = RG_LIST_TOP;
  for (int i = 0; i < DEMO_COUNT; i++) {
    bool sel = (i == s_sel);
    rg_color_t fg = sel ? t.list_selected_fg : t.list_standard_fg;
    rg_color_t bg = sel ? t.list_selected_bg : t.list_standard_bg;
    if (bg != C_TRANSPARENT && bg != C_NONE)
      rg_display_fill_rect(0, y, RG_SCREEN_W, lh, bg);
    rg_gui_draw_text(8, y, RG_SCREEN_W - 16, DEMO_ITEMS[i], fg,
                     C_TRANSPARENT, RG_TEXT_ALIGN_LEFT | RG_TEXT_NO_PADDING);
    y += lh;
  }
  // ---- footer gợi ý phím ----
  // Lưu ý: KHÔNG dùng RG_GUI_BOTTOM - 2. vpos() chỉ nhận đúng giá trị RG_GUI_BOTTOM;
  // biểu thức số học trên nó thành số dương khổng lồ (0x800000-2) => vẽ ra ngoài màn,
  // footer không bao giờ hiện. Tính y tuyệt đối.
  const int footer_h = fh + 2;
  rg_gui_draw_text(4, RG_SCREEN_H - footer_h - 2, RG_SCREEN_W - 8,
                   "2/8: cuon  4/6: tong  7: hop thoai",
                   t.foreground, C_TRANSPARENT, RG_TEXT_ALIGN_LEFT | RG_TEXT_NO_PADDING);

  rg_display_flush();
}

// ------------------------------------------------- dialog demo
static int s_volume = 7;
static rg_gui_event_t cb_volume(rg_gui_option_t *opt, rg_gui_event_t event) {
  if (event == RG_DIALOG_PREV && s_volume > 0) s_volume--;
  if (event == RG_DIALOG_NEXT && s_volume < 10) s_volume++;
  snprintf(opt->value, 8, "%d", s_volume);
  return RG_DIALOG_VOID;
}
static rg_gui_event_t cb_theme(rg_gui_option_t *opt, rg_gui_event_t event) {
  if (event == RG_DIALOG_PREV || event == RG_DIALOG_NEXT) {
    s_variant = (s_variant + (event == RG_DIALOG_NEXT ? 1 : 3)) % 4;
  }
  snprintf(opt->value, 16, "launcher_%d", s_variant + 1);
  return RG_DIALOG_VOID;
}

static void open_demo_dialog() {
  rg_gui_option_t options[] = {
    {0, "Âm lượng", (char *)"", RG_DIALOG_FLAG_NORMAL, cb_volume},
    {1, "Tông màu", (char *)"", RG_DIALOG_FLAG_NORMAL, cb_theme},
    {2, "WiFi (G4 mới có)", NULL, RG_DIALOG_FLAG_DISABLED, NULL},
    {3, "Đây là hộp thoại chuẩn Retro-Go: viền, tiêu đề, mục 2 cột, cuộn có mũi tên.",
       NULL, RG_DIALOG_FLAG_MESSAGE, NULL},
    {4, "Về Qeafbrowser", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
    RG_DIALOG_END,
  };
  intptr_t r = rg_gui_dialog("Tùy chọn", options, 0);
  if (r == 4) {
    rg_gui_draw_message("Qeafbrowser G2\nESP32-S3 + ST7789\n240x320 dọc");
    rg_display_flush();
    rg_input_wait_release(RG_KEY_ALL);
    while (!rg_input_read()) delay(20);
    rg_display_force_redraw();
  }
}

// ------------------------------------------------- Arduino
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[G2] Qeafbrowser test display/input/font/theme");
  Serial.printf("[G2] reset_reason=%d psram=%u\n", (int)esp_reset_reason(),
                (unsigned)ESP.getPsramSize());
  Serial.flush();

  Serial.println("[G2] step: LittleFS.begin"); Serial.flush();
  LittleFS.begin(true);
  bool sd = false;
#ifndef RG_DIAG_NO_SD
  // Switch chẩn đoán (không ảnh hưởng build thường): thêm
  //   PLATFORMIO_BUILD_FLAGS="-D RG_DIAG_NO_SD=1"
  // để bỏ qua khởi động thẻ SD. Dùng để tách nguyên nhân khi CH340 rớt khỏi USB
  // ngay sau SD_MMC.begin() (nghi sụt áp do dòng khởi động của thẻ).
  Serial.println("[G2] step: SD_MMC.begin"); Serial.flush();
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_DAT0);
  sd = SD_MMC.begin("/sdcard", true);   // 1-bit mode (E524546)
#else
  Serial.println("[G2] DIAG: bo qua SD_MMC.begin (RG_DIAG_NO_SD)"); Serial.flush();
#endif
  Serial.printf("[G2] SD: %s\n", sd ? "OK" : "khong co the (dung LittleFS)");
  Serial.flush();
  rg_theme_set_sd_mounted(sd);

  Serial.println("[G2] step: rg_display_init"); Serial.flush();
  rg_display_init();
  Serial.println("[G2] step: rg_input_init"); Serial.flush();
  rg_input_init();
  delay(30);   // để mức phím ổn định sau pinMode
  Serial.printf("[G2] keypad raw=0x%03X (0 = khong phim nao bi ket)\n",
                (unsigned)rg_input_raw());
  Serial.flush();
  Serial.println("[G2] step: rg_theme_load"); Serial.flush();
  rg_theme_load(RG_THEME_DEFAULT);      // SD -> LittleFS -> mặc định build-in
  Serial.println("[G2] step: rg_gui_init"); Serial.flush();
  rg_gui_init();
  Serial.println("[G2] step: draw_screen"); Serial.flush();
  rg_display_force_redraw();
  draw_screen();
  // Chứng minh UI có vẽ thật: số pixel khác nền trong framebuffer + số dòng đã đẩy
  // ra panel. Nếu nonblack > 0 mà màn vẫn trắng/đen trơn => lỗi panel/backlight,
  // không phải lỗi lớp vẽ.
  Serial.printf("[G2] fb nonblack=%d/%d, flush_lines=%u\n",
                rg_display_count_nonblack(), RG_FB_PIXELS,
                rg_display_flush_lines());
  Serial.println("[G2] setup done"); Serial.flush();
}

void loop() {
  // Heartbeat tạm thời (G2): xác nhận app còn sống kể cả khi port USB rớt/rớt lại.
  // Kèm bộ đếm redraw: nếu tăng liên tục mà không ai bấm phím => phím bị kẹt/GPIO
  // floating gây redraw liên tục (đúng triệu chứng "nhấp nháy"). Gỡ khi sang G3.
  static uint32_t t_last = 0;
  if (millis() - t_last >= 3000) {
    t_last = millis();
    Serial.printf("[G2] alive t=%lus heap=%u psram_free=%u redraws=%lu raw=0x%03X\n",
                  (unsigned long)(millis() / 1000), (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getFreePsram(), (unsigned long)s_redraws,
                  (unsigned)rg_input_raw());
    Serial.flush();
  }

  uint32_t keys = rg_input_read();
  if (!keys) { delay(10); return; }

  bool need_redraw = true;
  if (keys & RG_KEY_UP)     { if (--s_sel < 0) s_sel = DEMO_COUNT - 1; }
  else if (keys & RG_KEY_DOWN)  { if (++s_sel >= DEMO_COUNT) s_sel = 0; }
  else if (keys & RG_KEY_LEFT)  { s_variant = (s_variant + 3) % 4; }
  else if (keys & RG_KEY_RIGHT) { s_variant = (s_variant + 1) % 4; }
  else if (keys & RG_KEY_OPTION) { open_demo_dialog(); }
  else if (keys & RG_KEY_MODE)   { Serial.printf("[G2] T9 mode: %s\n",
                                    rg_input_t9_mode() ? "ON" : "OFF"); }
  else if (keys & RG_KEY_MENU)   { rg_display_force_redraw(); }
  else need_redraw = false;

  char t9 = rg_input_read_char();
  if (t9) Serial.printf("[G2] T9 char: %c\n", t9);
  if (need_redraw) draw_screen();
}


