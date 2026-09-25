#include "WiFiConnectionService.h"
#include <cstring>
#if defined(ESP_PLATFORM) && __has_include(<esp_wifi.h>)
  #include <esp_wifi.h>
#endif

constexpr int WiFiConnectionService::MAX_NETWORKS;

void WiFiConnectionService::begin(WiFiProfileStore &store) {
  profiles = &store;
  used = 0;
  state = Phase::Idle;
  changed = false;
  resultPending = false;
  failure[0] = 0;
  autoEnabled = false;
  autoState = AutoPhase::Disabled;
  autoScanOwner = autoJoinOwner = false;
  pausedByBrowse = false;
  candidateCount = candidateNext = 0;
  disconnectedAt = 0;
  autoBackoffMs = 30000UL;
}

void WiFiConnectionService::configureAuto(bool enabled) {
  if (!enabled) {
    autoEnabled = false;
    if (autoScanOwner && state == Phase::Scanning) cancelScan();
    if (autoJoinOwner && state == Phase::Connecting && WiFi.status() != WL_CONNECTED) {
      // Abort only our own unfinished association, never an existing session.
      WiFi.disconnect();
      state = Phase::Idle;
      pendingPassword = "";
    }
    autoScanOwner = autoJoinOwner = false;
    pausedByBrowse = false;
    autoState = AutoPhase::Disabled;
    changed = true;
    return;
  }
  if (!profiles || profiles->count() == 0) {
    autoEnabled = false;
    autoState = AutoPhase::Disabled;
    return;
  }
  // Repeated configuration from Settings should not restart an active scan.
  if (autoEnabled && autoState != AutoPhase::Paused) return;
  autoEnabled = true;
  pausedByBrowse = false;
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  if (WiFi.status() == WL_CONNECTED) {
    autoState = AutoPhase::Online;
    state = Phase::Connected;
  } else {
    // The manager (not the SDK's last AP) owns ranking and automatic retry.
    WiFi.setAutoReconnect(false);
    autoState = AutoPhase::Scheduled;
    autoDue = millis();
    disconnectedAt = 0;
  }
  changed = true;
}

void WiFiConnectionService::suspendAuto(bool intentional) {
  if (intentional) pausedByBrowse = false;
  else if (autoEnabled && autoState != AutoPhase::Paused) pausedByBrowse = true;
  if (!autoEnabled || autoState == AutoPhase::Paused) return;
  autoState = AutoPhase::Paused;
  autoScanOwner = false;
  autoJoinOwner = false;
  candidateCount = candidateNext = 0;
  changed = true;
}

void WiFiConnectionService::resumeAutoAfterBrowsing() {
  if (!autoEnabled || !pausedByBrowse) return;
  pausedByBrowse = false;
  if (state == Phase::Scanning && !autoScanOwner) cancelScan();
  // Do not tear down a successful connection just to re-rank APs.
  if (WiFi.status() == WL_CONNECTED) {
    autoState = AutoPhase::Online;
    state = Phase::Connected;
  } else {
    autoState = AutoPhase::Scheduled;
    autoDue = millis();
    state = Phase::Idle;
  }
  changed = true;
}

void WiFiConnectionService::externalOverride() {
  // Existing Shell WiFi commands directly operate on the shared Arduino
  // station. Avoid a background boot scan/join racing with their explicit
  // scan, station-off or manual connection, without dropping live sessions.
  suspendAuto(true);
  if (state == Phase::Scanning) cancelScan();
  else if (state == Phase::Connecting && WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    state = Phase::Idle;
    pendingPassword = "";
    changed = true;
  }
}

uint32_t WiFiConnectionService::retrySeconds() const {
  if (autoState != AutoPhase::Waiting) return 0;
  const int32_t remain = (int32_t)(autoDue - millis());
  return remain > 0 ? ((uint32_t)remain + 999UL) / 1000UL : 0;
}

const char *WiFiConnectionService::autoStateLabel() const {
  switch (autoState) {
    case AutoPhase::Scheduled: return "Searching saved networks";
    case AutoPhase::Scanning: return "Scanning saved networks";
    case AutoPhase::Connecting: return "Connecting saved WiFi";
    case AutoPhase::Online: return "Connected";
    case AutoPhase::Waiting: return "Retry pending";
    case AutoPhase::Paused: return "Manual WiFi mode";
    default: return "Auto WiFi off";
  }
}

