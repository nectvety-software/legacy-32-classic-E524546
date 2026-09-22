#include <Arduino.h>

#include "Pixeler.h"
#include "input_config.h"

using namespace pixeler;

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println("Legacy-32-Classic / Pixeler");
  Serial.printf("Flash: %u MB, PSRAM: %u MB\n",
                ESP.getFlashChipSize() / (1024U * 1024U),
                ESP.getPsramSize() / (1024U * 1024U));

  // Start Pixeler's context/UI task. No external coprocessor is used.
  Pixeler::begin(80);
}

void loop()
{
  // Pixeler runs in its own FreeRTOS task.
  vTaskDelete(nullptr);
}

