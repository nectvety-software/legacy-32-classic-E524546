#pragma GCC optimize("O3")
#include "Image.h"

namespace pixeler
{
  Image::Image(uint16_t widget_ID) : IWidget(widget_ID, TYPE_IMAGE) {}

  void Image::setTransparency(bool state)
  {
    _has_transparency = state;
    _is_changed = true;
  }

  void Image::copyTo(IWidget* widget) const
  {
    IWidget::copyTo(widget);

    Image* clone = static_cast<Image*>(widget);
    clone->_img_data = _img_data;
    clone->_has_transparency = _has_transparency;
    clone->_icon_type = _icon_type;
    clone->_icon_color = _icon_color;
  }

  Image* Image::clone(uint16_t id) const
  {
    try
    {
      Image* clone = new Image(id);
      copyTo(clone);
      return clone;
    }
    catch (const std::bad_alloc& e)
    {
      log_e("%s", e.what());
      esp_restart();
    }
  }

  void Image::setSrc(const uint16_t* image_data)
  {
    _is_changed = true;
    _img_data = image_data;
  }

  void Image::setIcon(IconType type)
  {
    _icon_type = type;
    _is_changed = true;
  }

  void Image::setIconColor(uint16_t color)
  {
    _icon_color = color;
    _is_changed = true;
  }

  Image::~Image()
  {
  }

  void Image::onDraw()
  {
    if (!_is_changed)
      return;

    _is_changed = false;

    if (_visibility == INVISIBLE)
    {
      hide();
      return;
    }

    uint16_t x_offset{0};
    uint16_t y_offset{0};

    if (_parent)
    {
      x_offset = _parent->getXPos();
      y_offset = _parent->getYPos();
    }

    if (_icon_type != ICON_NONE)
    {
      drawVectorIcon(_x_pos + x_offset, _y_pos + y_offset);
      return;
    }

    if (_img_data)
    {
      if (!_has_transparency)
      {
#if CONFIG_IDF_TARGET_ESP32P4
        if (_width * _height > PPA_IMG_SIZE_TRIGG)
        {
          bool old_state = _display.isPPAEnabled();

          _display.setPPAState(true);
          _display.drawBitmap(_x_pos + x_offset, _y_pos + y_offset, _img_data, _width, _height);
          _display.setPPAState(old_state);
        }
        else
#endif  // #if CONFIG_IDF_TARGET_ESP32P4
        {
          _display.drawBitmap(_x_pos + x_offset, _y_pos + y_offset, _img_data, _width, _height);
        }
      }
      else
      {
        _display.drawBitmapTransp(_x_pos + x_offset, _y_pos + y_offset, _img_data, _width, _height);
      }
    }
    else
    {
      log_e("Не встановлено src");
      esp_restart();
    }
  }

