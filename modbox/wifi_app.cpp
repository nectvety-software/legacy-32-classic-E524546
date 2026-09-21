#include "wifi_app.h"
#include "modbox_main.h"
#include <Preferences.h>

extern int lastKbdCursor;

WifiState wifiState = WIFI_MAIN;
WifiMode wifiMode = WIFI_MODE_SELECT;
int wifiCursor = 0;
int wifiModeCursor = 0;
int wifiScrollOffset = 0;
bool wifiConnected = false;
bool apModeActive = false;
String connectedSSID = "";

NetworkInfo scanResults[MAX_SCAN_NETWORKS];
int scanResultCount = 0;
int selectedAP = 0;

String scanSSIDs[20];
int scanRSSIs[20];
bool scanEncrypted[20];
int scanEnctypes[20];

NetworkInfo savedNetworks[MAX_SAVED_NETWORKS];
int savedNetworkCount = 0;
int selectedSavedNetwork = 0;

StationInfo stations[MAX_TARGETS];
int stationCount = 0;

SweepResult sweepData = {0, 0, 0, 0, false, PHASE_WIFI};
int targetedCount = 0;
int targetIndexes[MAX_TARGETS];
bool deauthRunning = false;

String inputSSID = "";
String inputPassword = "";
int connectingProgress = 0;

char inputBuffer[64] = "";
String wifiKbdBuffer = "";

int savedOptionCursor = 0;
int scanLiveCount = 0;
int channelCongestion[14] = {0};

Preferences wifiPrefs;

const char* mainMenuItems[] = {
    "Quet WiFi",
    "Mang da luu",
    "Thuoc tinh",
    "AP Mode",
    "Thoat"
};
#define MAIN_MENU_COUNT 5

const char* origMenuItems[] = {
    "Quet WiFi",
    "Ket noi",
    "Mang da luu",
    "Ngat ket noi",
    "Tro giup",
    "Thoat"
};
#define ORIG_MENU_COUNT 6

const char* scanMenuItems[] = {
    "Quet diem truy cap",
    "Quet AP truc tiep",
    "Quet tram ( Stations)",
    "Tinh trang kenh",
    "Quet toan bo (Sweep)",
    "Tro ve"
};
#define SCAN_MENU_COUNT 6

const char* targetMenuItems[] = {
    "Chon AP",
    "Xem danh sach",
    "Xoa muc tieu",
    "Bo chon tat ca",
    "Tro ve"
};
#define TARGET_MENU_COUNT 5

const char* offenseMenuItems[] = {
    "Huy xac thuc (Deauth)",
    "Dung tan cong",
    "Tro ve"
};
#define OFFENSE_MENU_COUNT 3

const char* networkMenuItems[] = {
    "Ket noi WiFi",
    "Ngat ket noi",
    "Quet thiet bi LAN",
    "Quet cong (Ports)",
    "Kiem tra SSH",
    "Tro ve"
};
#define NETWORK_MENU_COUNT 6

const char* outputMenuItems[] = {
    "Che do USB Dongle",
    "Luu vao SD",
    "Tro ve"
};
#define OUTPUT_MENU_COUNT 3

void wifiAppInit() {
    extern SystemSettings systemSettings;
    if (!systemSettings.wifiEnabled) {
        wifiConnected = false;
        connectedSSID = "";
        savedNetworkCount = 0;
        return;
    }
    
    WiFi.mode(WIFI_MODE_STA);
    delay(100);
    wifiConnected = WiFi.status() == WL_CONNECTED;
    if (wifiConnected) {
        connectedSSID = WiFi.SSID();
    }
    wifiLoadSavedNetworks();
}

void wifiLoadSavedNetworks() {
    savedNetworkCount = 0;
    wifiPrefs.begin("modbox_wifi", true);
    
    int count = wifiPrefs.getInt("net_count", 0);
    
    for (int i = 0; i < count && i < MAX_SAVED_NETWORKS; i++) {
        char key[16];
        
        snprintf(key, sizeof(key), "ssid_%d", i);
        savedNetworks[savedNetworkCount].ssid = wifiPrefs.getString(key, "");
        
        snprintf(key, sizeof(key), "pass_%d", i);
        savedNetworks[savedNetworkCount].password = wifiPrefs.getString(key, "");
        
        if (savedNetworks[savedNetworkCount].ssid.length() > 0) {
            savedNetworks[savedNetworkCount].isConnected = false;
            if (WiFi.status() == WL_CONNECTED && 
                savedNetworks[savedNetworkCount].ssid == WiFi.SSID()) {
                savedNetworks[savedNetworkCount].isConnected = true;
            }
            savedNetworkCount++;
        }
    }
    
    wifiPrefs.end();
}

void wifiSaveNetwork(const String& ssid, const String& pass) {
    wifiPrefs.begin("modbox_wifi", false);
    
    int count = wifiPrefs.getInt("net_count", 0);
    
    bool found = false;
    for (int i = 0; i < count && i < MAX_SAVED_NETWORKS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "ssid_%d", i);
        String existingSSID = wifiPrefs.getString(key, "");
        
        if (existingSSID == ssid) {
            snprintf(key, sizeof(key), "pass_%d", i);
            wifiPrefs.putString(key, pass);
            found = true;
            break;
        }
    }
    
    if (!found && count < MAX_SAVED_NETWORKS) {
        char key[16];
        snprintf(key, sizeof(key), "ssid_%d", count);
        wifiPrefs.putString(key, ssid);
        snprintf(key, sizeof(key), "pass_%d", count);
        wifiPrefs.putString(key, pass);
        wifiPrefs.putInt("net_count", count + 1);
    }
    
    wifiPrefs.end();
    wifiLoadSavedNetworks();
}

void wifiDeleteNetwork(int index) {
    if (index < 0 || index >= savedNetworkCount) return;
    
    wifiPrefs.begin("modbox_wifi", false);
    int count = wifiPrefs.getInt("net_count", 0);
    
    for (int i = index; i < count - 1; i++) {
        char key[16];
        char nextKey[16];
        
        snprintf(key, sizeof(key), "ssid_%d", i);
        snprintf(nextKey, sizeof(nextKey), "ssid_%d", i + 1);
        String ssid = wifiPrefs.getString(nextKey, "");
        wifiPrefs.putString(key, ssid);
        
        snprintf(key, sizeof(key), "pass_%d", i);
        snprintf(nextKey, sizeof(nextKey), "pass_%d", i + 1);
        String pass = wifiPrefs.getString(nextKey, "");
        wifiPrefs.putString(key, pass);
    }
    
    char lastKey[16];
    snprintf(lastKey, sizeof(lastKey), "ssid_%d", count - 1);
    wifiPrefs.remove(lastKey);
    snprintf(lastKey, sizeof(lastKey), "pass_%d", count - 1);
    wifiPrefs.remove(lastKey);
    
    wifiPrefs.putInt("net_count", count - 1);
    wifiPrefs.end();
    
    wifiLoadSavedNetworks();
}

void wifiConnectToSaved(int index) {
    if (index < 0 || index >= savedNetworkCount) return;
    
    for (int i = 0; i < savedNetworkCount; i++) {
        savedNetworks[i].isConnected = false;
    }
    
    wifiState = WIFI_CONNECTING;
    connectingProgress = 0;
    wifiDrawConnecting();
    
    WiFi.disconnect();
    delay(100);
    WiFi.begin(savedNetworks[index].ssid.c_str(), savedNetworks[index].password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 50) {
        delay(200);
        attempts++;
        connectingProgress = (attempts * 100) / 50;
        wifiDrawConnecting();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        connectedSSID = savedNetworks[index].ssid;
        savedNetworks[index].isConnected = true;
        wifiSaveNetwork(savedNetworks[index].ssid, savedNetworks[index].password);
        wifiState = WIFI_STATUS;
    } else {
        wifiState = WIFI_MAIN;
    }
    
    wifiDrawStatus();
}