const WiFiConnectionService::Network &WiFiConnectionService::network(int index) const {
  static const Network empty;
  return index >= 0 && index < used ? networks[index] : empty;
}

bool WiFiConnectionService::scan() {
  if (state == Phase::Connecting) return false;
  // Opening WiFi Manager during an automatic scan hands the scan results to
  // the UI and prevents background selection from fighting manual input.
  if (state == Phase::Scanning) {
    if (autoScanOwner) suspendAuto(false);
    return true;
  }
  // Viewing available networks while already online must not disable
  // subsequent signal-aware recovery when the user exits Settings.
  if (!(autoState == AutoPhase::Online && WiFi.status() == WL_CONNECTED))
    suspendAuto(false);
  WiFi.mode(WIFI_STA);
  // IMPORTANT: Qeafbrowser's rule — do not disconnect an existing association
  // merely to refresh the visible network list. No credentials are touched.
  WiFi.scanDelete();
  used = 0;
  resultPending = false; // a new scan cannot consume a stale connection event
  const int result = WiFi.scanNetworks(true, true);
  started = millis();
  failure[0] = 0;
  targetSSID = ""; // a scan failure is not a failed connection to the previous AP
  state = Phase::Scanning;
  changed = true;
  if (result >= 0) { // support implementations which return cached results.
    consumeScan(result);
  } else if (result != -1) {
    finish(false, "WiFi scan could not start");
  }
  return state != Phase::Failed;
}

void WiFiConnectionService::consumeScan(int total) {
  used = 0;
  // Rank candidates BEFORE scanDelete(), considering the full scan (not only
  // the strongest 16 APs retained for the handset UI).  Skip mismatched
  // open/secured variants sharing an SSID, preserving saved credential safety.
  if (autoScanOwner && autoEnabled && profiles) {
    int16_t strength[WiFiProfileStore::MAX_PROFILES];
    for (int p = 0; p < profiles->count(); ++p) strength[p] = -32768;
    for (int i = 0; i < total; ++i) {
      const String ssid = WiFi.SSID(i);
      const int p = profiles->find(ssid);
      if (p < 0 || ssid.length() == 0) continue;
      const bool open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
      if (profiles->at(p).open != open) continue;
      const int16_t rssi = (int16_t)WiFi.RSSI(i);
      if (rssi > strength[p]) strength[p] = rssi;
    }
    candidateCount = candidateNext = 0;
    for (int p = 0; p < profiles->count(); ++p) {
      if (strength[p] == -32768) continue; // do not blind-join absent APs
      uint8_t pos = 0;
      while (pos < candidateCount &&
             (candidateRssi[pos] > strength[p] ||
              (candidateRssi[pos] == strength[p] &&
               // Last-successful SSID wins equal RSSI; otherwise stable order.
               (profiles->at(candidateProfile[pos]).ssid == profiles->lastSSID() ||
                profiles->at(p).ssid != profiles->lastSSID())))) ++pos;
      for (uint8_t j = candidateCount; j > pos; --j) {
        candidateProfile[j] = candidateProfile[j-1];
        candidateRssi[j] = candidateRssi[j-1];
      }
      candidateProfile[pos] = p;
      candidateRssi[pos] = strength[p];
      ++candidateCount;
    }
  }
  // Keep strongest 16 from the ENTIRE scan, not only the first 16 returned.
  // Insertion sort's bounded fixed array needs no heap, PSRAM or String list.
  for (int i = 0; i < total; ++i) {
    String name = WiFi.SSID(i);
    if (name.length() > 32) continue;
    const int rssi = WiFi.RSSI(i);
    int pos = 0;
    while (pos < used && networks[pos].rssi >= rssi) ++pos;
    if (pos >= MAX_NETWORKS) continue;
    int last = used < MAX_NETWORKS ? used++ : MAX_NETWORKS-1;
    for (int j = last; j > pos; --j) networks[j] = networks[j-1];
    snprintf(networks[pos].ssid, sizeof(networks[pos].ssid), "%s", name.c_str());
    networks[pos].rssi = (int16_t)rssi;
    networks[pos].auth = (uint8_t)WiFi.encryptionType(i);
  }
  WiFi.scanDelete();
  state = Phase::Idle;
  changed = true;
  if (autoScanOwner) {
    autoScanOwner = false;
    if (candidateCount) joinNextCandidate();
    else autoRetry("No saved WiFi in range");
  }
}

