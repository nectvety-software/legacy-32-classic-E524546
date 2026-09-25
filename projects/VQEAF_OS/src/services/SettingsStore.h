#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "../core/Theme.h"

struct SystemSettings {
  ThemeId theme = ThemeId::S60Green;
  uint8_t brightness = 90;
  uint8_t volume = 80;
  bool hour12 = false;
  bool wifiAuto = true;
  uint16_t lockTimeoutSec = 60; // 0 disables automatic locking.
};

class SettingsStore {
public:
  void begin();
  void save();
  SystemSettings &data() { return cfg; }
  const SystemSettings &data() const { return cfg; }

  String selectedThemePath() const { return customThemePath; }
  void selectThemeFile(const String &path);
  void selectBuiltInTheme(ThemeId id);
  String note() const { return noteText; }
  void saveNote(const String &text);

private:
  Preferences prefs;
  SystemSettings cfg;
  String noteText;
  String customThemePath;
};
