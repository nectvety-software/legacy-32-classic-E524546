#include "SystemService.h"
#include "NotificationService.h"
#include <esp_heap_caps.h>

constexpr int SystemService::MAX_RECENT;

bool SystemService::isCrashReset(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
      return true;
    default:
      return false;
  }
}

void SystemService::begin() {
  prefs.begin("symbian-sys", false);
  boots = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", boots);
  minHeap = ESP.getFreeHeap();

  resetReason = esp_reset_reason();
  const bool previousPending = prefs.getBool("bootPending", false);
  const uint8_t savedCrashes = prefs.getUChar("crashStreak", 0);
  if (previousPending && isCrashReset(resetReason)) {
    crashStreak = (savedCrashes < 250) ? uint8_t(savedCrashes + 1) : savedCrashes;
  } else if (isCrashReset(resetReason)) {
    crashStreak = 1;
  } else {
    crashStreak = 0;
  }
  prefs.putUChar("crashStreak", crashStreak);

  safe = prefs.getBool("forceSafe", false) || crashStreak >= 2;
  healthyMarked = false;
  bootStartedAt = millis();
  prefs.putBool("bootPending", true);
}

void SystemService::markHealthy() {
  if (healthyMarked) return;
  healthyMarked = true;
  prefs.putBool("bootPending", false);
  // A stable boot breaks a crash loop, but an explicitly forced Safe Mode stays
  // enabled until the user leaves it from Recovery.
  crashStreak = 0;
  prefs.putUChar("crashStreak", 0);
}

void SystemService::setSafeMode(bool enabled) {
  safe = enabled;
  prefs.putBool("forceSafe", enabled);
  if (!enabled) {
    crashStreak = 0;
    prefs.putUChar("crashStreak", 0);
  }
}

void SystemService::clearRecoveryState() {
  crashStreak = 0;
  safe = false;
  healthyMarked = true;
  prefs.putUChar("crashStreak", 0);
  prefs.putBool("forceSafe", false);
  prefs.putBool("bootPending", false);
}

const char *SystemService::lastResetReasonText() const {
  switch (resetReason) {
    case ESP_RST_POWERON: return "Power on";
    case ESP_RST_EXT: return "External reset";
    case ESP_RST_SW: return "Software reset";
    case ESP_RST_PANIC: return "CPU panic";
    case ESP_RST_INT_WDT: return "Interrupt watchdog";
    case ESP_RST_TASK_WDT: return "Task watchdog";
    case ESP_RST_WDT: return "Other watchdog";
    case ESP_RST_DEEPSLEEP: return "Deep sleep wake";
    case ESP_RST_BROWNOUT: return "Brownout";
    case ESP_RST_SDIO: return "SDIO reset";
    default: return "Unknown reset";
  }
}

bool SystemService::isTaskScreen(ScreenId s) {
  switch (s) {
    case ScreenId::WiFi:
    case ScreenId::BLE:
    case ScreenId::Music:
    case ScreenId::Files:
    case ScreenId::Collection:
    case ScreenId::Gallery:
    case ScreenId::TextViewer:
    case ScreenId::Browser:
    case ScreenId::Shell:
    case ScreenId::Settings:
    case ScreenId::Themes:
    case ScreenId::Applications:
    case ScreenId::AppInstaller:
    case ScreenId::Notifications:
    case ScreenId::Notes:
    case ScreenId::Recovery:
    case ScreenId::Clock:
    case ScreenId::SystemInfo:
    case ScreenId::About:
    case ScreenId::Calculator:
    case ScreenId::Stopwatch:
    case ScreenId::Snake:
      return true;
    default:
      return false;
  }
}

void SystemService::recordScreen(ScreenId s) {
  if (!isTaskScreen(s)) return;
  int found = -1;
  for (int i = 0; i < recentUsed; ++i) {
    if (recent[i] == s) { found = i; break; }
  }
  if (found == 0) return;
  if (found > 0) {
    for (int i = found; i > 0; --i) recent[i] = recent[i - 1];
    recent[0] = s;
    return;
  }
  const int last = min(recentUsed, MAX_RECENT - 1);
  for (int i = last; i > 0; --i) recent[i] = recent[i - 1];
  recent[0] = s;
  if (recentUsed < MAX_RECENT) ++recentUsed;
}

