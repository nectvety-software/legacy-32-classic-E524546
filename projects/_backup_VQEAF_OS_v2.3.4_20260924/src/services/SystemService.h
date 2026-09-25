#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include "../core/Types.h"

class NotificationService;

class SystemService {
public:
  static constexpr int MAX_RECENT = 6;

  void begin();
  void update(NotificationService &notifications);
  void recordScreen(ScreenId screen);
  int recentCount() const { return recentUsed; }
  ScreenId recentAt(int index) const;
  void removeRecent(int index);
  void clearRecent();

  uint32_t bootCount() const { return boots; }
  uint32_t minFreeHeap() const { return minHeap; }
  uint32_t uptimeSeconds() const { return millis() / 1000UL; }
  String uptimeText() const;

  // Crash recovery / Safe Mode.
  bool safeMode() const { return safe; }
  bool recoverySuggested() const { return crashStreak > 0 || safe; }
  uint8_t consecutiveCrashes() const { return crashStreak; }
  esp_reset_reason_t lastResetReason() const { return resetReason; }
  const char *lastResetReasonText() const;
  void setSafeMode(bool enabled);
  void clearRecoveryState();
  void markHealthy();
  bool bootHealthy() const { return healthyMarked; }

  static const char *screenName(ScreenId screen);
  static const char *screenIcon(ScreenId screen);
  static bool isTaskScreen(ScreenId screen);

private:
  Preferences prefs;
  ScreenId recent[MAX_RECENT];
  int recentUsed = 0;
  uint32_t boots = 0;
  uint32_t minHeap = 0xFFFFFFFFUL;
  uint32_t lastHealthCheck = 0;
  bool lowMemoryLatched = false;

  esp_reset_reason_t resetReason = ESP_RST_UNKNOWN;
  uint8_t crashStreak = 0;
  bool safe = false;
  bool healthyMarked = false;
  uint32_t bootStartedAt = 0;

  static bool isCrashReset(esp_reset_reason_t reason);
};
