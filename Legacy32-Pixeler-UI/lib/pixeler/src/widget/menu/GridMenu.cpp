#pragma GCC optimize("O3")
#include "GridMenu.h"

namespace pixeler
{
  GridMenu::GridMenu(uint16_t widget_ID) : IMenu(widget_ID, TYPE_GRID_MENU) {}

  void GridMenu::setColumns(uint16_t columns)
  {
    _columns = columns > 0 ? columns : 1;
    _is_changed = true;
  }

  uint16_t GridMenu::getCyclesCount() const
  {
    if (_widgets.empty())
      return 0;

    uint16_t row_step = _item_height + _items_spacing;
    uint16_t rows = row_step > 0 ? _height / row_step : 1;
    if (rows == 0)
      rows = 1;

    uint16_t capacity = rows * _columns;
    uint16_t remaining = _widgets.size() - _first_item_index;

    return capacity < remaining ? capacity : remaining;
  }

  void GridMenu::drawItems(uint16_t start, uint16_t count)
  {
    clear();

    if (_widgets.empty())
      return;

    uint16_t spacing = _items_spacing;
    uint16_t cell_w = (_width - (_columns + 1) * spacing) / _columns;
    size_t container_size = _widgets.size();

    for (uint16_t i{start}; i < start + count && i < container_size; ++i)
    {
      uint16_t rel = i - start;
      uint16_t col = rel % _columns;
      uint16_t row = rel / _columns;

      uint16_t x = spacing + col * (cell_w + spacing);
      uint16_t y = spacing + row * (_item_height + spacing);

      _widgets[i]->setPos(x, y);
      _widgets[i]->setWidth(cell_w);
      _widgets[i]->setHeight(_item_height);
      _widgets[i]->onDraw();
    }
  }

  bool GridMenu::focusUp()
  {
    if (_has_touch_support || _widgets.empty() || _cur_focus_pos < _columns)
      return false;

    _widgets[_cur_focus_pos]->removeFocus();
    _cur_focus_pos -= _columns;
    _widgets[_cur_focus_pos]->setFocus();
    return true;
  }

  bool GridMenu::focusDown()
  {
    if (_has_touch_support || _widgets.empty())
      return false;

    uint16_t target = _cur_focus_pos + _columns;
    if (target >= _widgets.size())
      return false;

    _widgets[_cur_focus_pos]->removeFocus();
    _cur_focus_pos = target;
    _widgets[_cur_focus_pos]->setFocus();
    return true;
  }

  bool GridMenu::pageUp()
  {
    if (_has_touch_support || _widgets.empty() || _cur_focus_pos == 0)
      return false;

    _widgets[_cur_focus_pos]->removeFocus();
    --_cur_focus_pos;
    _widgets[_cur_focus_pos]->setFocus();
    return true;
  }

  bool GridMenu::pageDown()
  {
    if (_has_touch_support || _widgets.empty() || _cur_focus_pos >= _widgets.size() - 1)
      return false;

    _widgets[_cur_focus_pos]->removeFocus();
    ++_cur_focus_pos;
    _widgets[_cur_focus_pos]->setFocus();
    return true;
  }

  void GridMenu::setCurrFocusPos(uint16_t focus_pos)
  {
    if (_has_touch_support || _widgets.size() < 2 || _cur_focus_pos == focus_pos || focus_pos >= _widgets.size())
      return;

    _widgets[_cur_focus_pos]->removeFocus();
    _cur_focus_pos = focus_pos;
    _widgets[_cur_focus_pos]->setFocus();
    _is_changed = true;
  }

  void GridMenu::copyTo(IWidget* widget) const
  {
    IMenu::copyTo(widget);

    GridMenu* clone = static_cast<GridMenu*>(widget);
    clone->_columns = _columns;
  }

  GridMenu* GridMenu::clone(uint16_t id) const
  {
    try
    {
      GridMenu* clone = new GridMenu(id);
      copyTo(clone);
      return clone;
    }
    catch (const std::bad_alloc& e)
    {
      log_e("%s", e.what());
      esp_restart();
    }
  }
}  // namespace pixeler
