#include "MenuContext.h"

#include "../WidgetCreator.h"
#include "widget/image/Image.h"
#include "widget/layout/EmptyLayout.h"
#include "widget/menu/item/MenuItem.h"

#define ICON_SIZE 40
#define GRID_TOP 26
#define GRID_HEIGHT (UI_HEIGHT - GRID_TOP - 22)
#define GRID_ITEM_HEIGHT 120
#define GRID_SPACING 6

const char STR_FILES_ITEM[] = "Tep";
const char STR_GAME_ITEM[] = "Tro choi";
const char STR_READER_ITEM[] = "Doc";
const char STR_WIFI_ITEM[] = "WiFi";
const char STR_SETTINGS_ITEM[] = "Cai dat";
const char STR_FIRMWARE_ITEM[] = "Firmware";

uint8_t MenuContext::_last_sel_item_pos = 0;

MenuContext::MenuContext()
{
  EmptyLayout* layout = WidgetCreator::getEmptyLayout();
  setLayout(layout);

  layout->addWidget(WidgetCreator::getWindowHeader(ID_HEADER, "Ung dung"));

  _menu = new GridMenu(ID_MENU);
  layout->addWidget(_menu);
  _menu->setBackColor(COLOR_MAIN_BACK);
  _menu->setWidth(UI_WIDTH);
  _menu->setHeight(GRID_HEIGHT);
  _menu->setPos(0, GRID_TOP);
  _menu->setColumns(3);
  _menu->setItemsSpacing(GRID_SPACING);
  _menu->setItemHeight(GRID_ITEM_HEIGHT);

  addMenuItem(ID_CONTEXT_FILES, STR_FILES_ITEM, Image::ICON_SD);
  addMenuItem(ID_CONTEXT_GAMES, STR_GAME_ITEM, Image::ICON_GAMEPAD);
  addMenuItem(ID_CONTEXT_READER, STR_READER_ITEM, Image::ICON_BOOK);
  addMenuItem(ID_CONTEXT_WIFI, STR_WIFI_ITEM, Image::ICON_WIFI);
  addMenuItem(ID_CONTEXT_PREF_SEL, STR_SETTINGS_ITEM, Image::ICON_GEAR);
  addMenuItem(ID_CONTEXT_FIRMWARE, STR_FIRMWARE_ITEM, Image::ICON_CHIP);

  if (_last_sel_item_pos >= _menu->getSize())
    _last_sel_item_pos = 0;

  _menu->setCurrFocusPos(_last_sel_item_pos);

  layout->addWidget(WidgetCreator::getSoftkeyBar(ID_FOOTER, "Chon", "Thoat"));
}

void MenuContext::addMenuItem(ContextID id, const char* title, Image::IconType icon)
{
  MenuItem* item = WidgetCreator::getMenuItem(static_cast<uint16_t>(id));
  item->setVerticalLayout(true);
  item->setCornerRadius(6);
  item->setBackColor(COLOR_MENU_ITEM);
  item->setFocusBackColor(COLOR_FOCUS_BACK);
  item->setFocusBorderColor(COLOR_ACCENT);
  item->setChangingBorder(true);
  item->setChangingBack(true);
  _menu->addItem(item);

  Image* image = new Image(1);
  image->setWidth(ICON_SIZE);
  image->setHeight(ICON_SIZE);
  image->setIcon(icon);
  image->setIconColor(COLOR_ACCENT);
  item->setImg(image);

  Label* label = WidgetCreator::getItemLabel(title, font_5x7);
  label->setTextColor(COLOR_TEXT_PRIMARY);
  label->setFocusBackColor(COLOR_FOCUS_BACK);
  label->setAlign(IWidget::ALIGN_CENTER);
  label->setHPadding(2);
  item->setLbl(label);
}

bool MenuContext::loop()
{
  return true;
}

void MenuContext::update()
{
  if (_input.isHolded(BtnID::BTN_UP))
  {
    _input.lock(BtnID::BTN_UP, HOLD_LOCK);
    up();
  }
  else if (_input.isHolded(BtnID::BTN_DOWN))
  {
    _input.lock(BtnID::BTN_DOWN, HOLD_LOCK);
    down();
  }
  else if (_input.isReleased(BtnID::BTN_LEFT))
  {
    _input.lock(BtnID::BTN_LEFT, CLICK_LOCK);
    pageUp();
  }
  else if (_input.isReleased(BtnID::BTN_RIGHT))
  {
    _input.lock(BtnID::BTN_RIGHT, CLICK_LOCK);
    pageDown();
  }
  else if (_input.isReleased(BtnID::BTN_OK) || _input.isReleased(BtnID::BTN_A))
  {
    _input.lock(BtnID::BTN_OK, CLICK_LOCK);
    _input.lock(BtnID::BTN_A, CLICK_LOCK);
    ok();
  }
  else if (_input.isReleased(BtnID::BTN_START))
  {
    _input.lock(BtnID::BTN_START, CLICK_LOCK);
    openContextByID(ID_CONTEXT_FILES);
  }
  else if (_input.isReleased(BtnID::BTN_OPTION))
  {
    _input.lock(BtnID::BTN_OPTION, CLICK_LOCK);
    openContextByID(ID_CONTEXT_PREF_SEL);
  }
  else if (_input.isReleased(BtnID::BTN_BACK) || _input.isReleased(BtnID::BTN_MENU))
  {
    _input.lock(BtnID::BTN_BACK, CLICK_LOCK);
    _input.lock(BtnID::BTN_MENU, CLICK_LOCK);
    openContextByID(ID_CONTEXT_HOME);
  }
}

void MenuContext::up()
{
  _menu->focusUp();
}

void MenuContext::down()
{
  _menu->focusDown();
}

void MenuContext::pageUp()
{
  _menu->pageUp();
}

void MenuContext::pageDown()
{
  _menu->pageDown();
}

void MenuContext::ok()
{
  const uint16_t id = _menu->getCurrItemID();
  _last_sel_item_pos = _menu->getCurrFocusPos();
  openContextByID(static_cast<ContextID>(id));
}
