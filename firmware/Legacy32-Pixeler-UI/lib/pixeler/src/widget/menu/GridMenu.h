/**
 * @file GridMenu.h
 * @brief Віджет меню, що відображає елементи у вигляді сітки іконок (стиль Symbian S60).
 * @details Розташовує елементи зліва направо, зверху вниз у сітці з фіксованою кількістю
 * стовпців. Керує переміщенням фокусу між рядками (вгору/вниз) та в межах рядка (вліво/вправо).
 *
 * Успадкований від IMenu.
 */

#pragma once
#pragma GCC optimize("O3")
#include "IMenu.h"

namespace pixeler
{
  class GridMenu final : public IMenu
  {
  public:
    explicit GridMenu(uint16_t widget_ID);
    virtual ~GridMenu() {}

    /**
     * @brief Повертає вказівник на глибоку копію віджета.
     *
     * @param id Ідентифікатор, який буде присвоєно новому віджету.
     * @return GridMenu*
     */
    virtual GridMenu* clone(uint16_t id) const override;

    /**
     * @brief Повертає ідентифікатор типу.
     *
     * @return constexpr TypeID
     */
    static constexpr TypeID getTypeID()
    {
      return TypeID::TYPE_GRID_MENU;
    }

    /**
     * @brief Встановлює кількість стовпців сітки.
     *
     * @param columns Кількість стовпців (мінімум 1).
     */
    void setColumns(uint16_t columns);

    /**
     * @brief Встановлює фокус на елемент за його порядковим номером.
     *
     * @param focus_pos Порядковий номер елемента в контейнері.
     */
    void setCurrFocusPos(uint16_t focus_pos);

    // Переміщення на рядок вгору (-стовпців).
    virtual bool focusUp() override;
    // Переміщення на рядок вниз (+стовпців).
    virtual bool focusDown() override;
    // Переміщення вліво в межах рядка (-1).
    virtual bool pageUp() override;
    // Переміщення вправо в межах рядка (+1).
    virtual bool pageDown() override;

  protected:
    virtual void copyTo(IWidget* widget) const override;

    virtual void drawItems(uint16_t start, uint16_t count) override;
    virtual uint16_t getCyclesCount() const override;

  protected:
    uint16_t _columns{3};
  };
}  // namespace pixeler
