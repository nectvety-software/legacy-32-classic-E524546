// rg_display.cpp — cài đặt lớp hiển thị thấp (xem rg_display.h).
// Checksum-theo-dòng bắt chước rg_display.c của Retro-Go (screen_line_checksum[]):
// mỗi dòng 240 px có 1 hash; flush() gom các dòng dirty liền nhau thành 1 lần
// pushImage (LovyanGFX tự DMA) -> tối thiểu giao dịch SPI.
#include "rg_display.h"
#include "rg_config.h"
#include "pins.h"
#include "LGFX_ESP32S3_ST7789.h"
#include <esp_heap_caps.h>
#include <string.h>

LGFX_ESP32S3_ST7789 rg_lcd;   // panel dùng chung toàn app

static uint16_t *s_fb = nullptr;                    // framebuffer PSRAM
static uint32_t  s_line_crc[RG_SCREEN_H];           // checksum mỗi dòng lần flush trước
static bool      s_fb_ok = false;
static unsigned  s_flush_lines = 0;                 // chẩn đoán: tổng dòng đã đẩy ra panel

// FNV-1a 32-bit trên 1 dòng pixel (đủ nhanh, phân tán tốt cho ảnh RGB565)
static inline uint32_t rg_hash_line(const uint16_t *line) {
  uint32_t h = 2166136261u;
  const uint8_t *p = (const uint8_t *)line;
  for (int i = 0; i < RG_SCREEN_W * 2; i++) { h ^= p[i]; h *= 16777619u; }
  return h;
}

// Log từng bước + flush: khi panel/bus init treo thì biết chính xác dừng ở đâu
// (Serial.flush() là bắt buộc — UART TX mất dữ liệu đang chờ nếu code treo ngay sau).
#define RG_STEP(msg) do { Serial.println(msg); Serial.flush(); } while (0)

// ------------------------------------------------- tự kiểm tra panel
// Đổ lần lượt 4 màu toàn màn để tách bạch hai loại lỗi rất khác nhau:
//   - thấy 4 màu nhấp nháy  -> panel + backlight + SPI OK, lỗi nằm ở lớp vẽ UI
//   - không thấy gì         -> lỗi panel / backlight / đấu dây / chân reset
// Đặt RG_DISPLAY_SELFTEST 0 để tắt (không ảnh hưởng logic hiển thị).
#ifndef RG_DISPLAY_SELFTEST
#define RG_DISPLAY_SELFTEST 1
#endif

#if RG_DISPLAY_SELFTEST
static void display_selftest() {
  static const uint16_t cols[4] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };
  static const char *names[4] = { "do", "xanh la", "xanh duong", "trang" };
  for (int i = 0; i < 4; i++) {
    rg_lcd.fillScreen(cols[i]);
    Serial.printf("[rg_display] selftest %d/4: %s\n", i + 1, names[i]);
    Serial.flush();
    delay(400);
  }
  rg_lcd.fillScreen(0x0000);
  Serial.println("[rg_display] selftest xong (nen ve den)");
  Serial.flush();
}
#endif