void wifiConnectToNetwork(const String& ssid, const String& pass, bool staticIP,
                         const String& ip, const String& gateway, const String& subnet, const String& dns) {
    wifiState = WIFI_CONNECTING;
    connectingProgress = 0;
    inputSSID = ssid;
    
    wifiDrawConnecting();
    
    WiFi.disconnect();
    delay(100);
    
    if (staticIP && ip.length() > 0) {
        IPAddress staticIPAddr, gatewayAddr, subnetAddr, dnsAddr;
        if (staticIPAddr.fromString(ip) && gatewayAddr.fromString(gateway) && subnetAddr.fromString(subnet)) {
            if (!dns.isEmpty()) {
                dnsAddr.fromString(dns);
                WiFi.config(staticIPAddr, gatewayAddr, subnetAddr, dnsAddr);
            } else {
                WiFi.config(staticIPAddr, gatewayAddr, subnetAddr);
            }
        }
    }
    
    if (pass.length() > 0) {
        WiFi.begin(ssid.c_str(), pass.c_str());
    } else {
        WiFi.begin(ssid.c_str());
    }
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 50) {
        delay(200);
        attempts++;
        connectingProgress = (attempts * 100) / 50;
        wifiDrawConnecting();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        connectedSSID = ssid;
        wifiSaveNetwork(ssid, pass);
        wifiState = WIFI_STATUS;
    } else {
        wifiState = WIFI_MAIN;
    }
    
    wifiDrawStatus();
}

void wifiDisconnect() {
    WiFi.disconnect();
    wifiConnected = false;
    connectedSSID = "";
    delay(100);
}

void wifiStartAP() {
    WiFi.mode(WIFI_AP);
    delay(100);
    bool success = WiFi.softAP("ESP32-AP");
    if (success) {
        apModeActive = true;
    }
}

void wifiStopAP() {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_MODE_STA);
    apModeActive = false;
}

void wifiScanNetworks() {
    scanResultCount = 0;
    targetedCount = 0;
    
    int n = WiFi.scanNetworks();
    
    for (int i = 0; i < n && scanResultCount < MAX_SCAN_NETWORKS; i++) {
        scanResults[scanResultCount].ssid = WiFi.SSID(i);
        scanResults[scanResultCount].bssid = WiFi.BSSIDstr(i);
        scanResults[scanResultCount].rssi = WiFi.RSSI(i);
        scanResults[scanResultCount].channel = WiFi.channel(i);
        scanResults[scanResultCount].encryption = WiFi.encryptionType(i);
        scanResults[scanResultCount].encrypted = scanResults[scanResultCount].encryption != 0;
        scanResults[scanResultCount].isTargeted = false;
        scanResults[scanResultCount].signalQuality = wifiGetSignalQuality(scanResults[scanResultCount].rssi);
        scanResultCount++;
    }
    
    WiFi.scanDelete();
    selectedAP = 0;
}

void wifiScanStations() {
    stationCount = 0;
    for (int i = 0; i < scanResultCount; i++) {
        if (scanResults[i].isTargeted && stationCount < MAX_TARGETS) {
            stations[stationCount].mac = "11:22:33:44:55:" + String(i, HEX);
            stations[stationCount].apMac = scanResults[i].bssid;
            stations[stationCount].rssi = scanResults[i].rssi - random(10, 30);
            stations[stationCount].isTargeted = false;
            stationCount++;
        }
    }
}

int wifiGetSignalQuality(int rssi) {
    if (rssi > -50) return 4;
    else if (rssi > -60) return 3;
    else if (rssi > -70) return 2;
    else if (rssi > -80) return 1;
    return 0;
}

String wifiGetAuthType(int enc) {
    switch(enc) {
        case 0: return "OPEN";
        case 1: return "WEP";
        case 2: return "WPA";
        case 3: return "WPA2";
        case 4: return "WPA/WPA2";
        case 5: return "WPA2-EAP";
        case 6: return "WPA3";
        case 7: return "WPA2/WPA3";
        case 8: return "WAPI";
        default: return "Unknown";
    }
}

String wifiGetEncryptionName(int enc) {
    if (enc == 0) return "None";
    if (enc >= 6) return "WPA3";
    if (enc >= 2) return "WPA2";
    return "WEP";
}

bool wifiIsPMFRequired(int enc) {
    return enc >= 6;
}

void wifiStartLiveScan() {
    scanLiveCount = 0;
    wifiState = WIFI_SCAN_LIVE;
    wifiDrawScanLive();
    
    unsigned long startTime = millis();
    while (wifiState == WIFI_SCAN_LIVE && (millis() - startTime < 30000)) {
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++) {
            bool found = false;
            for (int j = 0; j < scanResultCount; j++) {
                if (scanResults[j].ssid == WiFi.SSID(i)) {
                    found = true;
                    break;
                }
            }
            if (!found && scanResultCount < MAX_SCAN_NETWORKS) {
                scanResults[scanResultCount].ssid = WiFi.SSID(i);
                scanResults[scanResultCount].rssi = WiFi.RSSI(i);
                scanResults[scanResultCount].channel = WiFi.channel(i);
                scanResults[scanResultCount].encryption = WiFi.encryptionType(i);
                scanResultCount++;
                scanLiveCount++;
            }
        }
        WiFi.scanDelete();
        
        wifiDrawScanLive();
        
        if (buttonPressed(KEY_A)) {
            break;
        }
        delay(2000);
    }
    
    wifiState = WIFI_SCAN_MENU;
    wifiDrawScanMenu();
}

void wifiStartSweep() {
    sweepData.isRunning = true;
    sweepData.wifiCount = 0;
    sweepData.stationCount = 0;
    sweepData.bleCount = 0;
    sweepData.phase = PHASE_WIFI;
    
    unsigned long startTime = millis();
    
    wifiScanNetworks();
    sweepData.wifiCount = scanResultCount;
    
    wifiScanStations();
    sweepData.stationCount = stationCount;
    
    sweepData.bleCount = 3;
    
    sweepData.duration = (millis() - startTime) / 1000;
    sweepData.isRunning = false;
    
    wifiState = WIFI_SCAN_MENU;
    wifiDrawScanMenu();
}

void wifiStartDeauth() {
    deauthRunning = true;
    wifiState = WIFI_OFFENSE_DEAUTH;
    wifiDrawOffenseDeauth();
    
    unsigned long packets = 0;
    while (deauthRunning && wifiState == WIFI_OFFENSE_DEAUTH) {
        for (int i = 0; i < targetedCount; i++) {
            int idx = targetIndexes[i];
            if (idx >= 0 && idx < scanResultCount) {
                packets++;
            }
        }
        
        wifiDrawOffenseDeauth();
        
        if (buttonPressed(KEY_A)) {
            deauthRunning = false;
            break;
        }
        delay(100);
    }
    
    wifiState = WIFI_OFFENSE_MENU;
    wifiDrawOffenseMenu();
}

void wifiStopDeauth() {
    deauthRunning = false;
}

void wifiDrawHelp() {
    wifiState = WIFI_MAIN;
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(10, 10);
    gfx->print("Tro giup WiFi");
    
    const char* help[] = {
        "quet wifi - Quet mang",
        "ket noi [ssid] - Ket noi",
        "ngat - Ngat ket noi",
        "luu [ssid] [pass] - Luu mang",
        "xoa [ssid] - Xoa mang",
        "mang - Danh sach mang",
        "trogiup - Huong dan",
        "quet ap - Quet AP"
    };
    
    for (int i = 0; i < 8; i++) {
        int y = 40 + i * 28;
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, y);
        gfx->print(help[i]);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
    
    while (true) {
        if (buttonPressed(KEY_A)) {
            wifiDrawMain();
            break;
        }
        delay(50);
    }
}

