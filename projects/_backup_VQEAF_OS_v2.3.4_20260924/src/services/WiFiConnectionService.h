#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "WiFiProfileStore.h"

// Qeafbrowser-inspired WiFi workflow shared with the S60 shell/browser radio.
// All scans and association checks are polled: never block the OS UI loop.
// No password is logged; NVS profiles are updated only on successful join.
class WiFiConnectionService {
public:
  static constexpr int MAX_NETWORKS = 16;
  enum class Phase : uint8_t { Idle, Scanning, Connecting, Connected, Failed };
  // Boot WiFi selector is a non-blocking state machine. Unlike a single
  // WiFi.begin(lastSSID), it ranks every saved SSID visible in the full scan.
  enum class AutoPhase : uint8_t { Disabled, Scheduled, Scanning, Connecting, Online, Waiting, Paused };
  struct Network {
    char ssid[33];  // IEEE 802.11: at most 32 bytes, plus NUL.
    int16_t rssi;
    uint8_t auth;
    Network() : ssid{0}, rssi(-127), auth(0) {}
  };

  void begin(WiFiProfileStore &profiles);
  void configureAuto(bool enabled);
  void resumeAutoAfterBrowsing(); // opening/closing WiFi list is not a disconnect
  void externalOverride();          // shell commands also own the shared radio   // safe mode and Settings must gate this
  AutoPhase autoPhase() const { return autoState; }
  uint32_t retrySeconds() const;
  int autoCandidateCount() const { return candidateCount; }
  const char *autoStateLabel() const;
  bool scan();             // asynchronous, retains an existing association
  void cancelScan();
  bool connect(const String &ssid, const String &password, bool open, bool autoReconnect = true);
  void disconnect();       // explicitly disables auto-reconnect until next connect
  void poll();             // call on every iteration of Arduino loop()
  bool takeChanged();
  bool takeConnectionResult(bool &successful);
  Phase phase() const { return state; }
  const char *error() const { return failure; }
  const String &requestedSSID() const { return targetSSID; }
  int count() const { return used; }
  const Network &network(int index) const;
  uint32_t elapsedMs() const { return millis() - started; }
private:
  WiFiProfileStore *profiles = nullptr;
  Network networks[MAX_NETWORKS];
  Phase state = Phase::Idle;
  uint32_t started = 0;
  int used = 0;
  bool changed = false;
  bool resultPending = false;
  bool resultSuccess = false;
  bool joiningOpen = false;
  String targetSSID;
  String pendingPassword;
  char failure[64] = {0};

  void finish(bool successful, const char *reason = nullptr);
  void consumeScan(int total);
  void suspendAuto(bool intentional);  // intentional connect/off differs from viewing
  void startAutoScan();
  void joinNextCandidate();
  void autoRetry(const char *reason);
  bool autoEnabled = false;
  bool autoScanOwner = false;
  bool pausedByBrowse = false;
  bool autoJoinOwner = false;
  AutoPhase autoState = AutoPhase::Disabled;
  uint32_t autoDue = 0;
  uint32_t disconnectedAt = 0;
  uint32_t autoBackoffMs = 30000UL;
  uint8_t candidateCount = 0;
  uint8_t candidateNext = 0;
  int candidateProfile[WiFiProfileStore::MAX_PROFILES];
  int16_t candidateRssi[WiFiProfileStore::MAX_PROFILES];
};
