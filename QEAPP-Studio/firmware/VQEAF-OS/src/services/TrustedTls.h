#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <time.h>

// Firmware-controlled trust store. No automatic trust of arbitrary files on SD.
// CA signatures and TLS hostname verification are performed by mbedTLS via
// WiFiClientSecure::setCACert(). Never substitute setInsecure().
class TrustedTls {
public:
  static bool timeValidAt(time_t timestamp);
  static bool configure(WiFiClientSecure &client, String &error);
  static const char *caPem();
  static size_t caPemLength();
};