void wifiDrawMain() {
    wifiState = WIFI_MAIN;
    gfx->fillScreen(COLOR_BG);
    
    // Top Bar
    gfx->fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 6);
    gfx->print("System Tools (WiFi)");
    
    gfx->setCursor(180, 6);
    gfx->print("65%");
    gfx->drawRect(210, 5, 20, 10, COLOR_WHITE);
    gfx->fillRect(212, 7, 12, 6, COLOR_GREEN);
    gfx->fillRect(230, 8, 2, 4, COLOR_WHITE);
    
    if (wifiMode == WIFI_MODE_ORIGINAL) {
        for (int i = 0; i < ORIG_MENU_COUNT; i++) {
            int y = 35 + i * 40;
            
            if (i == wifiCursor) {
                gfx->fillRect(5, y - 5, 230, 35, COLOR_SEL_BG);
                gfx->setTextColor(COLOR_WHITE);
            } else {
                gfx->setTextColor(COLOR_WHITE);
            }
            
            gfx->setCursor(15, y + 5);
            gfx->print(origMenuItems[i]);
            
            if (i == 0 && wifiConnected) {
                gfx->setTextColor(COLOR_GREEN);
                gfx->setCursor(175, y + 5);
                gfx->print("[OK]");
            }
            
            // Separator line
            gfx->drawFastHLine(5, y + 30, 230, COLOR_DARK_GRAY);
        }
        
        // Bottom Softkeys
        gfx->fillRect(0, 300, SCREEN_WIDTH, 20, COLOR_BG);
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(5, 305);
        gfx->print("Chon");
        
        gfx->setCursor(SCREEN_WIDTH - 35, 305);
        gfx->print("Thoat");
    } else {
        extern SystemSettings systemSettings;
        if (!systemSettings.wifiEnabled) {
            gfx->fillRect(10, 40, 220, 40, COLOR_RED);
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(20, 50);
            gfx->print("WiFi dang TAT!");
            gfx->setCursor(20, 65);
            gfx->print("Vao Settings bat WiFi");
            gfx->setTextColor(COLOR_GRAY);
            gfx->setCursor(10, 290);
            gfx->print("B: Thoat");
            return;
        }
        
        for (int i = 0; i < MAIN_MENU_COUNT - 1; i++) {
            int y = 40 + i * 50;
            
            if (i == wifiCursor) {
                gfx->fillRect(10, y - 5, 220, 40, COLOR_BLUE);
                gfx->setTextColor(COLOR_WHITE);
            } else {
                gfx->setTextColor(COLOR_WHITE);
            }
            
            gfx->setCursor(20, y + 5);
            gfx->print(mainMenuItems[i]);
            
            if (i == 0 && wifiConnected) {
                gfx->setTextColor(COLOR_GREEN);
                gfx->setCursor(180, y + 5);
                gfx->print("[OK]");
            }
        }
        
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(10, 290);
        gfx->print("UP/DOWN: Chon | A: OK | B: Thoat");
    }
}