void rg_display_init() {
  RG_STEP("[rg_display] step 1/6 lcd.init()");
  bool lcd_ok = rg_lcd.init();
  Serial.printf("[rg_display] lcd.init()=%d panel=%dx%d\n",
                (int)lcd_ok, rg_lcd.width(), rg_lcd.height());
  Serial.flush();
  RG_STEP("[rg_display] step 2/6 setRotation");
  rg_lcd.setRotation(RG_SCREEN_ROTATION);   // dọc 240x320
  Serial.printf("[rg_display] sau setRotation(%d): %dx%d\n",
                (int)RG_SCREEN_ROTATION, rg_lcd.width(), rg_lcd.height());
  Serial.flush();
  RG_STEP("[rg_display] step 3/6 fillScreen");
  rg_lcd.fillScreen(0x0000);
  RG_STEP("[rg_display] step 4/6 backlight");
  rg_display_set_backlight(RG_BRIGHTNESS_DEFAULT);
#if RG_DISPLAY_SELFTEST
  display_selftest();
#endif

  RG_STEP("[rg_display] step 5/6 framebuffer alloc");
  s_fb = (uint16_t *)heap_caps_malloc(RG_FB_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_fb) s_fb = (uint16_t *)malloc(RG_FB_BYTES);   // fallback SRAM (không mong đợi)
  s_fb_ok = (s_fb != nullptr);
  if (s_fb_ok) memset(s_fb, 0, RG_FB_BYTES);
  rg_display_force_redraw();
  Serial.printf("[rg_display] framebuffer %u bytes %s\n", (unsigned)RG_FB_BYTES,
                s_fb_ok ? "PSRAM" : "ALLOC-FAIL");
  Serial.printf("[rg_display] heap=%u maxblk=%u psram=%u psram_free=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
  RG_STEP("[rg_display] step 6/6 done");
}

uint16_t *rg_display_buffer() { return s_fb; }
int rg_display_width()  { return RG_SCREEN_W; }
int rg_display_height() { return RG_SCREEN_H; }

void rg_display_write_rect(int x, int y, int w, int h, const uint16_t *pix) {
  if (!s_fb || !pix || w <= 0 || h <= 0) return;
  const int src_stride = w;                     // stride của ảnh nguồn (pixel)
  if (x < 0) { pix += -x; w += x; x = 0; }
  if (y < 0) { pix += (size_t)(-y) * src_stride; h += y; y = 0; }
  if (x + w > RG_SCREEN_W) w = RG_SCREEN_W - x;
  if (y + h > RG_SCREEN_H) h = RG_SCREEN_H - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++)
    memcpy(&s_fb[(y + j) * RG_SCREEN_W + x], &pix[(size_t)j * src_stride], w * 2);
}

void rg_display_fill_rect(int x, int y, int w, int h, uint16_t color) {
  if (!s_fb || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > RG_SCREEN_W) w = RG_SCREEN_W - x;
  if (y + h > RG_SCREEN_H) h = RG_SCREEN_H - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++) {
    uint16_t *row = &s_fb[(y + j) * RG_SCREEN_W + x];
    for (int i = 0; i < w; i++) row[i] = color;
  }
}

void rg_display_flush() {
  if (!s_fb) return;
  int pushed = 0;
  rg_lcd.startWrite();
  int y = 0;
  while (y < RG_SCREEN_H) {
    if (rg_hash_line(&s_fb[y * RG_SCREEN_W]) == s_line_crc[y]) { y++; continue; }
    int y0 = y;   // đầu dãy dirty
    while (y < RG_SCREEN_H) {
      uint32_t h = rg_hash_line(&s_fb[y * RG_SCREEN_W]);
      if (h == s_line_crc[y]) break;
      s_line_crc[y] = h;
      y++;
    }
    // đẩy dãy [y0, y) ra LCD — DMA bên trong LovyanGFX
    rg_lcd.pushImage(0, y0, RG_SCREEN_W, y - y0, &s_fb[y0 * RG_SCREEN_W]);
    pushed += y - y0;
  }
  rg_lcd.endWrite();
  if (pushed) {
    s_flush_lines += pushed;
    Serial.printf("[rg_display] flush: %d dong -> panel (tong %u)\n",
                  pushed, (unsigned)s_flush_lines);
    Serial.flush();
  }
}

// Chẩn đoán: đếm pixel khác 0 trong framebuffer. Nếu sau draw_screen() mà = 0 thì
// lớp vẽ UI không ghi được gì (lỗi logic/theme), khác hẳn lỗi panel.
int rg_display_count_nonblack() {
  if (!s_fb) return -1;
  int n = 0;
  for (int i = 0; i < RG_FB_PIXELS; i++) if (s_fb[i]) n++;
  return n;
}

unsigned rg_display_flush_lines() { return s_flush_lines; }

void rg_display_force_redraw() {
  memset(s_line_crc, 0, sizeof(s_line_crc));
}

void rg_display_set_backlight(uint8_t brightness) {
  rg_lcd.setBrightness(brightness);
}
