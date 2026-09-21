#ifndef WIFI_APP_H
#define WIFI_APP_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#define MAX_SCAN_NETWORKS 50
#define MAX_SAVED_NETWORKS 20
#define MAX_TARGETS 10

enum WifiMode {
    WIFI_MODE_SELECT,
    WIFI_MODE_ORIGINAL,
    WIFI_MODE_ADVANCED
};

enum WifiState {
    WIFI_MAIN,
    WIFI_SCAN_MENU,
    WIFI_SCAN_AP,
    WIFI_SCAN_LIVE,
    WIFI_SCAN_STATIONS,
    WIFI_CHANNEL_STATUS,
    WIFI_TARGET_MENU,
    WIFI_TARGET_SELECT,
    WIFI_TARGET_LIST,
    WIFI_OFFENSE_MENU,
    WIFI_OFFENSE_DEAUTH,
    WIFI_OFFENSE_SELECT_TARGET,
    WIFI_NETWORK_MENU,
    WIFI_NETWORK_CONNECT,
    WIFI_NETWORK_DISCONNECT,
    WIFI_NETWORK_DEVICES,
    WIFI_NETWORK_PORTS,
    WIFI_NETWORK_SSH,
    WIFI_NETWORK_INFO,
    WIFI_OUTPUT_MENU,
    WIFI_OUTPUT_USB,
    WIFI_OUTPUT_SD,
    WIFI_ATTRIBUTES,
    WIFI_AP_MODE,
    WIFI_CONNECTING,
    WIFI_KEYBOARD,
    WIFI_SAVED_NETWORKS,
    WIFI_SAVED_OPTIONS,
    WIFI_STATUS
};

#define WIFI_MODE_COUNT 2

enum ScanPhase {
    PHASE_WIFI,
    PHASE_STATIONS,
    PHASE_BLE,
    PHASE_COMPLETE
};

struct NetworkInfo {
    String ssid;
    String password;
    String bssid;
    int rssi;
    int channel;
    int encryption;
    bool encrypted;
    bool isConnected;
    bool isTargeted;
    int signalQuality;
};

struct StationInfo {
    String mac;
    String apMac;
    int rssi;
    bool isTargeted;
};

struct SweepResult {
    int wifiCount;
    int stationCount;
    int bleCount;
    unsigned long duration;
    bool isRunning;
    ScanPhase phase;
};

extern WifiState wifiState;
extern WifiMode wifiMode;
extern int wifiCursor;
extern int wifiModeCursor;
extern int wifiScrollOffset;
extern bool wifiConnected;
extern bool apModeActive;
extern String connectedSSID;

extern NetworkInfo scanResults[MAX_SCAN_NETWORKS];
extern int scanResultCount;
extern int selectedAP;

extern NetworkInfo savedNetworks[MAX_SAVED_NETWORKS];
extern int savedNetworkCount;
extern int selectedSavedNetwork;

extern StationInfo stations[MAX_TARGETS];
extern int stationCount;

extern SweepResult sweepData;
extern int targetedCount;
extern int targetIndexes[MAX_TARGETS];

extern Preferences wifiPrefs;

void wifiAppInit();
void appWiFiMenu();

void wifiDrawModeSelect();
void wifiModeSelectHandle();

void wifiLoadSavedNetworks();
void wifiSaveNetwork(const String& ssid, const String& pass);
void wifiDeleteNetwork(int index);
void wifiConnectToSaved(int index);
void wifiConnectToNetwork(const String& ssid, const String& pass, bool staticIP,
                         const String& ip, const String& gateway, const String& subnet, const String& dns);
void wifiDisconnect();
void wifiStartAP();
void wifiStopAP();

void wifiScanNetworks();
void wifiScanStations();
void wifiStartLiveScan();
void wifiStartSweep();
void wifiStartDeauth();
void wifiStopDeauth();

int wifiGetSignalQuality(int rssi);
String wifiGetAuthType(int enc);
String wifiGetEncryptionName(int enc);
bool wifiIsPMFRequired(int enc);

void wifiDrawMain();
void wifiDrawHelp();
void wifiDrawScanMenu();
void wifiDrawScanAP();
void wifiDrawScanLive();
void wifiDrawScanStations();
void wifiDrawChannelStatus();
void wifiDrawTargetMenu();
void wifiDrawTargetSelect();
void wifiDrawTargetList();
void wifiDrawOffenseMenu();
void wifiDrawOffenseDeauth();
void wifiDrawNetworkMenu();
void wifiDrawNetworkConnect();
void wifiDrawNetworkDevices();
void wifiDrawNetworkPorts();
void wifiDrawNetworkSSH();
void wifiDrawOutputMenu();
void wifiDrawOutputUSB();
void wifiDrawSavedNetworks();
void wifiDrawSavedOptions();
void wifiDrawNetworkInfo();
void wifiDrawConnecting();
void wifiDrawStatus();
void wifiDrawAttributes();
void wifiDrawAPMode();

void wifiKeyboardDraw();
void wifiKeyboardHandle();

void wifiInputHandler();

#endif
