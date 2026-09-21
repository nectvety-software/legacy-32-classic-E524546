#include "HomeContext.h"

#include <Arduino.h>

#include "../WidgetCreator.h"
#include "manager/FileManager.h"

#define STATUS_UPDATE_INTERVAL_MS 1000UL

HomeContext::HomeContext()
{
  EmptyLayout* layout = WidgetCreator::getEmptyLayout();
  setLayout(layout);

  // Symbian S60 operator/title bar
  Label* header = createLabel(ID_HEADER, "LEGACY-32", 0, 0, UI_WIDTH, 26,
                              COLOR_PANEL_BACK, COLOR_WHITE, font_10x20, 0);
  header->setAlign(IWidget::ALIGN_START);
  header->setHPadding(8);

  Label* badge = createLabel(ID_HEADER_BADGE, "S3", 204, 4, 30, 18,
                             COLOR_MAIN_BACK, COLOR_ACCENT, font_5x7, 4);
  badge->setAlign(IWidget::ALIGN_CENTER);

  Label* device = createLabel(ID_DEVICE, "Pixeler UI  -  ESP32-S3", 0, 34, UI_WIDTH, 18,
                              COLOR_MAIN_BACK, COLOR_TEXT_MUTED, font_5x7, 0);
  device->setAlign(IWidget::ALIGN_CENTER);

  // Big idle clock (uptime)
  _status_lbl = createLabel(ID_STATUS, "00:00:00", 0, 104, UI_WIDTH, 56,
                            COLOR_MAIN_BACK, COLOR_ACCENT, font_inr30, 0);
  _status_lbl->setAlign(IWidget::ALIGN_CENTER);

  Label* clock_cap = createLabel(ID_HERO, "Thoi gian hoat dong", 0, 166, UI_WIDTH, 16,
                                 COLOR_MAIN_BACK, COLOR_TEXT_MUTED, font_5x7, 0);
  clock_cap->setAlign(IWidget::ALIGN_CENTER);

  _storage_lbl = createLabel(ID_STORAGE, "SD\nDang kiem tra", 8, 200, 110, 52,
                             COLOR_CARD_BACK, COLOR_TEXT_PRIMARY, font_5x7, 6);
  _storage_lbl->setAlign(IWidget::ALIGN_CENTER);
  _storage_lbl->setMultiline(true);

  _memory_lbl = createLabel(ID_MEMORY, "PSRAM\nDang kiem tra", 122, 200, 110, 52,
                            COLOR_CARD_BACK, COLOR_TEXT_PRIMARY, font_5x7, 6);
  _memory_lbl->setAlign(IWidget::ALIGN_CENTER);
  _memory_lbl->setMultiline(true);

  Label* shortcut_a = createLabel(ID_SHORTCUT_A, "A  Tro choi", 8, 260, 110, 28,
                                  COLOR_CARD_ALT, COLOR_ACCENT, font_5x7, 6);
  shortcut_a->setAlign(IWidget::ALIGN_CENTER);

  Label* shortcut_start = createLabel(ID_SHORTCUT_START, "START  Tep", 122, 260, 110, 28,
                                      COLOR_CARD_ALT, COLOR_ACCENT, font_5x7, 6);
  shortcut_start->setAlign(IWidget::ALIGN_CENTER);

  layout->addWidget(WidgetCreator::getSoftkeyBar(ID_FOOTER, "Menu", "Tep"));

  refreshStatus();
}

Label* HomeContext::createLabel(uint16_t id,
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
  label->setAlign(IWidget::ALIGN_START);
  label->setHPadding(6);
  label->setCornerRadius(radius);
  return label;
}

bool HomeContext::loop()
{
  return true;
}

void HomeContext::update()
{
  if (_input.isReleased(BtnID::BTN_OK) || _input.isReleased(BtnID::BTN_MENU))
  {
    _input.lock(BtnID::BTN_OK, CLICK_LOCK);
    _input.lock(BtnID::BTN_MENU, CLICK_LOCK);
    openMenu();
    return;
  }

  if (_input.isReleased(BtnID::BTN_A))
  {
    _input.lock(BtnID::BTN_A, CLICK_LOCK);
    openContextByID(ID_CONTEXT_GAMES);
    return;
  }

  if (_input.isReleased(BtnID::BTN_START))
  {
    _input.lock(BtnID::BTN_START, CLICK_LOCK);
    openContextByID(ID_CONTEXT_FILES);
    return;
  }

  if (_input.isReleased(BtnID::BTN_OPTION))
  {
    _input.lock(BtnID::BTN_OPTION, CLICK_LOCK);
    openContextByID(ID_CONTEXT_PREF_SEL);
    return;
  }

  if (millis() - _upd_timer >= STATUS_UPDATE_INTERVAL_MS)
  {
    _upd_timer = millis();
    refreshStatus();
  }
}

void HomeContext::openMenu()
{
  openContextByID(ID_CONTEXT_MENU);
}

void HomeContext::refreshStatus()
{
  if (_storage_lbl)
  {
    _storage_lbl->setText(_fs.isMounted() ? "SD\nSAN SANG" : "SD\nCHUA GAN");
    _storage_lbl->setTextColor(_fs.isMounted() ? COLOR_ACCENT_2 : COLOR_WARNING);
  }

  const uint32_t psram_mb = ESP.getPsramSize() / (1024U * 1024U);
  if (_memory_lbl)
  {
    String psram_text("PSRAM\n");
    psram_text += psram_mb;
    psram_text += " MB";
    _memory_lbl->setText(psram_text);
    _memory_lbl->setTextColor(psram_mb > 0 ? COLOR_ACCENT_2 : COLOR_DANGER);
  }

  if (_status_lbl)
  {
    const uint32_t total_seconds = millis() / 1000UL;
    const uint32_t hours = total_seconds / 3600UL;
    const uint32_t minutes = (total_seconds / 60UL) % 60UL;
    const uint32_t seconds = total_seconds % 60UL;

    char clock[16];
    snprintf(clock, sizeof(clock), "%02lu:%02lu:%02lu",
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds));
    _status_lbl->setText(clock);
  }
}