void WiFiConnectionService::cancelScan() {
  if (state != Phase::Scanning) return;
  autoScanOwner = false;
  // scanDelete clears driver-owned results. A running async scan might complete
  // afterwards; restarting the UI triggers a fresh scan.
  #if defined(ESP_PLATFORM) && __has_include(<esp_wifi.h>)
    esp_wifi_scan_stop();
  #endif
  WiFi.scanDelete();
  state = Phase::Idle;
  changed = true;
}

bool WiFiConnectionService::connect(const String &ssid, const String &password, bool open, bool autoReconnect) {
  suspendAuto(true); // manual selection always wins, including on same SSID
  if (!profiles || ssid.length() == 0 || ssid.length() > 32 || password.length() > 64 ||
      (!open && password.length() > 0 && password.length() < 8)) {
    snprintf(failure, sizeof(failure), "Invalid SSID or password");
    state = Phase::Failed;
    changed = true;
    return false;
  }
  if (state == Phase::Scanning) cancelScan();
  // Reconnecting to the same network must not disconnect a working session.
  targetSSID = ssid;
  pendingPassword = open ? String() : password;
  joiningOpen = open;
  failure[0] = 0;
  started = millis();
  state = Phase::Connecting;
  changed = true;
  resultPending = false;
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) {
    const int position = profiles->find(ssid);
    if (position >= 0 && profiles->at(position).open == open &&
        profiles->at(position).password == pendingPassword) {
      // Exact known saved credentials: do not interrupt a live browser session.
      finish(true);
      return true;
    }
    // IMPORTANT: never trust or persist an unverified password merely because
    // the device is ALREADY associated to the SSID with different credentials.
    // Force a real new association to check user-entered replacements.
    WiFi.disconnect();
  }
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false); // OS NVS controls persistence; SDK must not save failed joins
  WiFi.setAutoReconnect(autoReconnect);
  // Keep browser sessions alive while scanning, but intentionally change
  // association when the user explicitly picks a different AP.
  WiFi.begin(targetSSID.c_str(), joiningOpen ? nullptr : pendingPassword.c_str());
  return true;
}

void WiFiConnectionService::finish(bool successful, const char *reason) {
  state = successful ? Phase::Connected : Phase::Failed;
  if (successful && profiles && targetSSID.length()) {
    profiles->saveProfile(targetSSID, pendingPassword, joiningOpen);
  }
  if (!successful) snprintf(failure, sizeof(failure), "%s", reason ? reason : "Connection failed");
  pendingPassword = ""; // never retain password in RAM after an attempt
  resultPending = true;
  resultSuccess = successful;
  changed = true;
}

void WiFiConnectionService::disconnect() {
  suspendAuto(true); // an intentional Disconnect must not trigger an auto retry
  if (state == Phase::Scanning) cancelScan();
  if (state == Phase::Connecting) {
    pendingPassword = "";
    targetSSID = "";
  }
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  state = Phase::Idle;
  resultPending = false;
  changed = true;
}

void WiFiConnectionService::startAutoScan() {
  if (!autoEnabled || autoState == AutoPhase::Paused || !profiles || !profiles->count()) return;
  if (WiFi.status() == WL_CONNECTED) {
    state = Phase::Connected;
    autoState = AutoPhase::Online;
    changed = true;
    return;
  }
  if (state == Phase::Connecting || state == Phase::Scanning) return;
  WiFi.mode(WIFI_STA);
  WiFi.scanDelete();
  candidateCount = candidateNext = 0;
  autoScanOwner = true;
  started = millis();
  state = Phase::Scanning;
  autoState = AutoPhase::Scanning;
  failure[0] = 0;
  changed = true;
  const int n = WiFi.scanNetworks(true, true);
  if (n >= 0) consumeScan(n);
  else if (n != -1) {
    autoScanOwner = false;
    autoRetry("Saved-network scan failed");
  }
}

