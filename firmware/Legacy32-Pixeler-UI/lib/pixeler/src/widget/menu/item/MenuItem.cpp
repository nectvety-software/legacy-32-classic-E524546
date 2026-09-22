#pragma GCC optimize("O3")
#include "MenuItem.h"

namespace pixeler
{
  MenuItem::MenuItem(uint16_t widget_ID, TypeID type_ID) : IWidget(widget_ID, type_ID)
  {
    setLbl(new Label(1));
  }

  MenuItem::~MenuItem()
  {
    delete _label;
    delete _image;
  }

  String MenuItem::getText() const
  {
    return _label->getText();
  }

  Label* MenuItem::getLbl() const
  {
    return _label;
  }

  void MenuItem::setText(const String& text)
  {
    _label->setText(text);
  }

  void MenuItem::setVerticalLayout(bool state)
  {
    _vertical_layout = state;
    _is_changed = true;
  }

  void MenuItem::onDraw()
  {
    if (!_is_changed)
    {
      if (_image)
        _image->onDraw();
      _label->onDraw();
      return;
    }

    _is_changed = false;

    clear();

    if (_vertical_layout)
    {
      uint16_t img_bottom = ITEM_PADDING;
      if (_image)
      {
        _image->setPos((_width - _image->getWidth()) * 0.5, ITEM_PADDING);
        _image->setBackColor(_back_color);
        _image->onDraw();
        img_bottom = ITEM_PADDING + _image->getHeight() + 2;
      }

      _label->setPos(1, img_bottom);
      _label->setWidth(_width - 2);
      _label->setHeight(_height - img_bottom - 1);

      if (_has_focus && _need_change_back)
        _label->setFocus();
      else
        _label->removeFocus();

      _label->onDraw();
      return;
    }

    uint8_t img_width{0};
    if (_image)
    {
      img_width = _image->getWidth() + ITEM_PADDING;
      _image->setPos(ITEM_PADDING, (_height - _image->getHeight()) * 0.5);
      _image->setBackColor(_back_color);
      _image->onDraw();
    }

    _label->setHeight(_height - 2);
    _label->setPos(img_width + ITEM_PADDING, 1);
    _label->setWidth(_width - ITEM_PADDING * 2 - img_width);

    if (_has_focus && _need_change_back)
      _label->setFocus();
    else
      _label->removeFocus();

    _label->onDraw();
  }

  void MenuItem::copyTo(IWidget* widget) const
  {
    IWidget::copyTo(widget);

    MenuItem* clone = static_cast<MenuItem*>(widget);
    clone->_vertical_layout = _vertical_layout;
    clone->setLbl(_label->clone(_label->getID()));
    if (_image)
      clone->setImg(_image->clone(_image->getID()));
  }

  MenuItem* MenuItem::clone(uint16_t id) const
  {
    try
    {
      MenuItem* clone = new MenuItem(id);
      copyTo(clone);
      return clone;
    }
    catch (const std::bad_alloc& e)
    {
      log_e("%s", e.what());
      esp_restart();
    }
  }

  void MenuItem::setImg(Image* image)
  {
    if (image == _image)
      return;

    delete _image;
    _image = image;

    if (_image)
      _image->setParent(this);

    _is_changed = true;
  }

  Image* MenuItem::getImg() const
  {
    return _image;
  }

  void MenuItem::setLbl(Label* label)
  {
    if (!label)
    {
      log_e("Label не може бути null");
      esp_restart();
    }

    if (label == _label)
      return;

    delete _label;
    _label = label;

    _is_changed = true;

    _label->setParent(this);
    _label->setBorder(false);
    _label->setChangingBorder(false);
    _label->setGravity(GRAVITY_CENTER);

    _label->setBackColor(_back_color);
    _label->setFocusBackColor(_focus_back_color);
    _label->setChangingBack(true);
    _label->setTransparency(_is_transparent);
  }
}  // namespace pixeler