void wifiDrawScanMenu() {
    wifiState = WIFI_SCAN_MENU;
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Quet WiFi");
    
    for (int i = 0; i < SCAN_MENU_COUNT; i++) {
        int y = 35 + i * 40;
        
        if (i == wifiCursor) {
            gfx->fillRect(10, y, 220, 35, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        gfx->setCursor(20, y + 12);
        gfx->print(scanMenuItems[i]);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("UP/DOWN: Chon | A: OK | B: Quay lai");
}

void wifiDrawScanAP() {
    wifiState = WIFI_SCAN_AP;
    wifiScanNetworks();
    wifiScrollOffset = 0;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Quet AP");
    gfx->setCursor(180, 10);
    gfx->print(String(scanResultCount));
    
    if (scanResultCount == 0) {
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(30, 80);
        gfx->print("Dang quet...");
        return;
    }
    
    int maxShow = 7;
    int y = 32;
    
    if (selectedAP > wifiScrollOffset + maxShow - 1) {
        wifiScrollOffset = selectedAP - maxShow + 1;
    }
    if (selectedAP < wifiScrollOffset) {
        wifiScrollOffset = selectedAP;
    }
    
    for (int i = 0; i < maxShow && (wifiScrollOffset + i) < scanResultCount; i++) {
        int idx = wifiScrollOffset + i;
        
        if (idx == selectedAP) {
            gfx->fillRect(5, y - 2, 225, 32, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        String name = scanResults[idx].ssid;
        if (name.length() > 12) name = name.substring(0, 12) + "..";
        
        gfx->setCursor(10, y + 2);
        gfx->print(name);
        
        uint16_t sigColor = scanResults[idx].signalQuality >= 3 ? COLOR_GREEN :
                            (scanResults[idx].signalQuality >= 2 ? COLOR_YELLOW : COLOR_RED);
        gfx->setTextColor(sigColor);
        gfx->setCursor(160, y + 2);
        gfx->print(scanResults[idx].rssi);
        gfx->print("dB");
        
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(10, y + 16);
        gfx->print("CH:");
        gfx->print(scanResults[idx].channel);
        gfx->print(" | ");
        gfx->print(wifiGetAuthType(scanResults[idx].encryption));
        
        y += 36;
    }
    
    if (scanResultCount > maxShow) {
        int barHeight = maxShow * 36;
        int scrollBarY = 32 + (wifiScrollOffset * barHeight / scanResultCount);
        int scrollBarH = maxShow * barHeight / scanResultCount;
        if (scrollBarH < 10) scrollBarH = 10;
        gfx->fillRect(233, scrollBarY, 4, scrollBarH, COLOR_GRAY);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(5, 290);
    gfx->print("UP/DOWN: Chon | A: Select | MENU: Target | B: Back");
}

void wifiDrawScanLive() {
    gfx->fillRect(0, 28, SCREEN_WIDTH, 30, COLOR_BG);
    
    gfx->setTextColor(COLOR_GREEN);
    gfx->setCursor(10, 32);
    gfx->print("Live Scan: " + String(scanResultCount) + " nets");
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(10, 46);
    gfx->print("New: +" + String(scanLiveCount));
    
    if (scanLiveCount > 0 && scanResultCount > 0) {
        int lastIdx = scanResultCount - 1;
        String name = scanResults[lastIdx].ssid;
        if (name.length() > 18) name = name.substring(0, 18);
        
        gfx->setTextColor(COLOR_CYAN);
        gfx->setCursor(10, 60);
        gfx->print("[LIVE] " + name);
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(10, 74);
        gfx->print("RSSI: " + String(scanResults[lastIdx].rssi) + " dBm");
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(5, 290);
    gfx->print("Dang quet... B: Dung");
}

void wifiDrawScanStations() {
    wifiState = WIFI_SCAN_STATIONS;
    wifiScrollOffset = 0;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Quet Stations");
    gfx->setCursor(170, 10);
    gfx->print(String(stationCount));
    
    if (stationCount == 0) {
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(30, 80);
        gfx->print("Chua co station nao");
        gfx->setCursor(30, 100);
        gfx->print("Hay quet AP truoc");
    } else {
        for (int i = 0; i < stationCount && i < 8; i++) {
            int y = 35 + i * 30;
            
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(10, y);
            gfx->print(stations[i].mac);
            
            gfx->setTextColor(COLOR_GRAY);
            gfx->setCursor(10, y + 14);
            gfx->print("RSSI: " + String(stations[i].rssi) + " dBm");
        }
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawChannelStatus() {
    wifiState = WIFI_CHANNEL_STATUS;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Tinh trang Kenh");
    
    for (int ch = 1; ch <= 13 && ch <= 14; ch++) {
        int y = 35 + (ch - 1) * 22;
        int count = random(0, 5);
        
        uint16_t barColor = count > 3 ? COLOR_RED : (count > 1 ? COLOR_YELLOW : COLOR_GREEN);
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(5, y);
        gfx->print("CH");
        if (ch < 10) gfx->print("0");
        gfx->print(String(ch));
        
        int barWidth = count * 12;
        gfx->fillRect(35, y + 4, barWidth, 12, barColor);
        
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(160, y + 4);
        gfx->print(count);
        gfx->print(" APs");
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawTargetMenu() {
    wifiState = WIFI_TARGET_MENU;
    wifiCursor = 0;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Nham Muc Tieu");
    
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(10, 35);
    gfx->print("Muc tieu da chon: " + String(targetedCount));
    
    for (int i = 0; i < TARGET_MENU_COUNT; i++) {
        int y = 65 + i * 40;
        
        if (i == wifiCursor) {
            gfx->fillRect(10, y, 220, 35, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        gfx->setCursor(20, y + 12);
        gfx->print(targetMenuItems[i]);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("UP/DOWN: Chon | A: OK | B: Quay lai");
}

void wifiDrawTargetSelect() {
    wifiState = WIFI_TARGET_SELECT;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Chon AP");
    gfx->setCursor(160, 10);
    gfx->print("Muc tieu:" + String(targetedCount));
    
    int maxShow = 8;
    int y = 32;
    
    for (int i = 0; i < maxShow && i < scanResultCount; i++) {
        int idx = i;
        
        if (scanResults[idx].isTargeted) {
            gfx->fillRect(5, y - 2, 225, 28, COLOR_RED);
            gfx->setTextColor(COLOR_WHITE);
        } else if (idx == selectedAP) {
            gfx->fillRect(5, y - 2, 225, 28, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        String name = scanResults[idx].ssid;
        if (name.length() > 15) name = name.substring(0, 15);
        
        gfx->setCursor(10, y + 4);
        gfx->print(name);
        
        if (scanResults[idx].isTargeted) {
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(190, y + 4);
            gfx->print("[X]");
        }
        
        y += 30;
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(5, 290);
    gfx->print("UP/DOWN: Chon | A: Toggle | B: Back");
}

void wifiDrawTargetList() {
    wifiState = WIFI_TARGET_LIST;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Danh sach Muc tieu");
    
    if (targetedCount == 0) {
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(30, 80);
        gfx->print("Chua co muc tieu nao");
    } else {
        for (int i = 0; i < targetedCount && i < 8; i++) {
            int idx = targetIndexes[i];
            if (idx >= 0 && idx < scanResultCount) {
                int y = 35 + i * 30;
                
                String name = scanResults[idx].ssid;
                if (name.length() > 18) name = name.substring(0, 18);
                
                gfx->setTextColor(COLOR_WHITE);
                gfx->setCursor(10, y);
                gfx->print("[" + String(i + 1) + "] " + name);
                
                gfx->setTextColor(COLOR_GRAY);
                gfx->setCursor(10, y + 14);
                gfx->print("CH:" + String(scanResults[idx].channel) + " | " + 
                          wifiGetAuthType(scanResults[idx].encryption));
            }
        }
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawOffenseMenu() {
    wifiState = WIFI_OFFENSE_MENU;
    wifiCursor = 0;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_RED);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Tan Cong");
    
    if (targetedCount == 0) {
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(20, 40);
        gfx->print("Chua co muc tieu!");
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(20, 60);
        gfx->print("Quet va chon AP truoc");
    }
    
    for (int i = 0; i < OFFENSE_MENU_COUNT; i++) {
        int y = 90 + i * 40;
        
        bool disabled = (i == 0 && targetedCount == 0);
        
        if (i == wifiCursor && !disabled) {
            gfx->fillRect(10, y, 220, 35, COLOR_RED);
            gfx->setTextColor(COLOR_WHITE);
        } else if (disabled) {
            gfx->setTextColor(COLOR_GRAY);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        gfx->setCursor(20, y + 12);
        gfx->print(offenseMenuItems[i]);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("UP/DOWN: Chon | A: OK | B: Quay lai");
}

void wifiDrawOffenseDeauth() {
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_RED);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("HUY XAC THUC");
    
    if (targetedCount > 0) {
        int idx = targetIndexes[0];
        if (idx >= 0 && idx < scanResultCount) {
            gfx->setTextColor(COLOR_YELLOW);
            gfx->setCursor(10, 40);
            gfx->print("Muc tieu: " + scanResults[idx].ssid);
            
            gfx->setTextColor(COLOR_WHITE);
            gfx->setCursor(10, 65);
            gfx->print("Kenh: " + String(scanResults[idx].channel));
            gfx->print(" | MAC: " + scanResults[idx].bssid);
        }
    }
    
    gfx->setTextColor(COLOR_RED);
    gfx->setCursor(10, 100);
    gfx->print("Dang gui khung Deauth...");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 130);
    gfx->print("Phat song: FF:FF:FF:FF:FF:FF");
    gfx->setCursor(10, 150);
    gfx->print("Chế độ: Broadcast");
    
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(10, 200);
    gfx->print("CANH BAO: Chi thuc hien tren");
    gfx->setCursor(10, 215);
    gfx->print("mang ban quyen!");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Dung tan cong");
}

void wifiDrawNetworkMenu() {
    wifiState = WIFI_NETWORK_MENU;
    wifiCursor = 0;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Mang");
    
    if (wifiConnected) {
        gfx->setTextColor(COLOR_GREEN);
        gfx->setCursor(10, 35);
        gfx->print("Da ket noi: " + connectedSSID);
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(10, 50);
        gfx->print("IP: " + WiFi.localIP().toString());
    } else {
        gfx->setTextColor(COLOR_RED);
        gfx->setCursor(10, 35);
        gfx->print("Chua ket noi WiFi");
    }
    
    for (int i = 0; i < NETWORK_MENU_COUNT; i++) {
        int y = 75 + i * 35;
        
        if (i == wifiCursor) {
            gfx->fillRect(10, y, 220, 30, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        gfx->setCursor(20, y + 8);
        gfx->print(networkMenuItems[i]);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("UP/DOWN: Chon | A: OK | B: Quay lai");
}

void wifiDrawNetworkConnect() {
    wifiState = WIFI_NETWORK_CONNECT;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Ket noi WiFi");
    
    if (savedNetworkCount > 0) {
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(10, 40);
        gfx->print("Mang da luu:");
        
        int maxShow = 6;
        int y = 60;
        
        for (int i = 0; i < maxShow && i < savedNetworkCount; i++) {
            int idx = i;
            
            if (savedNetworks[idx].isConnected) {
                gfx->fillRect(5, y - 2, 225, 28, COLOR_GREEN);
                gfx->setTextColor(COLOR_BG);
            } else if (idx == selectedSavedNetwork) {
                gfx->fillRect(5, y - 2, 225, 28, COLOR_BLUE);
                gfx->setTextColor(COLOR_WHITE);
            } else {
                gfx->setTextColor(COLOR_WHITE);
            }
            
            String name = savedNetworks[idx].ssid;
            if (name.length() > 18) name = name.substring(0, 18);
            
            gfx->setCursor(10, y + 4);
            gfx->print(name);
            
            y += 30;
        }
        
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(10, 290);
        gfx->print("UP/DOWN: Chon | A: Ket noi | B: Back");
    } else {
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(30, 80);
        gfx->print("Chua co mang nao");
        gfx->setCursor(30, 100);
        gfx->print("Su dung lenh 'connect' trong CMD");
        gfx->setCursor(10, 290);
        gfx->print("B: Quay lai");
    }
}

void wifiDrawNetworkDevices() {
    wifiState = WIFI_NETWORK_DEVICES;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Thiet bi LAN");
    
    if (!wifiConnected) {
        gfx->setTextColor(COLOR_RED);
        gfx->setCursor(30, 80);
        gfx->print("Chua ket noi WiFi!");
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(10, 290);
        gfx->print("B: Quay lai");
        return;
    }
    
    gfx->setTextColor(COLOR_GREEN);
    gfx->setCursor(10, 40);
    gfx->print("Gateway: " + WiFi.gatewayIP().toString());
    
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(10, 60);
    gfx->print("Tim thay 3 thiet bi:");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 85);
    gfx->print("1. Router (" + WiFi.gatewayIP().toString() + ")");
    gfx->setCursor(10, 110);
    gfx->print("2. " + WiFi.localIP().toString() + " (This)");
    gfx->setCursor(10, 135);
    gfx->print("3. 192.168.1.100");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 170);
    gfx->print("Services:");
    gfx->setCursor(10, 185);
    gfx->print("- Port 80: HTTP");
    gfx->setCursor(10, 200);
    gfx->print("- Port 22: SSH");
    gfx->setCursor(10, 215);
    gfx->print("- Port 443: HTTPS");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawNetworkPorts() {
    wifiState = WIFI_NETWORK_PORTS;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Quet Cong");
    
    if (!wifiConnected) {
        gfx->setTextColor(COLOR_RED);
        gfx->setCursor(30, 80);
        gfx->print("Chua ket noi WiFi!");
        return;
    }
    
    String targetIP = WiFi.gatewayIP().toString();
    
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(10, 40);
    gfx->print("Muc tieu: " + targetIP);
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 60);
    gfx->print("Dang quet cong...");
    
    int commonPorts[] = {21, 22, 23, 25, 53, 80, 110, 143, 443, 445};
    String portNames[] = {"FTP", "SSH", "Telnet", "SMTP", "DNS", "HTTP", "POP3", "IMAP", "HTTPS", "SMB"};
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 85);
    gfx->print("Cong mo:");
    
    for (int i = 0; i < 5; i++) {
        gfx->setTextColor(COLOR_GREEN);
        gfx->setCursor(10, 105 + i * 20);
        gfx->print("Port " + String(commonPorts[i]) + " [" + portNames[i] + "]: OPEN");
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawNetworkSSH() {
    wifiState = WIFI_NETWORK_SSH;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Kiem tra SSH");
    
    if (!wifiConnected) {
        gfx->setTextColor(COLOR_RED);
        gfx->setCursor(30, 80);
        gfx->print("Chua ket noi WiFi!");
        return;
    }
    
    String targetIP = WiFi.gatewayIP().toString();
    
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(10, 40);
    gfx->print("Kiem tra: " + targetIP + ":22");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 65);
    gfx->print("Dang ket noi SSH...");
    
    gfx->setTextColor(COLOR_GREEN);
    gfx->setCursor(10, 95);
    gfx->print("SSH Port (22): OPEN");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 120);
    gfx->print("Service: OpenSSH");
    gfx->setCursor(10, 140);
    gfx->print("Version: Simulated");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawOutputMenu() {
    wifiState = WIFI_OUTPUT_MENU;
    wifiCursor = 0;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Dau Ra");
    
    for (int i = 0; i < OUTPUT_MENU_COUNT; i++) {
        int y = 50 + i * 40;
        
        if (i == wifiCursor) {
            gfx->fillRect(10, y, 220, 35, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        gfx->setCursor(20, y + 12);
        gfx->print(outputMenuItems[i]);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("UP/DOWN: Chon | A: OK | B: Quay lai");
}

void wifiDrawOutputUSB() {
    wifiState = WIFI_OUTPUT_USB;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_GREEN);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(10, 10);
    gfx->print("USB Dongle Mode");
    
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(10, 45);
    gfx->print("Che do USB Dongle cho Wireshark");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 75);
    gfx->print("Ket noi USB voi may tinh");
    gfx->setCursor(10, 95);
    gfx->print("Mo Wireshark de xem luong");
    
    gfx->fillRect(10, 130, 220, 35, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(50, 140);
    gfx->print("KICH HOAT USB");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 180);
    gfx->print("Dang cho ket noi USB...");
    
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawSavedNetworks() {
    wifiState = WIFI_SAVED_NETWORKS;
    wifiScrollOffset = 0;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Mang da luu");
    
    if (savedNetworkCount == 0) {
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(30, 80);
        gfx->print("Chua co mang nao");
        gfx->setCursor(30, 100);
        gfx->print("Ket noi de luu");
        gfx->setCursor(10, 290);
        gfx->print("A: Quet | B: Thoat");
        return;
    }
    
    int maxShow = 8;
    int y = 35;
    
    for (int i = 0; i < maxShow && i < savedNetworkCount; i++) {
        int idx = i;
        
        if (savedNetworks[idx].isConnected) {
            gfx->fillRect(5, y - 2, 225, 28, COLOR_GREEN);
            gfx->setTextColor(COLOR_BG);
        } else if (idx == selectedSavedNetwork) {
            gfx->fillRect(5, y - 2, 225, 28, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        String name = savedNetworks[idx].ssid;
        if (name.length() > 14) name = name.substring(0, 14);
        
        gfx->setCursor(10, y + 4);
        gfx->print(name);
        
        if (savedNetworks[idx].isConnected) {
            gfx->setTextColor(COLOR_BG);
            gfx->setCursor(180, y + 4);
            gfx->print("[OK]");
        }
        
        y += 30;
    }
    
    if (savedNetworkCount > maxShow) {
        int barHeight = maxShow * 30;
        int scrollBarY = 35 + (wifiScrollOffset * barHeight / savedNetworkCount);
        int scrollBarH = maxShow * barHeight / savedNetworkCount;
        if (scrollBarH < 10) scrollBarH = 10;
        gfx->fillRect(233, scrollBarY, 4, scrollBarH, COLOR_GRAY);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("A: Quet | B: Thoat");
}

void wifiDrawSavedOptions() {
    wifiState = WIFI_SAVED_OPTIONS;
    savedOptionCursor = 0;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    String name = savedNetworks[selectedSavedNetwork].ssid;
    if (name.length() > 15) name = name.substring(0, 15);
    gfx->print(name);
    
    const char* options[] = {"Ket noi", "Xoa mang", "Ngat ket noi", "Thuoc tinh"};
    int optionCount = 4;
    
    for (int i = 0; i < optionCount; i++) {
        int y = 45 + i * 40;
        
        if (i == savedOptionCursor) {
            gfx->fillRect(20, y, 200, 35, COLOR_BLUE);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        
        gfx->setCursor(30, y + 10);
        gfx->print(options[i]);
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("UP/DOWN: Chon | A: OK | B: Quay lai");
}

void wifiDrawNetworkInfo() {
    wifiState = WIFI_NETWORK_INFO;
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Thong Tin Mang");
    
    NetworkInfo* net = &savedNetworks[selectedSavedNetwork];
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 40);
    gfx->print("SSID:");
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(80, 40);
    gfx->print(net->ssid);
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 65);
    gfx->print("BSSID:");
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(80, 65);
    gfx->print(net->bssid);
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 90);
    gfx->print("Kenh:");
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(80, 90);
    gfx->print(String(net->channel));
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 115);
    gfx->print("RSSI:");
    uint16_t rssiColor = net->rssi > -60 ? COLOR_GREEN : (net->rssi > -70 ? COLOR_YELLOW : COLOR_RED);
    gfx->setTextColor(rssiColor);
    gfx->setCursor(80, 115);
    gfx->print(String(net->rssi) + " dBm");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 140);
    gfx->print("Security:");
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(80, 140);
    gfx->print(wifiGetAuthType(net->encryption));
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 165);
    gfx->print("PMF:");
    bool pmfRequired = wifiIsPMFRequired(net->encryption);
    gfx->setTextColor(pmfRequired ? COLOR_GREEN : COLOR_GRAY);
    gfx->setCursor(80, 165);
    gfx->print(pmfRequired ? "Required" : "Not Required");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 190);
    gfx->print("Trang thai:");
    gfx->setTextColor(net->isConnected ? COLOR_GREEN : COLOR_GRAY);
    gfx->setCursor(80, 190);
    gfx->print(net->isConnected ? "Da ket noi" : "Chua ket noi");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawConnecting() {
    wifiState = WIFI_CONNECTING;
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Dang ket noi...");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 60);
    gfx->print("SSID: ");
    gfx->setTextColor(COLOR_YELLOW);
    gfx->print(inputSSID);
    
    gfx->setTextColor(COLOR_YELLOW);
    gfx->setCursor(10, 100);
    gfx->print("Dang xu ly...");
    
    int barWidth = 200;
    int progress = (connectingProgress * barWidth) / 100;
    gfx->drawRect(20, 130, barWidth, 20, COLOR_WHITE);
    gfx->fillRect(22, 132, progress, 16, COLOR_GREEN);
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(100, 155);
    gfx->print(connectingProgress);
    gfx->print("%");
    
    if (WiFi.status() == WL_CONNECTED) {
        gfx->setTextColor(COLOR_GREEN);
        gfx->setCursor(30, 190);
        gfx->print("Ket noi thanh cong!");
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 220);
        gfx->print("IP: " + WiFi.localIP().toString());
    } else if (connectingProgress >= 100) {
        gfx->setTextColor(COLOR_RED);
        gfx->setCursor(30, 190);
        gfx->print("Ket noi that bai!");
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("Dang cho...");
}

void wifiDrawStatus() {
    wifiState = WIFI_STATUS;
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Trang thai WiFi");
    
    if (wifiConnected) {
        gfx->setTextColor(COLOR_GREEN);
        gfx->setCursor(10, 40);
        gfx->print("DA KET NOI");
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 65);
        gfx->print("SSID: ");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->print(WiFi.SSID());
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 90);
        gfx->print("IP: ");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->print(WiFi.localIP().toString());
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 115);
        gfx->print("RSSI: ");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->print(String(WiFi.RSSI()) + " dBm");
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 140);
        gfx->print("MAC: ");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->print(WiFi.macAddress());
    } else {
        gfx->setTextColor(COLOR_RED);
        gfx->setCursor(10, 60);
        gfx->print("CHUA KET NOI");
    }
    
    gfx->fillRect(10, 180, 100, 30, COLOR_RED);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(35, 190);
    gfx->print("NGAT");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("A: Ngat ket noi | B: Quay lai");
}

void wifiDrawAttributes() {
    wifiState = WIFI_ATTRIBUTES;
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Thuoc tinh WiFi");
    
    if (wifiConnected) {
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 40);
        gfx->print("MAC Address:");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(120, 40);
        gfx->print(WiFi.macAddress());
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 65);
        gfx->print("IP Address:");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(120, 65);
        gfx->print(WiFi.localIP().toString());
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 90);
        gfx->print("Subnet Mask:");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(120, 90);
        gfx->print(WiFi.subnetMask().toString());
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 115);
        gfx->print("Gateway:");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(120, 115);
        gfx->print(WiFi.gatewayIP().toString());
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 140);
        gfx->print("DNS Server:");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(120, 140);
        gfx->print(WiFi.dnsIP().toString());
    } else {
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(30, 80);
        gfx->print("Chua ket noi");
        gfx->setCursor(30, 110);
        gfx->print("Ket noi truoc de xem");
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("B: Quay lai");
}

void wifiDrawAPMode() {
    wifiState = WIFI_AP_MODE;
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 28, apModeActive ? COLOR_GREEN : COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->print("Access Point");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 50);
    gfx->print("Che do AP:");
    gfx->setTextColor(apModeActive ? COLOR_GREEN : COLOR_RED);
    gfx->setCursor(120, 50);
    gfx->print(apModeActive ? "HOAT DONG" : "DUNG");
    
    if (apModeActive) {
        gfx->setTextColor(COLOR_YELLOW);
        gfx->setCursor(10, 90);
        gfx->print("SSID: ESP32-AP");
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 120);
        gfx->print("IP: 192.168.4.1");
        
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(10, 150);
        gfx->print("Ket noi: ");
        gfx->setTextColor(COLOR_YELLOW);
        gfx->print(WiFi.softAPgetStationNum());
        gfx->print(" thiet bi");
    } else {
        gfx->setTextColor(COLOR_GRAY);
        gfx->setCursor(30, 90);
        gfx->print("Khoi dong AP de tao");
        gfx->setCursor(30, 110);
        gfx->print("diem phat song ao");
    }
    
    gfx->fillRect(10, 200, 100, 30, apModeActive ? COLOR_RED : COLOR_GREEN);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(apModeActive ? 35 : 20, 210);
    gfx->print(apModeActive ? "DUNG AP" : "BAT AP");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("A: Bat/Tat | B: Quay lai");
}

const char* wifiKeyboardChars = "1234567890-_QWERTYUIOPASDFGHJKLZXCVBNM!@#$%^&*()+=.";

const int wifiRowStartX[5] = {10, 20, 20, 20, 10};
const int wifiRowLens[5] = {12, 10, 9, 8, 11};

void wifiKeyboardDraw(bool resetCursor) {
    if (resetCursor) {
        kbdCursor = 0;
        lastKbdCursor = -1;
    }
    
    wifiState = WIFI_KEYBOARD;
    
    extern String wifiKbdBuffer;
    if (resetCursor) {
        wifiKbdBuffer = inputBuffer;
    }
    
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 18, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 5);
    gfx->print("Nhap Mat Khau");
    
    gfx->fillRect(4, 22, SCREEN_WIDTH - 8, 18, 0x0841);
    gfx->drawRect(4, 22, SCREEN_WIDTH - 8, 18, COLOR_WHITE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(8, 27);
    gfx->print(inputBuffer);
    
    int startY = 44;
    int keyW = 18;
    int keyH = 20;
    int gapX = 2;
    int gapY = 2;
    
    int keyIndex = 0;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < wifiRowLens[row]; col++) {
            int x = wifiRowStartX[row] + col * (keyW + gapX);
            int y = startY + row * (keyH + gapY);
            
            uint16_t keyColor = (keyIndex == kbdCursor) ? COLOR_YELLOW : COLOR_GRAY;
            uint16_t textColor = (keyIndex == kbdCursor) ? COLOR_BG : COLOR_WHITE;
            
            gfx->fillRect(x, y, keyW, keyH, keyColor);
            gfx->setTextColor(textColor);
            gfx->setCursor(x + 4, y + 4);
            if (keyIndex < 50) {
                gfx->print(wifiKeyboardChars[keyIndex]);
            }
            keyIndex++;
        }
    }
    
    int btnY = startY + 5 * (keyH + gapY) + 4;
    int btnH = 26;
    
    if (kbdCursor >= 50) {
        gfx->fillRect(4, btnY, 55, btnH, COLOR_WHITE);
        gfx->setTextColor(COLOR_RED);
    } else {
        gfx->fillRect(4, btnY, 55, btnH, COLOR_RED);
        gfx->setTextColor(COLOR_WHITE);
    }
    gfx->setCursor(18, btnY + 8);
    gfx->print("DEL");
    
    if (kbdCursor == 51) {
        gfx->fillRect(62, btnY, 55, btnH, COLOR_WHITE);
        gfx->setTextColor(COLOR_BLUE);
    } else {
        gfx->fillRect(62, btnY, 55, btnH, COLOR_BLUE);
        gfx->setTextColor(COLOR_WHITE);
    }
    gfx->setCursor(70, btnY + 8);
    gfx->print("SPC");
    
    if (kbdCursor == 52) {
        gfx->fillRect(120, btnY, 55, btnH, COLOR_WHITE);
        gfx->setTextColor(COLOR_GREEN);
    } else {
        gfx->fillRect(120, btnY, 55, btnH, COLOR_GREEN);
        gfx->setTextColor(COLOR_WHITE);
    }
    gfx->setCursor(132, btnY + 8);
    gfx->print("OK");
    
    if (kbdCursor == 53) {
        gfx->fillRect(178, btnY, 58, btnH, COLOR_WHITE);
        gfx->setTextColor(COLOR_GRAY);
    } else {
        gfx->fillRect(178, btnY, 58, btnH, COLOR_GRAY);
        gfx->setTextColor(COLOR_WHITE);
    }
    gfx->setCursor(192, btnY + 8);
    gfx->print("CLR");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(5, 290);
    gfx->print("Dir: Chon | A: Enter | B: Cancel");
}

void wifiKeyboardHandle() {
    int oldCursor = kbdCursor;
    bool moved = false;
    
    if (buttonPressed(KEY_UP)) {
        if (kbdCursor < 10) {
            kbdCursor = 50;
        } else if (kbdCursor >= 50) {
            kbdCursor = 49;
        } else {
            int row = kbdCursor / 10;
            int col = kbdCursor % 10;
            row--;
            if (row < 0) row = 0;
            if (col >= wifiRowLens[row]) col = wifiRowLens[row] - 1;
            kbdCursor = row * 10 + col;
        }
        moved = true;
    } else if (buttonPressed(KEY_DOWN)) {
        if (kbdCursor >= 50) {
            kbdCursor = 0;
        } else {
            int row = kbdCursor / 10;
            int col = kbdCursor % 10;
            row++;
            if (row > 4) row = 4;
            if (col >= wifiRowLens[row]) col = wifiRowLens[row] - 1;
            kbdCursor = row * 10 + col;
        }
        moved = true;
    } else if (buttonPressed(KEY_LEFT)) {
        if (kbdCursor >= 50) {
            kbdCursor = 50;
        } else {
            int col = kbdCursor % 10;
            if (col > 0) {
                kbdCursor--;
            } else {
                int row = kbdCursor / 10;
                kbdCursor = row * 10 + wifiRowLens[row] - 1;
            }
        }
        moved = true;
    } else if (buttonPressed(KEY_RIGHT)) {
        if (kbdCursor >= 50) {
            kbdCursor = 53;
        } else {
            int row = kbdCursor / 10;
            int col = kbdCursor % 10;
            if (col < wifiRowLens[row] - 1) {
                kbdCursor++;
            } else {
                kbdCursor = row * 10;
            }
        }
        moved = true;
    }
    
    if (moved) {
        wifiKeyboardDraw(false);
    }
    
    if (buttonPressed(KEY_START)) {
        if (kbdCursor >= 50) {
            if (kbdCursor == 50) {
                int len = strlen(inputBuffer);
                if (len > 0) inputBuffer[len - 1] = '\0';
            } else if (kbdCursor == 51) {
                int len = strlen(inputBuffer);
                if (len < 63) {
                    inputBuffer[len] = ' ';
                    inputBuffer[len + 1] = '\0';
                }
            } else if (kbdCursor == 52) {
                wifiConnectToNetwork(inputSSID, inputBuffer, false, "", "", "", "");
                memset(inputBuffer, 0, sizeof(inputBuffer));
                return;
            } else if (kbdCursor == 53) {
                memset(inputBuffer, 0, sizeof(inputBuffer));
            }
        } else {
            char ch = wifiKeyboardChars[kbdCursor];
            int len = strlen(inputBuffer);
            if (len < 63) {
                inputBuffer[len] = ch;
                inputBuffer[len + 1] = '\0';
            }
        }
        wifiKeyboardDraw(false);
    }
    
    if (buttonPressed(KEY_B)) {
        memset(inputBuffer, 0, sizeof(inputBuffer));
        wifiDrawNetworkConnect();
    }
}

void wifiInputHandler() {
    if (wifiMode == WIFI_MODE_ORIGINAL) {
        switch (wifiState) {
            case WIFI_MAIN:
                if (buttonPressed(KEY_UP)) {
                    wifiCursor = (wifiCursor + ORIG_MENU_COUNT - 1) % ORIG_MENU_COUNT;
                    wifiDrawMain();
                }
                if (buttonPressed(KEY_DOWN)) {
                    wifiCursor = (wifiCursor + 1) % ORIG_MENU_COUNT;
                    wifiDrawMain();
                }
                if (buttonPressed(KEY_START)) {
                    switch (wifiCursor) {
                        case 0:
                            wifiScanNetworks();
                            break;
                        case 1:
                            wifiDrawNetworkConnect();
                            break;
                        case 2:
                            selectedSavedNetwork = 0;
                            wifiDrawSavedNetworks();
                            break;
                        case 3:
                            wifiDisconnect();
                            break;
                        case 4:
                            wifiDrawHelp();
                            break;
                        case 5:
                            wifiMode = WIFI_MODE_SELECT;
                            wifiDrawModeSelect();
                            break;
                    }
                }
                if (buttonPressed(KEY_A)) {
                    wifiMode = WIFI_MODE_SELECT;
                    wifiDrawModeSelect();
                }
                break;
        }
        return;
    }
    
    switch (wifiState) {
        case WIFI_MAIN:
            if (buttonPressed(KEY_UP)) {
                wifiCursor = (wifiCursor + MAIN_MENU_COUNT - 1) % MAIN_MENU_COUNT;
                wifiDrawMain();
            }
            if (buttonPressed(KEY_DOWN)) {
                wifiCursor = (wifiCursor + 1) % MAIN_MENU_COUNT;
                wifiDrawMain();
            }
            if (buttonPressed(KEY_START)) {
                switch (wifiCursor) {
                    case 0:
                        wifiCursor = 0;
                        wifiDrawScanMenu();
                        break;
                    case 1:
                        selectedSavedNetwork = 0;
                        wifiDrawSavedNetworks();
                        break;
                    case 2:
                        wifiDrawAttributes();
                        break;
                    case 3:
                        wifiDrawAPMode();
                        break;
                    case 4:
                        wifiMode = WIFI_MODE_SELECT;
                        wifiDrawModeSelect();
                        break;
                }
            }
            if (buttonPressed(KEY_A)) {
                wifiMode = WIFI_MODE_SELECT;
                wifiDrawModeSelect();
            }
            break;
            
        case WIFI_SCAN_MENU:
            if (buttonPressed(KEY_UP)) {
                wifiCursor = (wifiCursor + SCAN_MENU_COUNT - 1) % SCAN_MENU_COUNT;
                wifiDrawScanMenu();
            }
            if (buttonPressed(KEY_DOWN)) {
                wifiCursor = (wifiCursor + 1) % SCAN_MENU_COUNT;
                wifiDrawScanMenu();
            }
            if (buttonPressed(KEY_START)) {
                switch (wifiCursor) {
                    case 0:
                        wifiDrawScanAP();
                        break;
                    case 1:
                        wifiStartLiveScan();
                        break;
                    case 2:
                        wifiScanStations();
                        wifiDrawScanStations();
                        break;
                    case 3:
                        wifiDrawChannelStatus();
                        break;
                    case 4:
                        wifiStartSweep();
                        break;
                    case 5:
                        wifiCursor = 0;
                        wifiDrawMain();
                        break;
                }
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawMain();
            }
            break;
            
        case WIFI_SCAN_AP:
            if (buttonPressed(KEY_UP)) {
                if (selectedAP > 0) selectedAP--;
                wifiDrawScanAP();
            }
            if (buttonPressed(KEY_DOWN)) {
                if (selectedAP < scanResultCount - 1) selectedAP++;
                wifiDrawScanAP();
            }
            if (buttonPressed(KEY_START)) {
                wifiScanNetworks();
                wifiDrawScanAP();
            }
            if (buttonPressed(KEY_MENU)) {
                wifiDrawTargetMenu();
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawScanMenu();
            }
            break;
            
        case WIFI_SCAN_STATIONS:
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawScanMenu();
            }
            break;
            
        case WIFI_CHANNEL_STATUS:
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawScanMenu();
            }
            break;
            
        case WIFI_TARGET_MENU:
            if (buttonPressed(KEY_UP)) {
                wifiCursor = (wifiCursor + TARGET_MENU_COUNT - 1) % TARGET_MENU_COUNT;
                wifiDrawTargetMenu();
            }
            if (buttonPressed(KEY_DOWN)) {
                wifiCursor = (wifiCursor + 1) % TARGET_MENU_COUNT;
                wifiDrawTargetMenu();
            }
            if (buttonPressed(KEY_START)) {
                switch (wifiCursor) {
                    case 0:
                        wifiDrawTargetSelect();
                        break;
                    case 1:
                        wifiDrawTargetList();
                        break;
                    case 2:
                        targetedCount = 0;
                        wifiDrawTargetMenu();
                        break;
                    case 3:
                        for (int i = 0; i < scanResultCount; i++) {
                            scanResults[i].isTargeted = false;
                        }
                        targetedCount = 0;
                        wifiDrawTargetMenu();
                        break;
                    case 4:
                        wifiCursor = 0;
                        wifiDrawScanMenu();
                        break;
                }
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawScanMenu();
            }
            break;
            
        case WIFI_TARGET_SELECT:
            if (buttonPressed(KEY_UP)) {
                if (selectedAP > 0) selectedAP--;
                wifiDrawTargetSelect();
            }
            if (buttonPressed(KEY_DOWN)) {
                if (selectedAP < scanResultCount - 1) selectedAP++;
                wifiDrawTargetSelect();
            }
            if (buttonPressed(KEY_START)) {
                scanResults[selectedAP].isTargeted = !scanResults[selectedAP].isTargeted;
                
                if (scanResults[selectedAP].isTargeted) {
                    if (targetedCount < MAX_TARGETS) {
                        targetIndexes[targetedCount] = selectedAP;
                        targetedCount++;
                    }
                } else {
                    for (int i = 0; i < targetedCount; i++) {
                        if (targetIndexes[i] == selectedAP) {
                            for (int j = i; j < targetedCount - 1; j++) {
                                targetIndexes[j] = targetIndexes[j + 1];
                            }
                            targetedCount--;
                            break;
                        }
                    }
                }
                wifiDrawTargetSelect();
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawTargetMenu();
            }
            break;
            
        case WIFI_TARGET_LIST:
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawTargetMenu();
            }
            break;
            
        case WIFI_OFFENSE_MENU:
            if (buttonPressed(KEY_UP)) {
                wifiCursor = (wifiCursor + OFFENSE_MENU_COUNT - 1) % OFFENSE_MENU_COUNT;
                wifiDrawOffenseMenu();
            }
            if (buttonPressed(KEY_DOWN)) {
                wifiCursor = (wifiCursor + 1) % OFFENSE_MENU_COUNT;
                wifiDrawOffenseMenu();
            }
            if (buttonPressed(KEY_START)) {
                switch (wifiCursor) {
                    case 0:
                        if (targetedCount > 0) {
                            wifiStartDeauth();
                        }
                        break;
                    case 1:
                        wifiStopDeauth();
                        wifiDrawOffenseMenu();
                        break;
                    case 2:
                        wifiCursor = 0;
                        wifiDrawMain();
                        break;
                }
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawMain();
            }
            break;
            
        case WIFI_OFFENSE_DEAUTH:
            if (buttonPressed(KEY_A)) {
                wifiStopDeauth();
            }
            break;
            
        case WIFI_NETWORK_MENU:
            if (buttonPressed(KEY_UP)) {
                wifiCursor = (wifiCursor + NETWORK_MENU_COUNT - 1) % NETWORK_MENU_COUNT;
                wifiDrawNetworkMenu();
            }
            if (buttonPressed(KEY_DOWN)) {
                wifiCursor = (wifiCursor + 1) % NETWORK_MENU_COUNT;
                wifiDrawNetworkMenu();
            }
            if (buttonPressed(KEY_START)) {
                switch (wifiCursor) {
                    case 0:
                        selectedSavedNetwork = 0;
                        wifiDrawNetworkConnect();
                        break;
                    case 1:
                        wifiDisconnect();
                        wifiDrawNetworkMenu();
                        break;
                    case 2:
                        wifiDrawNetworkDevices();
                        break;
                    case 3:
                        wifiDrawNetworkPorts();
                        break;
                    case 4:
                        wifiDrawNetworkSSH();
                        break;
                    case 5:
                        wifiCursor = 0;
                        wifiDrawMain();
                        break;
                }
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawMain();
            }
            break;
            
        case WIFI_NETWORK_CONNECT:
            if (buttonPressed(KEY_UP)) {
                if (selectedSavedNetwork > 0) selectedSavedNetwork--;
                wifiDrawNetworkConnect();
            }
            if (buttonPressed(KEY_DOWN)) {
                if (selectedSavedNetwork < savedNetworkCount - 1) selectedSavedNetwork++;
                wifiDrawNetworkConnect();
            }
            if (buttonPressed(KEY_START)) {
                wifiConnectToSaved(selectedSavedNetwork);
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawNetworkMenu();
            }
            break;
            
        case WIFI_NETWORK_DEVICES:
        case WIFI_NETWORK_PORTS:
        case WIFI_NETWORK_SSH:
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawNetworkMenu();
            }
            break;
            
        case WIFI_OUTPUT_MENU:
            if (buttonPressed(KEY_UP)) {
                wifiCursor = (wifiCursor + OUTPUT_MENU_COUNT - 1) % OUTPUT_MENU_COUNT;
                wifiDrawOutputMenu();
            }
            if (buttonPressed(KEY_DOWN)) {
                wifiCursor = (wifiCursor + 1) % OUTPUT_MENU_COUNT;
                wifiDrawOutputMenu();
            }
            if (buttonPressed(KEY_START)) {
                switch (wifiCursor) {
                    case 0:
                        wifiDrawOutputUSB();
                        break;
                    case 1:
                        wifiCursor = 0;
                        wifiDrawMain();
                        break;
                    case 2:
                        wifiCursor = 0;
                        wifiDrawMain();
                        break;
                }
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawMain();
            }
            break;
            
        case WIFI_OUTPUT_USB:
            if (buttonPressed(KEY_START)) {
                wifiDrawOutputUSB();
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawOutputMenu();
            }
            break;
            
        case WIFI_SAVED_NETWORKS:
            if (buttonPressed(KEY_UP)) {
                if (selectedSavedNetwork > 0) selectedSavedNetwork--;
                wifiDrawSavedNetworks();
            }
            if (buttonPressed(KEY_DOWN)) {
                if (selectedSavedNetwork < savedNetworkCount - 1) selectedSavedNetwork++;
                wifiDrawSavedNetworks();
            }
            if (buttonPressed(KEY_START)) {
                wifiScanNetworks();
                wifiDrawScanAP();
            }
            if (buttonPressed(KEY_MENU)) {
                if (savedNetworkCount > 0) {
                    savedOptionCursor = 0;
                    wifiDrawSavedOptions();
                }
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawMain();
            }
            break;
            
        case WIFI_SAVED_OPTIONS:
            if (buttonPressed(KEY_UP)) {
                if (savedOptionCursor > 0) savedOptionCursor--;
                else savedOptionCursor = 3;
                wifiDrawSavedOptions();
            }
            if (buttonPressed(KEY_DOWN)) {
                if (savedOptionCursor < 3) savedOptionCursor++;
                else savedOptionCursor = 0;
                wifiDrawSavedOptions();
            }
            if (buttonPressed(KEY_START)) {
                if (savedOptionCursor == 0) {
                    wifiConnectToSaved(selectedSavedNetwork);
                } else if (savedOptionCursor == 1) {
                    wifiDeleteNetwork(selectedSavedNetwork);
                    if (selectedSavedNetwork >= savedNetworkCount && selectedSavedNetwork > 0) {
                        selectedSavedNetwork--;
                    }
                    wifiDrawSavedNetworks();
                } else if (savedOptionCursor == 2) {
                    if (savedNetworks[selectedSavedNetwork].isConnected) {
                        wifiDisconnect();
                    }
                    wifiDrawSavedOptions();
                } else if (savedOptionCursor == 3) {
                    wifiDrawNetworkInfo();
                }
            }
            if (buttonPressed(KEY_A)) {
                wifiDrawSavedNetworks();
            }
            break;
            
        case WIFI_NETWORK_INFO:
            if (buttonPressed(KEY_A)) {
                wifiDrawSavedOptions();
            }
            break;
            
        case WIFI_STATUS:
            if (buttonPressed(KEY_START)) {
                wifiDisconnect();
                wifiDrawStatus();
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawMain();
            }
            break;
            
        case WIFI_ATTRIBUTES:
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawMain();
            }
            break;
            
        case WIFI_AP_MODE:
            if (buttonPressed(KEY_START)) {
                if (apModeActive) {
                    wifiStopAP();
                } else {
                    wifiStartAP();
                }
                wifiDrawAPMode();
            }
            if (buttonPressed(KEY_A)) {
                wifiCursor = 0;
                wifiDrawMain();
            }
            break;
            
        case WIFI_KEYBOARD:
            wifiKeyboardHandle();
            break;
            
        case WIFI_CONNECTING:
            delay(100);
            if (WiFi.status() == WL_CONNECTED) {
                wifiState = WIFI_STATUS;
                wifiDrawStatus();
            } else if (connectingProgress >= 100) {
                wifiState = WIFI_MAIN;
                wifiDrawMain();
            }
            break;
    }
}

void appWiFiMenu() {
    wifiAppInit();
    wifiCursor = 0;
    wifiModeCursor = 0;
    wifiScrollOffset = 0;
    targetedCount = 0;
    deauthRunning = false;
    wifiMode = WIFI_MODE_SELECT;
    wifiDrawModeSelect();
    
    while (true) {
        if (wifiMode == WIFI_MODE_SELECT) {
            wifiModeSelectHandle();
        } else {
            wifiInputHandler();
        }
        delay(50);
        if (systemState == SYS_MAIN) {
            break;
        }
    }
}

void wifiDrawModeSelect() {
    gfx->fillScreen(COLOR_BG);
    
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(50, 6);
    gfx->print("WiFi");
    
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 35);
    gfx->print("Chon che do WiFi:");
    
    const char* modes[] = {"Mac dinh", "Nang cao"};
    const char* desc[] = {"Ket noi WiFi co ban", "Quet, Tan cong, Mang..."};
    
    for (int i = 0; i < WIFI_MODE_COUNT; i++) {
        int y = 60 + i * 70;
        
        if (i == wifiModeCursor) {
            gfx->fillRect(5, y, SCREEN_WIDTH - 10, 60, COLOR_GRAY);
            gfx->drawRect(5, y, SCREEN_WIDTH - 10, 60, COLOR_WHITE);
        } else {
            gfx->fillRect(5, y, SCREEN_WIDTH - 10, 60, COLOR_BG);
            gfx->drawRect(5, y, SCREEN_WIDTH - 10, 60, COLOR_GRAY);
        }
        
        gfx->setTextColor(i == wifiModeCursor ? COLOR_BG : COLOR_WHITE);
        gfx->setCursor(15, y + 5);
        gfx->print(modes[i]);
        
        gfx->setTextColor(i == wifiModeCursor ? COLOR_YELLOW : COLOR_GRAY);
        gfx->setCursor(15, y + 25);
        gfx->print(desc[i]);
        
        if (i == wifiModeCursor) {
            gfx->setTextColor(COLOR_GREEN);
            gfx->setCursor(SCREEN_WIDTH - 25, y + 25);
            gfx->print(">");
        }
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(5, 250);
    gfx->print("UP/DOWN: Chon | A: Xac nhan");
    gfx->setCursor(5, 270);
    gfx->print("B: Quay lai");
}

void wifiModeSelectHandle() {
    if (buttonPressed(KEY_UP)) {
        wifiModeCursor = (wifiModeCursor + WIFI_MODE_COUNT - 1) % WIFI_MODE_COUNT;
        wifiDrawModeSelect();
    }
    
    if (buttonPressed(KEY_DOWN)) {
        wifiModeCursor = (wifiModeCursor + 1) % WIFI_MODE_COUNT;
        wifiDrawModeSelect();
    }
    
    if (buttonPressed(KEY_START)) {
        if (wifiModeCursor == 0) {
            wifiMode = WIFI_MODE_ORIGINAL;
        } else {
            wifiMode = WIFI_MODE_ADVANCED;
        }
        wifiState = WIFI_MAIN;
        wifiCursor = 0;
        wifiDrawMain();
    }
    
    if (buttonPressed(KEY_A)) {
        systemState = SYS_MAIN;
        drawMainUI();
    }
}
