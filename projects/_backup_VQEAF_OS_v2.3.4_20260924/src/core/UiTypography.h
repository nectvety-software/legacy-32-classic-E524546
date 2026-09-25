#pragma once
#include <stdint.h>
// Central UI text roles for the ST7789 native 240x320 portrait panel.
// ASCII uses existing TFT_eSPI rasters; UTF-8 NFC Vietnamese uses generated
// fixed bitmap glyphs (UiVietnameseFont.h) and never allocates on each frame.
namespace UiTypography {
  static constexpr uint8_t MICRO=1;   // status details / hints
  static constexpr uint8_t CAPTION=1; // centered 3x4 menu label
  static constexpr uint8_t BODY=2;    // list items / footer
  static constexpr uint8_t TITLE=2;   // header & card title
  static constexpr int MENU_CAPTION_Y=44;
  static constexpr int HOME_CLOCK_Y=53;
  static constexpr int HOME_DATE_Y=91;
  static constexpr int HOME_HINT_Y=272;
  static constexpr int HEADER_TITLE_X=4;
  static constexpr int HEADER_TITLE_Y=5;
  static constexpr int FOOTER_TEXT_Y=301;
}