  void Image::drawVectorIcon(int16_t x, int16_t y)
  {
    _display.fillRect(x, y, _width, _height, _back_color);

    const int16_t s = _width < _height ? _width : _height;
    const int16_t cx = x + _width / 2;
    const int16_t cy = y + _height / 2;
    const uint16_t c = _icon_color;

    switch (_icon_type)
    {
      case ICON_SD:
      {
        const int16_t cardW = s * 0.46;
        const int16_t cardH = s * 0.64;
        const int16_t x0 = cx - cardW / 2;
        const int16_t y0 = cy - cardH / 2;
        const int16_t cut = cardW * 0.4;
        for (int16_t o = 0; o < 2; ++o)  // жирна лінія у два проходи
        {
          _display.drawLine(x0 + o, y0, x0 + cardW - cut, y0, c);
          _display.drawLine(x0 + cardW - cut, y0, x0 + cardW, y0 + cut, c);
          _display.drawLine(x0 + cardW + o, y0 + cut, x0 + cardW + o, y0 + cardH, c);
          _display.drawLine(x0, y0 + cardH + o, x0 + cardW, y0 + cardH + o, c);
          _display.drawLine(x0 + o, y0, x0 + o, y0 + cardH, c);
        }
        for (int16_t k = 0; k < 3; ++k)  // контакти зверху
        {
          const int16_t px = x0 + cardW * 0.28 + k * (cardW * 0.22);
          _display.drawLine(px, y0 + 3, px, y0 + cut, c);
        }
        break;
      }

      case ICON_GAMEPAD:
      {
        const int16_t gw = s * 0.74;
        const int16_t gh = s * 0.44;
        const int16_t gx = cx - gw / 2;
        const int16_t gy = cy - gh / 2;
        _display.drawRoundRect(gx, gy, gw, gh, gh / 3, c);
        _display.drawRoundRect(gx + 1, gy + 1, gw - 2, gh - 2, gh / 3, c);
        const int16_t dx = cx - gw / 4;
        const int16_t a = gh * 0.30;
        _display.drawLine(dx - a, cy, dx + a, cy, c);
        _display.drawLine(dx, cy - a, dx, cy + a, c);
        const int16_t bx = cx + gw / 4;
        _display.fillCircle(bx - 3, cy + 3, 2, c);
        _display.fillCircle(bx + 4, cy - 3, 2, c);
        break;
      }

      case ICON_BOOK:
      {
        const int16_t bw = s * 0.62;
        const int16_t bh = s * 0.48;
        const int16_t x0 = cx - bw / 2;
        const int16_t y0 = cy - bh / 2;
        _display.drawRect(x0, y0, bw, bh, c);
        _display.drawLine(cx, y0, cx, y0 + bh, c);
        _display.drawLine(cx + 1, y0, cx + 1, y0 + bh, c);
        for (int16_t i = 1; i <= 3; ++i)  // рядки на сторінках
        {
          const int16_t ly = y0 + i * bh / 4;
          _display.drawLine(x0 + 3, ly, cx - 3, ly, c);
          _display.drawLine(cx + 4, ly, x0 + bw - 3, ly, c);
        }
        break;
      }

      case ICON_WIFI:
      {
        const int16_t ox = cx;
        const int16_t oy = y + _height * 0.72;
        _display.fillCircle(ox, oy, s * 0.06 + 1, c);
        for (int16_t i = 1; i <= 3; ++i)
        {
          const int16_t r = i * s * 0.18;
          _display.drawCircle(ox, oy, r, c);
          _display.drawCircle(ox, oy, r - 1, c);
        }
        _display.fillRect(x, oy + 1, _width, y + _height - (oy + 1), _back_color);
        break;
      }

      case ICON_GEAR:
      {
        const int16_t r = s * 0.28;
        const int16_t t = s * 0.12;
        _display.drawCircle(cx, cy, r, c);
        _display.drawCircle(cx, cy, r - 1, c);
        _display.drawCircle(cx, cy, r * 0.42, c);
        _display.fillRect(cx - 1, cy - r - t, 3, t, c);   // зубці N/S/E/W
        _display.fillRect(cx - 1, cy + r, 3, t, c);
        _display.fillRect(cx - r - t, cy - 1, t, 3, c);
        _display.fillRect(cx + r, cy - 1, t, 3, c);
        const int16_t d = r * 0.72;
        _display.drawLine(cx - d, cy - d, cx - d - t, cy - d - t, c);  // діагональні зубці
        _display.drawLine(cx + d, cy - d, cx + d + t, cy - d - t, c);
        _display.drawLine(cx - d, cy + d, cx - d - t, cy + d + t, c);
        _display.drawLine(cx + d, cy + d, cx + d + t, cy + d + t, c);
        break;
      }

      case ICON_CHIP:
      {
        const int16_t cs = s * 0.44;
        const int16_t x0 = cx - cs / 2;
        const int16_t y0 = cy - cs / 2;
        _display.drawRect(x0, y0, cs, cs, c);
        _display.drawRect(x0 + 1, y0 + 1, cs - 2, cs - 2, c);
        _display.drawRect(cx - cs / 6, cy - cs / 6, cs / 3, cs / 3, c);
        const int16_t p = cs * 0.28;
        for (int16_t k = 0; k < 3; ++k)  // ніжки чіпа
        {
          const int16_t off = -cs / 3 + k * (cs / 3);
          _display.drawLine(cx + off, y0 - p, cx + off, y0, c);
          _display.drawLine(cx + off, y0 + cs, cx + off, y0 + cs + p, c);
          _display.drawLine(x0 - p, cy + off, x0, cy + off, c);
          _display.drawLine(x0 + cs, cy + off, x0 + cs + p, cy + off, c);
        }
        break;
      }

      default:
        break;
    }
  }
}  // namespace pixeler
