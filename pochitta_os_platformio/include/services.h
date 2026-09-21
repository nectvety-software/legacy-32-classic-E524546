#pragma once
#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include "app_types.h"

class Services {
 public:
  bool begin();
  std::vector<MenuItem> wifiScan();
  std::vector<MenuItem> bleScan();
  std::vector<MenuItem> listFiles(const String& path = "/");
  std::vector<MenuItem> listSDCard(const String& path = "/");
  bool saveNote(const String& text);
  std::vector<MenuItem> listNotes();
  String httpGet(const String& url);
  String terminalCommand(const String& cmd);
  void setBrightness(uint8_t value);
  uint8_t brightness() const { return brightness_; }
  bool sdReady() const { return sdReady_; }
  uint64_t sdTotalBytes() const;
  uint64_t sdUsedBytes() const;

 private:
  Preferences prefs_;
  uint8_t brightness_ = 255;
  bool sdReady_ = false;
};
