#pragma once
class TFT_eSPI;
namespace VqeafIconSelfTest {
// Call AFTER the UI has initialized ST7789, only with VQEAF_ICON_SELFTEST=1.
// No SD/network needed. Logs to 115200 Serial and returns number failed cases.
#if defined(VQEAF_ICON_SELFTEST)
unsigned run(TFT_eSPI &display);
#endif
}