void WiFiConnectionService::joinNextCandidate() {
  if (!autoEnabled || autoState == AutoPhase::Paused || !profiles) return;
  if (candidateNext >= candidateCount) {
    autoRetry("Saved networks unavailable");
    return;
  }
  const int index = candidateProfile[candidateNext++];
  const WiFiProfile &candidate = profiles->at(index);
  targetSSID = candidate.ssid;
  pendingPassword = candidate.open ? String() : candidate.password;
  joiningOpen = candidate.open;
  started = millis();
  failure[0] = 0;
  state = Phase::Connecting;
  autoJoinOwner = true;
  autoState = AutoPhase::Connecting;
  changed = true;
  // Another AP might have just connected while we scanned. Do not drop it.
  if (WiFi.status() == WL_CONNECTED) {
    state = Phase::Connected;
    autoJoinOwner = false;
    autoState = AutoPhase::Online;
    return;
  }
  WiFi.setAutoReconnect(false);
  WiFi.begin(targetSSID.c_str(), joiningOpen ? nullptr : pendingPassword.c_str());
}

void WiFiConnectionService::autoRetry(const char *reason) {
  // No repeated notifications in background: the Home status line shows
  // the next retry countdown and System's standard WiFi watcher handles joins.
  snprintf(failure, sizeof(failure), "%s", reason ? reason : "Saved network unavailable");
  pendingPassword = "";
  autoScanOwner = autoJoinOwner = false;
  state = Phase::Failed;
  autoState = AutoPhase::Waiting;
  autoDue = millis() + autoBackoffMs;
  if (autoBackoffMs < 120000UL) autoBackoffMs *= 2UL;
  changed = true;
}

void WiFiConnectionService::poll() {
  if (autoEnabled && autoState == AutoPhase::Online) {
    if (WiFi.status() == WL_CONNECTED) disconnectedAt = 0;
    else if (!disconnectedAt) disconnectedAt = millis() ? millis() : 1;
    else if ((uint32_t)(millis() - disconnectedAt) >= 5000UL &&
             state != Phase::Scanning && state != Phase::Connecting) {
      autoState = AutoPhase::Scheduled;
      autoDue = millis();
      state = Phase::Idle;
      changed = true;
    }
  }
  if (autoEnabled && (autoState == AutoPhase::Scheduled || autoState == AutoPhase::Waiting) &&
      (int32_t)(millis() - autoDue) >= 0 &&
      state != Phase::Scanning && state != Phase::Connecting) {
    startAutoScan();
  }
  if (state == Phase::Scanning) {
    const int n = WiFi.scanComplete();
    if (n >= 0) consumeScan(n);
    else if (n != -1 || elapsedMs() > 12000UL) {
      // A scan error cannot be mistaken for a failed saved password.
      if (autoScanOwner) {
        autoScanOwner = false;
        WiFi.scanDelete();
        autoRetry(n == -1 ? "Saved-network scan timeout" : "Saved-network scan failed");
      } else {
        WiFi.scanDelete();
        targetSSID = "";
        finish(false, n == -1 ? "WiFi scan timed out" : "WiFi scan failed");
      }
    }
  } else if (state == Phase::Connecting) {
    const int status = WiFi.status();
    if (autoJoinOwner) {
      if (status == WL_CONNECTED && WiFi.SSID() == targetSSID) {
        state = Phase::Connected;
        autoJoinOwner = false;
        autoState = AutoPhase::Online;
        disconnectedAt = 0;
        autoBackoffMs = 30000UL;
        if (profiles && profiles->lastSSID() != targetSSID) profiles->setLastSSID(targetSSID);
        pendingPassword = "";
        changed = true;
      } else if ((elapsedMs() >= 3000UL &&
                 (status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED)) ||
                 elapsedMs() >= 12000UL) {
        pendingPassword = "";
        // Do not consume a stale result as a successful association to the
        // next SSID. Each subsequent attempt has its own 12s deadline.
        if (WiFi.status() != WL_CONNECTED) WiFi.disconnect();
        autoJoinOwner = false;
        joinNextCandidate();
      }
    } else {
      if (status == WL_CONNECTED && WiFi.SSID() == targetSSID) finish(true);
      else if (elapsedMs() >= 3000UL && status == WL_NO_SSID_AVAIL) finish(false, "Network not found");
      else if (elapsedMs() >= 3000UL && status == WL_CONNECT_FAILED) finish(false, "Connection rejected - check key");
      else if (elapsedMs() >= 12000UL) finish(false, "Connection timed out (12s)");
    }
  }
}

bool WiFiConnectionService::takeChanged() { bool value = changed; changed = false; return value; }
bool WiFiConnectionService::takeConnectionResult(bool &successful) {
  if (!resultPending) return false;
  resultPending = false;
  successful = resultSuccess;
  return true;
}
