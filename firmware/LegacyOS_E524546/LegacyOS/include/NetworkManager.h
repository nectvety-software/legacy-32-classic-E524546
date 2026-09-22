#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ═══════════════════════════════════════════════════════════
//  NetworkManager - WiFi + BLE networking stack
// ═══════════════════════════════════════════════════════════

#define NTP_SERVER    "pool.ntp.org"
#define NTP_OFFSET    25200   // UTC+7 (Vietnam)
#define WIFI_TIMEOUT  10000

// BLE UART service UUIDs
#define BLE_SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_RX_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_TX_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

struct WiFiProfile {
  String ssid;
  String password;
};

class NetMgr {
public:
  bool wifiEnabled   = false;
  bool bleEnabled    = false;
  bool wifiConnected = false;
  bool bleConnected  = false;
  
  String currentSSID;
  String localIP;
  int    wifiRSSI;
  
  // BLE
  BLEServer*      bleServer    = nullptr;
  BLECharacteristic* bleTxChar = nullptr;
  BLECharacteristic* bleRxChar = nullptr;
  String          bleRxBuffer;
  bool            bleNewData = false;
  
  // Saved profiles
  std::vector<WiFiProfile> savedProfiles;
  
  void init(Preferences& prefs) {
    _prefs = &prefs;
    _loadProfiles();
    Serial.println(F("[Net] NetworkManager initialized"));
  }
  
  // ─── WiFi ─────────────────────────────────────────────────
  bool wifiConnect(const String& ssid, const String& pass) {
    Serial.printf("[Net] Connecting to: %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_TIMEOUT) {
      delay(200);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      currentSSID   = ssid;
      localIP       = WiFi.localIP().toString();
      wifiRSSI      = WiFi.RSSI();
      wifiEnabled   = true;
      _saveProfile(ssid, pass);
      Serial.printf("[Net] WiFi connected: %s\n", localIP.c_str());
      
      // Sync NTP time
      syncNTP();
      return true;
    }
    
    Serial.println(F("[Net] WiFi connect failed"));
    return false;
  }
  
  void wifiDisconnect() {
    WiFi.disconnect();
    wifiConnected = false;
    wifiEnabled   = false;
    currentSSID   = "";
  }
  
  void wifiDisable() {
    WiFi.mode(WIFI_OFF);
    wifiEnabled = wifiConnected = false;
  }
  
  // Auto-connect to saved profiles
  bool wifiAutoConnect() {
    for (auto& p : savedProfiles) {
      if (wifiConnect(p.ssid, p.password)) return true;
    }
    return false;
  }
  
  void update() {
    if (wifiEnabled) {
      wifiConnected = (WiFi.status() == WL_CONNECTED);
      if (wifiConnected) {
        wifiRSSI = WiFi.RSSI();
        localIP  = WiFi.localIP().toString();
      }
    }
  }
  
  int getSignalStrength() {
    if (!wifiConnected) return 0;
    int r = abs(wifiRSSI);
    if (r < 55) return 4;
    if (r < 65) return 3;
    if (r < 75) return 2;
    if (r < 85) return 1;
    return 0;
  }
  
  // NTP time sync
  void syncNTP() {
    if (!wifiConnected) return;
    configTime(NTP_OFFSET, 0, NTP_SERVER);
    Serial.println(F("[Net] NTP syncing..."));
  }
  
  // Get current time string
  String getTimeString() {
    struct tm ti;
    if (!getLocalTime(&ti, 1000)) return "--:--";
    char buf[8];
    snprintf(buf, 8, "%02d:%02d", ti.tm_hour, ti.tm_min);
    return String(buf);
  }
  
  String getDateString() {
    struct tm ti;
    if (!getLocalTime(&ti, 1000)) return "----";
    char buf[20];
    const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    snprintf(buf, 20, "%s %02d/%02d/%04d",
      days[ti.tm_wday], ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900);
    return String(buf);
  }
  
  // HTTP GET helper
  String httpGet(const String& url, int timeout = 5000) {
    if (!wifiConnected) return "";
    HTTPClient http;
    http.begin(url);
    http.setTimeout(timeout);
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
      String payload = http.getString();
      http.end();
      return payload;
    }
    http.end();
    return "";
  }
  
  // ─── BLE ──────────────────────────────────────────────────
  class BLECallbacks : public BLEServerCallbacks {
  public:
    NetMgr* mgr;
    BLECallbacks(NetMgr* m) : mgr(m) {}
    void onConnect(BLEServer* s) override    { mgr->bleConnected = true; }
    void onDisconnect(BLEServer* s) override { mgr->bleConnected = false; 
      s->startAdvertising(); }
  };
  
  class BLERxCallbacks : public BLECharacteristicCallbacks {
  public:
    NetMgr* mgr;
    BLERxCallbacks(NetMgr* m) : mgr(m) {}
    void onWrite(BLECharacteristic* c) override {
      mgr->bleRxBuffer = c->getValue().c_str();
      mgr->bleNewData  = true;
    }
  };
  
  void bleStart() {
    if (bleEnabled) return;
    BLEDevice::init("LEGACY-32-E524546");
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new BLECallbacks(this));
    
    BLEService* svc = bleServer->createService(BLE_SERVICE_UUID);
    
    bleTxChar = svc->createCharacteristic(BLE_TX_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);
    bleTxChar->addDescriptor(new BLE2902());
    
    bleRxChar = svc->createCharacteristic(BLE_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE);
    bleRxChar->setCallbacks(new BLERxCallbacks(this));
    
    svc->start();
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
    
    bleEnabled = true;
    Serial.println(F("[Net] BLE UART started"));
  }
  
  void bleStop() {
    if (!bleEnabled) return;
    BLEDevice::deinit(true);
    bleEnabled = bleConnected = false;
  }
  
  void bleSend(const String& msg) {
    if (bleEnabled && bleConnected && bleTxChar) {
      bleTxChar->setValue(msg.c_str());
      bleTxChar->notify();
    }
  }
  
  bool bleHasData() {
    if (bleNewData) { bleNewData = false; return true; }
    return false;
  }
  
  String bleReadLine() {
    String s = bleRxBuffer;
    bleRxBuffer = "";
    return s;
  }

private:
  Preferences* _prefs;
  
  void _saveProfile(const String& ssid, const String& pass) {
    for (auto& p : savedProfiles) {
      if (p.ssid == ssid) { p.password = pass; return; }
    }
    savedProfiles.push_back({ssid, pass});
    // Persist
    String key = "wifi_" + String(savedProfiles.size());
    _prefs->putString((key + "_s").c_str(), ssid);
    _prefs->putString((key + "_p").c_str(), pass);
  }
  
  void _loadProfiles() {
    for (int i = 1; i <= 5; i++) {
      String key = "wifi_" + String(i);
      String s = _prefs->getString((key + "_s").c_str(), "");
      String p = _prefs->getString((key + "_p").c_str(), "");
      if (s.length() > 0) savedProfiles.push_back({s, p});
    }
    Serial.printf("[Net] Loaded %d WiFi profiles\n", savedProfiles.size());
  }
};
