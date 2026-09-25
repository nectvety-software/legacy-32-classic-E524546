#pragma once
// VQEAF OS target build contract. These assertions do not change pin wiring.
#include "BoardConfig.h"

#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "VQEAF OS target must be an ESP32-S3. Select env:vqeaf_os."
#endif
#if defined(ARDUINO_ARCH_ESP32) && !defined(BOARD_HAS_PSRAM)
#error "Enable BOARD_HAS_PSRAM for ESP32-S3 N16R8."
#endif

namespace VqeafBuildSanity {
  template<int Pin, int... Rest> struct NotIn;
  template<int Pin> struct NotIn<Pin> { static constexpr bool value = true; };
  template<int Pin, int Other, int... Rest> struct NotIn<Pin, Other, Rest...> {
    static constexpr bool value = (Pin != Other) && NotIn<Pin, Rest...>::value;
  };
  template<int... Pins> struct AllUnique;
  template<> struct AllUnique<> { static constexpr bool value = true; };
  template<int Pin, int... Rest> struct AllUnique<Pin, Rest...> {
    static constexpr bool value = NotIn<Pin, Rest...>::value && AllUnique<Rest...>::value;
  };
  static_assert(Board::SCREEN_W == 240 && Board::SCREEN_H == 320 &&
                Board::TFT_ROTATION == 0, "VQEAF OS targets portrait 240x320");
  static_assert(AllUnique<
     Board::TFT_LEDK_PIN, Board::TFT_DC_PIN, Board::TFT_CS_PIN,
     Board::TFT_SCL_PIN, Board::TFT_SDA_PIN, Board::TFT_RST_PIN,
     Board::KEY_MENU, Board::KEY_UP, Board::KEY_A,
     Board::KEY_LEFT, Board::KEY_START, Board::KEY_RIGHT,
     Board::KEY_OPTION, Board::KEY_DOWN, Board::KEY_B,
     Board::KEY_SELECT, Board::SD_D3, Board::SD_CMD,
     Board::SD_CLK, Board::SD_D0>::value,
     "Duplicate GPIO in documented LCD, keypad, SD map");
}
