#include "SettingsStore.h"

void SettingsStore::begin() {
  prefs.begin("symbian-s3", false);
  // Keep the original NVS namespace when upgrading older firmware.
  // First-install follows the user's screenshot-style Lime reference.
  // Existing v2.x settings remain untouched (including user-selected Night).
  uint8_t themeRev = prefs.getUChar("themeRev", 0);
  uint8_t rawTheme = prefs.getUChar("theme", static_cast<uint8_t>(ThemeId::S60Green));
  if (themeRev < 1) {
    // Older firmware could save theme without a revision. Keep that choice;
    // only a truly fresh installation starts in the screenshot Lime theme.
    cfg.theme = prefs.isKey("theme") && rawTheme <= static_cast<uint8_t>(ThemeId::External)
                  ? static_cast<ThemeId>(rawTheme) : ThemeId::S60Green;
    prefs.putUChar("theme", static_cast<uint8_t>(cfg.theme));
    prefs.putUChar("themeRev", 3);
  } else {
    if (rawTheme > static_cast<uint8_t>(ThemeId::External)) rawTheme = static_cast<uint8_t>(ThemeId::S60Green);
    cfg.theme = static_cast<ThemeId>(rawTheme);
    // Preserve previously stored user choice: the v2.3 pixel pass must NEVER
    // silently migrate an existing Lime or imported .vqeaf theme to Night.
    if (themeRev < 3) prefs.putUChar("themeRev", 3);
  }
  cfg.brightness = prefs.getUChar("bright", 90);
  cfg.volume = prefs.getUChar("volume", 80);
  cfg.hour12 = prefs.getBool("hour12", false);
  cfg.wifiAuto = prefs.getBool("wifiAuto", true);
  cfg.lockTimeoutSec = prefs.getUShort("lockSec", 60);
  if (!(cfg.lockTimeoutSec == 0 || cfg.lockTimeoutSec == 30 || cfg.lockTimeoutSec == 60 ||
        cfg.lockTimeoutSec == 120 || cfg.lockTimeoutSec == 300)) {
    cfg.lockTimeoutSec = 60;
  }
  customThemePath = prefs.getString("themeFile", "");
  if (cfg.theme == ThemeId::External && (customThemePath.length() == 0 || customThemePath.length() > 119))
    cfg.theme = ThemeId::Classic;
  noteText = prefs.getString("note", "");
}

void SettingsStore::save() {
  prefs.putUChar("theme", static_cast<uint8_t>(cfg.theme));
  prefs.putUChar("bright", cfg.brightness);
  prefs.putUChar("volume", cfg.volume);
  prefs.putBool("hour12", cfg.hour12);
  prefs.putBool("wifiAuto", cfg.wifiAuto);
  prefs.putUShort("lockSec", cfg.lockTimeoutSec);
}

void SettingsStore::selectThemeFile(const String &path) {
  if (path.length() < 1 || path.length() > 119) return;
  customThemePath = path;
  cfg.theme = ThemeId::External;
  prefs.putString("themeFile", customThemePath);
  save();
}

void SettingsStore::selectBuiltInTheme(ThemeId id) {
  if (id == ThemeId::External) return;
  cfg.theme = id;
  customThemePath = "";
  prefs.remove("themeFile");
  save();
}

void SettingsStore::saveNote(const String &text) {
  noteText = text;
  prefs.putString("note", noteText);
}