ScreenId SystemService::recentAt(int index) const {
  if (index < 0 || index >= recentUsed) return ScreenId::Launcher;
  return recent[index];
}

void SystemService::removeRecent(int index) {
  if (index < 0 || index >= recentUsed) return;
  for (int i = index; i < recentUsed - 1; ++i) recent[i] = recent[i + 1];
  if (recentUsed) --recentUsed;
}

void SystemService::clearRecent() { recentUsed = 0; }

void SystemService::update(NotificationService &notifications) {
  const uint32_t now = millis();

  // Once the OS survives the early boot window it is considered healthy. This
  // prevents a power cycle from being mistaken for a crash loop.
  if (!healthyMarked && now - bootStartedAt >= 12000UL) markHealthy();

  if (now - lastHealthCheck < 5000UL) return;
  lastHealthCheck = now;
  const uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < minHeap) minHeap = freeHeap;

  if (freeHeap < 48UL * 1024UL && !lowMemoryLatched) {
    lowMemoryLatched = true;
    notifications.push("Low memory", String(freeHeap / 1024UL) + " KB heap remaining");
  } else if (freeHeap > 64UL * 1024UL) {
    lowMemoryLatched = false;
  }
}

String SystemService::uptimeText() const {
  uint32_t sec = uptimeSeconds();
  uint32_t days = sec / 86400UL; sec %= 86400UL;
  uint32_t hours = sec / 3600UL; sec %= 3600UL;
  uint32_t mins = sec / 60UL;
  if (days) return String(days) + "d " + String(hours) + "h";
  if (hours) return String(hours) + "h " + String(mins) + "m";
  return String(mins) + "m";
}

const char *SystemService::screenName(ScreenId s) {
  switch (s) {
    case ScreenId::WiFi: return "WiFi";
    case ScreenId::BLE: return "Bluetooth";
    case ScreenId::Music: return "Music";
    case ScreenId::Files: return "File manager";
    case ScreenId::Collection: return "Library";
    case ScreenId::Gallery: return "Gallery";
    case ScreenId::TextViewer: return "Text viewer";
    case ScreenId::Browser: return "Qeafbrowser";
    case ScreenId::Shell: return "Shell";
    case ScreenId::Settings: return "Settings";
    case ScreenId::Themes: return "Themes";
    case ScreenId::Applications: return "Applications";
    case ScreenId::AppInstaller: return "App installer";
    case ScreenId::Notifications: return "Notifications";
    case ScreenId::Notes: return "Notes";
    case ScreenId::Recovery: return "Recovery";
    case ScreenId::Clock: return "Clock";
    case ScreenId::SystemInfo: return "System info";
    case ScreenId::About: return "About";
    case ScreenId::Calculator: return "Calculator";
    case ScreenId::Stopwatch: return "Stopwatch";
    case ScreenId::Snake: return "Pixel Snake";
    case ScreenId::LuaApp: return "Lua application";
    default: return "Menu";
  }
}

const char *SystemService::screenIcon(ScreenId s) {
  switch (s) {
    case ScreenId::WiFi: return "Wi";
    case ScreenId::BLE: return "BLE";
    case ScreenId::Music: return "Mus";
    case ScreenId::Files: return "Dir";
    case ScreenId::Collection: return "Col";
    case ScreenId::Gallery: return "Pic";
    case ScreenId::TextViewer: return "Doc";
    case ScreenId::Browser: return "Web";
    case ScreenId::Shell: return "Term";
    case ScreenId::Settings: return "Set";
    case ScreenId::Themes: return "Th";
    case ScreenId::Applications: return "App";
    case ScreenId::AppInstaller: return "App";
    case ScreenId::Notifications: return "Bell";
    case ScreenId::Notes: return "Note";
    case ScreenId::Recovery: return "Rec";
    case ScreenId::Clock: return "Clk";
    case ScreenId::SystemInfo: return "Sys";
    case ScreenId::About: return "i";
    case ScreenId::Calculator: return "Calc";
    case ScreenId::Stopwatch: return "Clk";
    case ScreenId::Snake: return "Game";
    case ScreenId::LuaApp: return "Lua";
    default: return "App";
  }
}
