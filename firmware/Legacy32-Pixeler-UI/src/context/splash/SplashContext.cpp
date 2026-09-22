#include "SplashContext.h"

#include <Arduino.h>

#include "../WidgetCreator.h"
#include "manager/FileManager.h"
#include "manager/SettingsManager.h"
#include "manager/WiFiManager.h"

#define SHOW_INIT_TIME 1400UL

SplashContext::SplashContext()
{
  _display.enableBackLight();
  _start_time = millis();

  EmptyLayout* layout = WidgetCreator::getEmptyLayout();
  setLayout(layout);

  Label* title = createLabel(1, "PIXELER", 10, 22, 220, 44,
                             COLOR_ACCENT, COLOR_MAIN_BACK, font_10x20, 10);
  title->setAlign(IWidget::ALIGN_CENTER);

  Label* board = createLabel(2, "Legacy-32-Classic E524546", 10, 75, 220, 26,
                             COLOR_PANEL_BACK, COLOR_TEXT_PRIMARY, font_5x7, 7);
  board->setAlign(IWidget::ALIGN_CENTER);

  const bool sd_ok = _fs.mount();
  if (sd_ok)
  {
    String bright = SettingsManager::get(STR_PREF_BRIGHT);
    _display.setBrightness((bright.isEmpty() || bright.equals("0")) ? 240 : atoi(bright.c_str()));
  }
  else
  {
    _display.setBrightness(240);
  }

  const bool psram_ok = psramInit();

  Label* display = createLabel(3, "TFT ST7789  240x320", 18, 122, 204, 34,
                               COLOR_CARD_BACK, COLOR_TEXT_PRIMARY, font_5x7, 7);
  display->setAlign(IWidget::ALIGN_CENTER);

  Label* sd = createLabel(4, sd_ok ? "SD CARD       OK" : "SD CARD       LOI",
                          18, 164, 204, 34, COLOR_CARD_BACK,
                          sd_ok ? COLOR_ACCENT_2 : COLOR_DANGER, font_5x7, 7);
  sd->setAlign(IWidget::ALIGN_CENTER);

  Label* psram = createLabel(5, psram_ok ? "PSRAM 8MB     OK" : "PSRAM         LOI",
                             18, 206, 204, 34, COLOR_CARD_BACK,
                             psram_ok ? COLOR_ACCENT_2 : COLOR_DANGER, font_5x7, 7);
  psram->setAlign(IWidget::ALIGN_CENTER);

  Label* footer = createLabel(6, "DANG KHOI DONG HE THONG...", 18, 270, 204, 24,
                              COLOR_MAIN_BACK, COLOR_TEXT_MUTED, font_5x7, 0);
  footer->setAlign(IWidget::ALIGN_CENTER);
}

Label* SplashContext::createLabel(uint16_t id,
                                  const char* text,
                                  uint16_t x,
                                  uint16_t y,
                                  uint16_t width,
                                  uint16_t height,
                                  uint16_t back_color,
                                  uint16_t text_color,
                                  const uint8_t* font,
                                  uint8_t radius)
{
  Label* label = new Label(id);
  getLayout()->addWidget(label);
  label->setText(text);
  label->setPos(x, y);
  label->setWidth(width);
  label->setHeight(height);
  label->setBackColor(back_color);
  label->setTextColor(text_color);
  label->setFont(font);
  label->setGravity(IWidget::GRAVITY_CENTER);
  label->setCornerRadius(radius);
  return label;
}

bool SplashContext::loop()
{
  return true;
}

void SplashContext::update()
{
  if (millis() - _start_time <= SHOW_INIT_TIME)
    return;

  if (_fs.isMounted())
  {
    String wifi_autoconn = SettingsManager::get(STR_PREF_WIFI_AUTOCONNECT, STR_WIFI_SUBDIR);
    if (wifi_autoconn.equals("1"))
    {
      String last_ssid = SettingsManager::get(STR_PREF_WIFI_LAST_SSID, STR_WIFI_SUBDIR);
      if (!last_ssid.isEmpty())
      {
        String last_ssid_pwd = SettingsManager::get(last_ssid.c_str(), STR_WIFI_SUBDIR);
        _wifi.enable();
        _wifi.tryConnectTo(last_ssid, last_ssid_pwd);
      }
    }
  }

  openContextByID(ContextID::ID_CONTEXT_HOME);
}
