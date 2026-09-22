/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║          LEGACY-32 CLASSIC E524546 - LegacyOS v1.0          ║
 * ║       ESP32-S3-WROOM-1 (N16R8) | ST7789 240x320 TFT         ║
 * ║              Android-Style Embedded OS                       ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 * Hardware: ESP32-S3-WROOM-1 N16R8 (16MB Flash, 8MB PSRAM)
 * Display:  ST7789 2" TFT 240x320
 * Input:    10 Buttons (D-Pad + Menu + Option + Select + Start + A + B)
 * Storage:  MicroSD Card (SPI)
 * Features: WiFi, BLE, LoRa-ready, Lua/JS scripting, Retro Emulation
 *
 * Board: ESP32S3 Dev Module
 * Flash: 16MB QIO, PSRAM: OPI PSRAM
 * Partition: Huge App (3MB No OTA / 1MB SPIFFS)
 */

#include "include/LegacyOS.h"

LegacyOS* os = nullptr;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println(F("═══════════════════════════════════════"));
  Serial.println(F("   LEGACY-32 CLASSIC E524546 BOOT"));
  Serial.println(F("   LegacyOS v1.0 by Android-Style OS"));
  Serial.println(F("═══════════════════════════════════════"));
  
  // Khởi tạo hệ điều hành
  os = new LegacyOS();
  os->init();
}

void loop() {
  if (os) {
    os->run();
  }
}
