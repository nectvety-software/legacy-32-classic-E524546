// rg_display.h — lớp hiển thị thấp, port ý tưởng retro-go/components/retro-go/rg_display.c.
// Framebuffer RGB565 đặt trong PSRAM; mọi thao tác vẽ ghi vào framebuffer (chế độ
// buffered). rg_display_flush() so checksum TỪNG DÒNG với lần flush trước và chỉ
// đẩy các dãy dòng thay đổi ra ST7789 qua DMA của LovyanGFX -> không nháy, ít SPI.
// Vẽ theo sự kiện (immediate-mode): caller tự gọi flush() sau khi vẽ xong 1 màn.
#pragma once
#include <stdint.h>
#include <stddef.h>

void      rg_display_init();                 // init panel ST7789 + cấp framebuffer PSRAM
void      rg_display_write_rect(int x, int y, int w, int h, const uint16_t *pix); // RGB565 -> fb
void      rg_display_fill_rect(int x, int y, int w, int h, uint16_t color);
uint16_t *rg_display_buffer();               // con trỏ framebuffer (240*320)
int       rg_display_width();
int       rg_display_height();
void      rg_display_flush();                // đẩy dòng dirty (checksum) ra LCD qua DMA
void      rg_display_force_redraw();         // xóa checksum -> flush kế đẩy toàn màn
void      rg_display_set_backlight(uint8_t brightness); // 0..255, PWM LEDK
int       rg_display_count_nonblack();       // chẩn đoán: số pixel khác 0 trong framebuffer
unsigned  rg_display_flush_lines();          // chẩn đoán: tổng số dòng đã đẩy ra panel
