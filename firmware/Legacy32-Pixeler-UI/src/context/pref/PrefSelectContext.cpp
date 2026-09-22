#include "PrefSelectContext.h"

#include "../WidgetCreator.h"
#include "bright/PrefBrightContext.h"
#include "file_server/PrefFileServerContext.h"
#include "manager/SettingsManager.h"
#include "wifi_power/PrefWiFiPowerContext.h"
#include "widget/menu/item/ToggleItem.h"
#include "widget/toggle/ToggleSwitch.h"

static const char STR_WIFI_AUTOCONNECT[] = "Tu dong ket noi WiFi";
static const char STR_FILE_SERVER[] = "May chu tep";

void PrefSelectContext::showSDErrTmpl()
{
  _mode = MODE_SD_UNCONN;

  EmptyLayout* layout = WidgetCreator::getEmptyLayout();
  setLayout(layout);
  layout->addWidget(WidgetCreator::getStatusMsgLable(ID_ERR_LBL, STR_SD_ERR));
}

PrefSelectContext::PrefSelectContext()
{
  if (!_fs.isMounted())
  {
    showSDErrTmpl();
    return;
  }

  showMainTmpl();
}

void PrefSelectContext::showMainTmpl()
{
  EmptyLayout* layout = WidgetCreator::getEmptyLayout();
  setLayout(layout);

  _menu = new FixedMenu(ID_MENU);
  layout->addWidget(_menu);
  _menu->setBackColor(COLOR_MAIN_BACK);
  _menu->setWidth(UI_WIDTH - 8);
  _menu->setHeight(UI_HEIGHT - 8);
  _menu->setPos(4, 4);
  _menu->setItemHeight(60);
  _menu->setItemsSpacing(3);
  _menu->setLoopState(true);

  MenuItem* bright_item = WidgetCreator::getMenuItem(ITEM_ID_BRIGHT);
  _menu->addItem(bright_item);
  bright_item->setCornerRadius(8);
  bright_item->setLbl(WidgetCreator::getItemLabel(STR_BRIGHT, font_10x20));

  ToggleItem* wifi_autoconn_item = new ToggleItem(ITEM_ID_WIFI_AUTOCONN);
  _menu->addItem(wifi_autoconn_item);
  wifi_autoconn_item->setBackColor(COLOR_MENU_ITEM);
  wifi_autoconn_item->setFocusBorderColor(COLOR_ACCENT);
  wifi_autoconn_item->setFocusBackColor(COLOR_FOCUS_BACK);
  wifi_autoconn_item->setChangingBorder(true);
  wifi_autoconn_item->setChangingBack(true);
  wifi_autoconn_item->setCornerRadius(8);
  wifi_autoconn_item->setLbl(WidgetCreator::getItemLabel(STR_WIFI_AUTOCONNECT, font_10x20));

  ToggleSwitch* wifi_toggle = new ToggleSwitch(ID_TOGGLE);
  wifi_autoconn_item->setToggle(wifi_toggle);
  wifi_toggle->setWidth(40);
  wifi_toggle->setHeight(20);
  wifi_toggle->setCornerRadius(7);

  String wifi_autoconn = SettingsManager::get(STR_PREF_WIFI_AUTOCONNECT, STR_WIFI_SUBDIR);
  wifi_toggle->setOn(wifi_autoconn.equals("1"));

  MenuItem* file_server_item = WidgetCreator::getMenuItem(ITEM_ID_FILE_SERVER);
  _menu->addItem(file_server_item);
  file_server_item->setCornerRadius(8);
  file_server_item->setLbl(WidgetCreator::getItemLabel(STR_FILE_SERVER, font_10x20));

  MenuItem* wifi_power_item = WidgetCreator::getMenuItem(ITEM_ID_WIFI_POWER);
  _menu->addItem(wifi_power_item);
  wifi_power_item->setCornerRadius(8);
  wifi_power_item->setLbl(WidgetCreator::getItemLabel(STR_WIFI_POWER, font_10x20));

  _scrollbar = new ScrollBar(ID_SCROLLBAR);
  layout->addWidget(_scrollbar);
  _scrollbar->setWidth(3);
  _scrollbar->setHeight(UI_HEIGHT - 16);
  _scrollbar->setPos(UI_WIDTH - 4, 8);
  _scrollbar->setBackColor(COLOR_PANEL_BACK);
  _scrollbar->setSliderColor(COLOR_ACCENT);
  _scrollbar->setMax(_menu->getSize());
}

bool PrefSelectContext::loop()
{
  if (_mode == MODE_SUBCONTEXT)
  {
    if (!_sub_context->isReleased())
    {
      _sub_context->tick();
      return false;
    }

    delete _sub_context;
    _sub_context = nullptr;
    _mode = MODE_NORMAL;
    showMainTmpl();
    return true;
  }

  return true;
}

void PrefSelectContext::update()
{
  if (_mode == MODE_SD_UNCONN)
  {
    if (_input.isReleased(BtnID::BTN_BACK) || _input.isReleased(BtnID::BTN_MENU))
    {
      _input.lock(BtnID::BTN_BACK, CLICK_LOCK);
      _input.lock(BtnID::BTN_MENU, CLICK_LOCK);
      openContextByID(ID_CONTEXT_MENU);
    }
    return;
  }

  if (_input.isReleased(BtnID::BTN_OK) || _input.isReleased(BtnID::BTN_A))
  {
    _input.lock(BtnID::BTN_OK, CLICK_LOCK);
    _input.lock(BtnID::BTN_A, CLICK_LOCK);
    ok();
  }
  else if (_input.isReleased(BtnID::BTN_BACK) || _input.isReleased(BtnID::BTN_MENU))
  {
    _input.lock(BtnID::BTN_BACK, CLICK_LOCK);
    _input.lock(BtnID::BTN_MENU, CLICK_LOCK);
    openContextByID(ID_CONTEXT_MENU);
  }
  else if (_input.isHolded(BtnID::BTN_UP))
  {
    _input.lock(BtnID::BTN_UP, HOLD_LOCK);
    _menu->focusUp();
    _scrollbar->setValue(_menu->getCurrFocusPos());
  }
  else if (_input.isHolded(BtnID::BTN_DOWN))
  {
    _input.lock(BtnID::BTN_DOWN, HOLD_LOCK);
    _menu->focusDown();
    _scrollbar->setValue(_menu->getCurrFocusPos());
  }
}

void PrefSelectContext::ok()
{
  const uint16_t id = _menu->getCurrItemID();

  if (id == ITEM_ID_WIFI_AUTOCONN)
  {
    ToggleItem* toggle = _menu->getCurrItem()->castTo<ToggleItem>();
    const bool enable = !toggle->isOn();
    if (SettingsManager::set(STR_PREF_WIFI_AUTOCONNECT, enable ? "1" : "0", STR_WIFI_SUBDIR))
      toggle->setOn(enable);
  }
  else if (id == ITEM_ID_BRIGHT)
  {
    _mode = MODE_SUBCONTEXT;
    getLayout()->delWidgets();
    _sub_context = new PrefBrightContext();
  }
  else if (id == ITEM_ID_FILE_SERVER)
  {
    _mode = MODE_SUBCONTEXT;
    getLayout()->delWidgets();
    _sub_context = new PrefFileServerContext();
  }
  else if (id == ITEM_ID_WIFI_POWER)
  {
    _mode = MODE_SUBCONTEXT;
    getLayout()->delWidgets();
    _sub_context = new PrefWiFiPowerContext();
  }
}
