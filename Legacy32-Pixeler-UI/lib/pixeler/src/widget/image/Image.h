/**
 * @file Image.h
 * @brief Віджет для відображення статичних зображень
 * @details Відображає на Canvas растрові bmp-зображення,
 * що мають 16 бітну глибину кольору та кодування пікселів в форматі RGB565.
 * Підтримує псевдопрозорість.
 */

#pragma once
#pragma GCC optimize("O3")

#include "../IWidget.h"

namespace pixeler
{
  class Image final : public IWidget
  {
    // TODO додати анімацію
  public:
    explicit Image(uint16_t widget_ID);
    virtual ~Image();

    /**
     * @brief Викликає процедуру малювання віджета на дисплей.
     * Якщо віджет не було змінено, він автоматично пропустить перемальовування.
     *
     */
    virtual void onDraw() override;

    /**
     * @brief Повертає вказівник на глибоку копію віджета.
     *
     * @param id Ідентифікатор, який буде присвоєно новому віджету.
     * @return Image*
     */
    virtual Image* clone(uint16_t id) const override;

    /**
     * @brief Повертає ідентифікатор типу.
     * Використовується в системі приведення типу.
     *
     * @return constexpr TypeID
     */
    static constexpr TypeID getTypeID()
    {
      return TypeID::TYPE_IMAGE;
    }

    /**
     * @brief Встановлює прапор, який змінює поведінку виводу зображення на дисплей.
     *
     * @param state Якщо true зображення буде виводитися з ігноруванням кольору COLOR_TRANSPARENT.
     * Інакше буде виводитись, як є.
     */
    void setTransparency(bool state);

    /**
     * @brief Встановлює вказівник на буффер з даними зображення.
     * Буфер не буде видалено разом із віджетом.
     *
     * @param image_data Вказівник на буфер з даними зображення.
     */
    void setSrc(const uint16_t* image_data);

    /**
     * @brief Перелік вбудованих векторних іконок у стилі Symbian S60.
     * Малюються примітивами графіки замість растрового буфера.
     */
    enum IconType : uint8_t
    {
      ICON_NONE = 0,
      ICON_SD,
      ICON_GAMEPAD,
      ICON_BOOK,
      ICON_WIFI,
      ICON_GEAR,
      ICON_CHIP,
    };

    /**
     * @brief Вмикає режим векторної іконки. Іконка малюється графічними примітивами,
     * масштабуючись під розмір віджета. Має пріоритет над растровим src.
     *
     * @param type Тип іконки. ICON_NONE вимикає режим і повертає до малювання src.
     */
    void setIcon(IconType type);

    /**
     * @brief Встановлює колір ліній векторної іконки.
     *
     * @param color
     */
    void setIconColor(uint16_t color);

  protected:
    /**
     * @brief Копіює поля до іншого віджета.
     *
     * @param widget
     */
    virtual void copyTo(IWidget* widget) const override;

  private:
    /**
     * @brief Малює вибрану векторну іконку в абсолютних координатах.
     *
     * @param x Абсолютна X-координата верхнього лівого кута.
     * @param y Абсолютна Y-координата верхнього лівого кута.
     */
    void drawVectorIcon(int16_t x, int16_t y);

  private:
    const uint16_t* _img_data{nullptr};
    bool _has_transparency{false};
    IconType _icon_type{ICON_NONE};
    uint16_t _icon_color{COLOR_WHITE};
  };

}  // namespace pixeler
