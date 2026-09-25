// Cấu hình LovyanGFX cho ST7789 240x320 + ESP32-S3 với đúng chân ở pins.h
#pragma once
// QUAN TRỌNG (LovyanGFX "include order dependent extension"): FS.h / LittleFS.h /
// SD_MMC.h phải được include TRƯỚC LovyanGFX.hpp, nếu không các overload
// drawPngFile/drawJpgFile/drawBmpFile cho LittleFS/SD sẽ bị biên dịch thiếu
// (lỗi DataWrapperT abstract).
#include <FS.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include "pins.h"

class LGFX_ESP32S3_ST7789 : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789   _panel;
  lgfx::Bus_SPI        _bus;
  lgfx::Light_PWM      _light;

public:
  LGFX_ESP32S3_ST7789() {
    {
      auto cfg = _bus.config();
      cfg.spi_host   = SPI2_HOST;      // match E524546-OS reference
      cfg.spi_mode   = 0;
      cfg.freq_write = 20000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = TFT_SCL;
      cfg.pin_mosi = TFT_SDA;
      cfg.pin_miso = -1;
      cfg.pin_dc   = TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs   = TFT_CS;
      // GPIO3 = hardware reset của panel (DESIGN.txt, docs/system_prompt_phan_cung.md).
      // BẮT BUỘC phải đấu: mọi project khác trong repo (Qeafbrowser_v1/v1.4/v1.7,
      // firmware/E524546-OS, pochita) đều dùng TFT_RESET. Bản trước đặt -1 với lý do
      // "suspected COM6 drop" -> panel không bao giờ được reset cứng, ST7789 vào trạng
      // thái không xác định => màn nhấp nháy/không hiện gì. COM drop vẫn xảy ra cả khi
      // đã bỏ GPIO3, nên nghi ngờ đó là chẩn đoán sai.
      cfg.pin_rst  = TFT_RESET;
      cfg.pin_busy = -1;
      cfg.memory_width  = TFT_WIDTH;
      cfg.memory_height = TFT_HEIGHT;
      cfg.panel_width   = TFT_WIDTH;
      cfg.panel_height  = TFT_HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable = false;
      cfg.invert   = true;      // ST7789 2 inch thường cần invert
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = TFT_LEDK;
      cfg.invert = false;
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};
