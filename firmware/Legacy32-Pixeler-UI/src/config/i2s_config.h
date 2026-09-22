#pragma once
#include <stdint.h>

// Legacy-32-Classic has no configured I2S codec or microphone.
// Keep every I2S signal disconnected so the audio subsystem can never claim
// the TFT, button or SD GPIOs. The application MP3 context has been removed from this custom build.
#define PIN_I2S_OUT_BCLK -1
#define PIN_I2S_OUT_LRC  -1
#define PIN_I2S_OUT_DOUT -1
#define PIN_I2S_IN_SCK   -1
#define PIN_I2S_IN_WS    -1
#define PIN_I2S_IN_SD    -1
