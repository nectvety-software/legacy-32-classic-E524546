// Calls the IDENTICAL test harness and decoder as the diagnostic ESP32 build,
// with a host TFT stub. SPI timings here are not hardware measurements.
#include "Arduino.h"
#include <TFT_eSPI.h>
#include "VqeafIconSelfTest.h"
HostSerial Serial;
int main() {
  TFT_eSPI tft;
  return VqeafIconSelfTest::run(tft) == 0 ? 0 : 1;
}
