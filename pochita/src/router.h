#ifndef ROUTER_H
#define ROUTER_H

#include <WiFi.h>
#include "component/Display.h"
#include <Preferences.h>
#include <FS.h>
#include <algorithm>
#include "component/Keyboard.h"
#include "component/ui_utils.h"
#include "wifi_sense.h"

extern TFT_eSPI tft;
extern void drawLauncherContent();
void drawRouterApp();
void drawRouterContext(int sel); // Forward declaration

// Router State
enum RouterState { 
    ROUTER_MAIN, 
    ROUTER_SCANNING, 
    ROUTER_LIST, 
    ROUTER_DETAIL,   // Detailed scan list (BSSID/channel/encryption/hidden)
    ROUTER_CONTEXT, 
    ROUTER_PROPS, 
    ROUTER_ADVANCED,
    ROUTER_SCANNER,
    ROUTER_ANALYZER,
    ROUTER_SENSE,   // RuView-style RSSI heatmap
    ROUTER_ESPECTRE, // ESPectre CSI motion detector
    ROUTER_FALL,    // Fall detection (CSI + RSSI) alert
    ROUTER_DIAGNOSE, // AI connection diagnosis
    ROUTER_SAVED,  // Saved Networks List
    ROUTER_PASSWORD_INPUT  // New state for password input
};
RouterState routerState = ROUTER_MAIN;

#include "espectre.h"
#include "fall_alert.h"

// Display order for the detailed scan: maps a visible row -> scan index, sorted
// by signal strength so the strongest access points appear first.
std::vector<int> wifiOrder;

int routerSel = 0;
int routerScroll = 0;
int wifiCount = 0;
String routerPass = "";
String targetSSID = "";
int targetWifiIdx = -1; // Index in scan list
int targetWifiEncryption = 0; // Encryption type of target WiFi
int routerCtxSel = 0; // Current context menu selection
// Where the context menu returns to when closed (ROUTER_LIST or ROUTER_DETAIL).
RouterState routerCtxReturn = ROUTER_LIST;

// Helper function: Get encryption name
String getEncryptionName(int type) {
    switch(type) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "Unknown";
    }
}

// Helper function: Get signal strength
String getSignalStrength(int rssi) {
    if (rssi > -50) return "Excellent";
    if (rssi > -60) return "Very Good";
    if (rssi > -70) return "Good";
    if (rssi > -80) return "Fair";
    return "Weak";
}

// Helper function: Get signal color
uint16_t getSignalColor(int rssi) {
    if (rssi > -50) return 0x00FF00;  // Green
    if (rssi > -60) return 0x7FE0;  // Yellow-Green, RGB565
    if (rssi > -70) return TFT_YELLOW;
    if (rssi > -80) return TFT_ORANGE;
    return TFT_RED;
}

// Helper function: Get signal bars (1-4)
int getSignalBars(int rssi) {
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    return 1;
}

struct WiFiCred {
    String ssid;
    String pass;
};
std::vector<WiFiCred> savedNetworks;

// AI connection diagnosis (ROUTER_DIAGNOSE) - uses the loaded AI model to
// summarise the current link state and suggest fixes.
String aiDiagText = "";
static void aiDiagSink(const char* text, void* ctx) {
  (void)ctx;
  if (text && *text) aiDiagText += text;
}
// Defined in settings.h (same TU): resolves the configured AI model to an SD path.
String aiSettingsModelPath();
void aiDiagRun() {
  aiDiagText = "";
  if (!aiModelLoaded()) {
    // Smart automation: try to auto-load the model selected in Settings.
    String p = aiSettingsModelPath();
    if (p.length() > 0) {
      char err[96];
      if (aiModelLoad(p, err, sizeof(err)) == 0) {
        aiDiagText = "Auto-loaded " + p + "\n";
      } else {
        aiDiagText = String("Load failed (") + p + "): " + err + "\n";
      }
    }
    if (!aiModelLoaded()) {
      aiDiagText += "No AI model loaded.\nLoad one in Terminal: ai load <path>\nor via AI Chat app.";
      return;
    }
  }
  char buf[512];
  int n = 0;
  n += snprintf(buf + n, sizeof(buf) - n, "WiFi diagnostic summary. "
      "Current status:\n");
  n += snprintf(buf + n, sizeof(buf) - n, "- connected: %s\n",
      WiFi.status() == WL_CONNECTED ? "yes" : "no");
  n += snprintf(buf + n, sizeof(buf) - n, "- ssid: %s\n",
      WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "-");
  n += snprintf(buf + n, sizeof(buf) - n, "- rssi: %d dBm\n", WiFi.RSSI());
  n += snprintf(buf + n, sizeof(buf) - n, "- ip: %s\n",
      WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-");
  n += snprintf(buf + n, sizeof(buf) - n, "- channel: %d\n", WiFi.channel());
  n += snprintf(buf + n, sizeof(buf) - n, "- saved networks: %d\n",
                (int)savedNetworks.size());
  n += snprintf(buf + n, sizeof(buf) - n,
      "- motion sensors: csi=%s rssi_var=%d%%\n"
      "Give a short 3-line diagnosis in Vietnamese and one concrete fix.",
      espRssiMode ? "off" : "on", sMotionLevel);
  if (n < 0) n = 0;
  aiGenerate(buf, 120, aiDiagSink, nullptr);
}

constexpr size_t MAX_SAVED_WIFI_NETWORKS = 16;

bool addOrUpdateSavedNetwork(const String &ssid, const String &pass) {
    if (ssid.isEmpty()) return false;
    for (auto &cred : savedNetworks) {
        if (cred.ssid == ssid) {
            if (cred.pass != pass) {
                cred.pass = pass;
                return true;
            }
            return false;
        }
    }
    if (savedNetworks.size() >= MAX_SAVED_WIFI_NETWORKS) {
        savedNetworks.erase(savedNetworks.begin());
    }
    savedNetworks.push_back({ssid, pass});
    return true;
}

void saveSavedNetworks();

void loadSavedNetworks() {
    savedNetworks.clear();

    // NVS is the primary store, so credentials survive reboot even without SD.
    Preferences prefs;
    // Open read/write so the namespace is created cleanly on first boot.
    prefs.begin("wifi_saved", false);
    size_t count = min((size_t)prefs.getUChar("count", 0),
                       MAX_SAVED_WIFI_NETWORKS);
    for (size_t i = 0; i < count; ++i) {
        String ssid = prefs.getString(("ssid" + String(i)).c_str(), "");
        String pass = prefs.getString(("pass" + String(i)).c_str(), "");
        addOrUpdateSavedNetwork(ssid, pass);
    }
    prefs.end();

    // Import credentials written by older POCHITA OS versions on the SD card.
    if (!SD.exists("/system/wifi.json")) return;
    
    File f = SD.open("/system/wifi.json", FILE_READ);
    if (!f) return;
    
    String json = f.readString();
    f.close();
    
    // Very basic manual parsing for [{"ssid":"A","pass":"B"}]
    int start = 0;
    while (true) {
        int objStart = json.indexOf('{', start);
        if (objStart == -1) break;
        int objEnd = json.indexOf('}', objStart);
        if (objEnd == -1) break;
        
        String obj = json.substring(objStart, objEnd + 1);
        
        // Extract SSID
        int sKey = obj.indexOf("\"ssid\":\"");
        if (sKey != -1) {
            int sEnd = obj.indexOf("\"", sKey + 8);
            String ssid = obj.substring(sKey + 8, sEnd);
            
            // Extract Pass
            String pass = "";
            int pKey = obj.indexOf("\"pass\":\"");
            if (pKey != -1) {
                int pEnd = obj.indexOf("\"", pKey + 8);
                pass = obj.substring(pKey + 8, pEnd);
            }
            
            addOrUpdateSavedNetwork(ssid, pass);
        }
        start = objEnd + 1;
    }

    // Persist any legacy SD entries into NVS for subsequent SD-independent boot.
    prefs.begin("wifi_saved", false);
    prefs.clear();
    prefs.putUChar("count", (uint8_t)savedNetworks.size());
    for (size_t i = 0; i < savedNetworks.size(); ++i) {
        prefs.putString(("ssid" + String(i)).c_str(), savedNetworks[i].ssid);
        prefs.putString(("pass" + String(i)).c_str(), savedNetworks[i].pass);
    }
    prefs.end();
}

void saveSavedNetworks() {
    Preferences prefs;
    prefs.begin("wifi_saved", false);
    prefs.clear();
    prefs.putUChar("count", (uint8_t)min(savedNetworks.size(),
                                               MAX_SAVED_WIFI_NETWORKS));
    for (size_t i = 0; i < savedNetworks.size() &&
                       i < MAX_SAVED_WIFI_NETWORKS; ++i) {
        prefs.putString(("ssid" + String(i)).c_str(), savedNetworks[i].ssid);
        prefs.putString(("pass" + String(i)).c_str(), savedNetworks[i].pass);
    }
    prefs.end();

    String json = "[";
    for (size_t i = 0; i < savedNetworks.size(); i++) {
        json += "{\"ssid\":\"" + savedNetworks[i].ssid + "\",\"pass\":\"" + savedNetworks[i].pass + "\"}";
        if (i < savedNetworks.size() - 1) json += ",";
    }
    json += "]";
    
    // Ensure dir exists
    if (!SD.exists("/system")) SD.mkdir("/system");
    
    File f = SD.open("/system/wifi.json", FILE_WRITE);
    if (f) {
        f.print(json);
        f.close();
    }
}

struct AutoWiFiCandidate {
    int credentialIndex;
    int rssi;
};

// Scan all visible access points and connect to the strongest saved network.
// If its password is stale or it cannot associate, try the next strongest one.
bool autoConnectSavedWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    loadSavedNetworks();
    if (savedNetworks.empty()) {
        // Compatibility with firmware that only used Arduino-ESP32's single
        // persisted station credential before POCHITA gained its own list.
        Serial.println("WiFi auto-connect: trying legacy ESP32 credential");
        WiFi.begin();
        unsigned long legacyStarted = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - legacyStarted < 6000) {
            delay(100);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("WiFi auto-connect: legacy connection restored: %s\n",
                          WiFi.SSID().c_str());
            return true;
        }
        WiFi.disconnect(false, false);
        Serial.println("WiFi auto-connect: no usable saved networks");
        return false;
    }
    if (WiFi.status() == WL_CONNECTED) return true;

    WiFi.disconnect(false, false);
    WiFi.scanDelete();
    delay(100);
    int scanCount = WiFi.scanNetworks(false, true);
    if (scanCount <= 0) {
        Serial.printf("WiFi auto-connect: scan returned %d\n", scanCount);
        return false;
    }

    std::vector<AutoWiFiCandidate> candidates;
    for (size_t credIndex = 0; credIndex < savedNetworks.size(); ++credIndex) {
        int strongestRssi = -1000;
        for (int scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
            if (WiFi.SSID(scanIndex) == savedNetworks[credIndex].ssid) {
                strongestRssi = max(strongestRssi, WiFi.RSSI(scanIndex));
            }
        }
        if (strongestRssi > -1000)
            candidates.push_back({(int)credIndex, strongestRssi});
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const AutoWiFiCandidate &a, const AutoWiFiCandidate &b) {
                  return a.rssi > b.rssi;
              });
    WiFi.scanDelete();

    int attempts = 0;
    for (const auto &candidate : candidates) {
        if (attempts++ >= 3) break;
        const WiFiCred &cred = savedNetworks[candidate.credentialIndex];
        Serial.printf("WiFi auto-connect: trying %s (%d dBm)\n",
                      cred.ssid.c_str(), candidate.rssi);
        WiFi.begin(cred.ssid.c_str(), cred.pass.c_str());
        unsigned long started = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - started < 7000) {
            delay(100);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("WiFi auto-connect: connected to %s, IP=%s\n",
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            return true;
        }
        WiFi.disconnect(false, false);
        delay(100);
    }

    Serial.println("WiFi auto-connect: saved networks unavailable");
    return false;
}

void scanWiFi() {
    WiFi.mode(WIFI_STA);
    routerState = ROUTER_SCANNING;
    SymbianUI::drawMessageScreen("Network scanner", SymbianUI::ICON_SEARCH,
                                 "Scanning", "Searching for WiFi networks", "", "Cancel");
    SymbianUI::drawProgressBar(35, 188, 170, 15);

    // Use the synchronous scan path from Arduino-ESP32 2.x. It is more stable
    // on this ESP32-S3 board than polling scanComplete() asynchronously.
    WiFi.scanDelete();
    delay(100);
    SymbianUI::drawProgressBar(35, 188, 170, 35);
    int result = WiFi.scanNetworks(false, true);
    wifiCount = result < 0 ? 0 : result;
    Serial.printf("WiFi scan result: %d network(s), raw=%d\n", wifiCount, result);
    SymbianUI::drawProgressBar(35, 188, 170, 100);
    delay(120);
    
    routerState = ROUTER_LIST;
    routerSel = 0;
    routerScroll = 0;
    drawRouterApp();
}

// Detailed scan: like scanWiFi() but keeps hidden networks, builds a signal-
// sorted display order and opens the detail list showing BSSID, channel and
// encryption for every access point (Bruce-style detailed view).
void scanWiFiDetailed() {
    WiFi.mode(WIFI_STA);
    routerState = ROUTER_SCANNING;
    SymbianUI::drawMessageScreen("Detailed scan", SymbianUI::ICON_SEARCH,
                                 "Scanning", "Collecting BSSID / channel info", "", "Cancel");
    SymbianUI::drawProgressBar(35, 188, 170, 20);

    WiFi.scanDelete();
    delay(100);
    SymbianUI::drawProgressBar(35, 188, 170, 45);
    int result = WiFi.scanNetworks(false, true);   // show_hidden = true
    wifiCount = result < 0 ? 0 : result;
    SymbianUI::drawProgressBar(35, 188, 170, 100);

    // Build a display order sorted by RSSI (strongest first).
    wifiOrder.clear();
    for (int i = 0; i < wifiCount; ++i) wifiOrder.push_back(i);
    std::sort(wifiOrder.begin(), wifiOrder.end(),
              [](int a, int b) { return WiFi.RSSI(a) > WiFi.RSSI(b); });

    Serial.printf("WiFi detailed scan: %d network(s)\n", wifiCount);
    delay(120);

    routerState = ROUTER_DETAIL;
    routerSel = 0;
    routerScroll = 0;
    drawRouterApp();
}
bool inputWifiPassword(String& password, String ssid, int encType) {
    keyboard.begin(true);
    
    bool inputComplete = false;
    int keyResult = -1;
    keyboard.draw(true);
    
    while (!inputComplete) {
        keyResult = keyboard.handleInput(password);
        
        if (keyResult == 1) { // OK pressed
            inputComplete = true;
            return true;
        } else if (keyResult == 2) { // Cancel pressed
            inputComplete = true;
            password = "";
            return false;
        }
        
        delay(10);
    }
    
    return false;
}

void connectToWiFi(String ssid, String pass) {
    SymbianUI::drawMessageScreen("WiFi connect", SymbianUI::ICON_WIFI,
                                 "Connecting", UiLayout::ellipsize(ssid, 28), "", "Cancel");
    
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false, false);
    delay(100);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    // Animated connecting dots
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 25) {
        delay(400);
        timeout++;
        SymbianUI::drawProgressBar(35, 188, 170, timeout * 4);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
       // Save to JSON
       loadSavedNetworks(); // Refresh first
       addOrUpdateSavedNetwork(ssid, pass);
       saveSavedNetworks();
       
       SymbianUI::drawMessageScreen("WiFi connect", SymbianUI::ICON_WIFI,
                                    "Connected", WiFi.localIP().toString(), "", "Done",
                                    SymbianUI::GOOD);
       
       delay(2000);
       routerState = ROUTER_LIST;
    } else {
       SymbianUI::drawMessageScreen("WiFi connect", SymbianUI::ICON_LOCK,
                                    "Connection failed", "Check password and try again", "", "Back",
                                    SymbianUI::BAD);
       
       delay(2000);
       routerState = ROUTER_LIST;
    }
    drawRouterApp();
}

void saveScanToSD() {
    if (!mountSDCard()) {
        tft.drawString("No SD Card!", UiLayout::CENTER_X, 120, 2);
        delay(1000);
        return;
    }
    fs::File f = SD.open("/wifi_scan.txt", FILE_WRITE);
    if (f) {
        f.println("SSID, BSSID, RSSI, Channel, Encryption");
        for (int i=0; i<wifiCount; i++) {
             String ssid = WiFi.SSID(i);
             if (ssid.length() == 0) ssid = "<hidden>";
             f.printf("%s, %s, %d, %d, %s\n", ssid.c_str(), WiFi.BSSIDstr(i).c_str(),
                      WiFi.RSSI(i), WiFi.channel(i), getEncryptionName(WiFi.encryptionType(i)).c_str());
        }
        f.close();
        tft.drawString("Saved to /wifi_scan.txt", UiLayout::CENTER_X, 120, 2);
    } else {
        tft.drawString("Save Failed!", UiLayout::CENTER_X, 120, 2);
    }
    delay(1000);
}

void drawRouterApp() {
    String title = "WiFi";
    if (routerState == ROUTER_LIST) title = "Available Networks";
    else if (routerState == ROUTER_DETAIL) title = "Detailed Scan";
    else if (routerState == ROUTER_PROPS) title = "WiFi Status";
    else if (routerState == ROUTER_SAVED) title = "Saved Networks";
    else if (routerState == ROUTER_ADVANCED) title = "Advanced";
    else if (routerState == ROUTER_SENSE) title = "WiFi Sense";
    else if (routerState == ROUTER_ESPECTRE) title = "ESPectre";
    else if (routerState == ROUTER_FALL) title = "Fall Detect";
    else if (routerState == ROUTER_DIAGNOSE) title = "AI Diagnose";
    else if (routerState == ROUTER_SCANNER) title = "Network Scanner";
    SymbianUI::drawChrome(title, "Select", "Back");
    int startY = 62;
    
    // Main Menu
    if (routerState == ROUTER_MAIN) {
        const char* menu[] = {"Available Networks", "Saved Networks", "WiFi Status", "Advanced"};
        const SymbianUI::Icon icons[] = {
            SymbianUI::ICON_SEARCH, SymbianUI::ICON_BOOKMARK,
            SymbianUI::ICON_INFO, SymbianUI::ICON_SETTINGS
        };
        int menuCount = 4;
        
        for (int i = 0; i < menuCount; i++) {
            String value = ">";
            if (i == 2) value = WiFi.status() == WL_CONNECTED ? "Connected" : "Offline";
            SymbianUI::drawListRow(58 + i * 38, 34, menu[i],
                                   i == routerSel, value, icons[i]);
        }
        SymbianUI::drawSectionLabel(216, "Connection");
        SymbianUI::drawInfoLine(246, "Mode", WiFi.getMode() == WIFI_OFF ? "Off" : "Station");
        SymbianUI::drawInfoLine(265, "Address", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Not connected");
        SymbianUI::drawSoftkeys("Select", "Back");
    }
    
    // WiFi List
    else if (routerState == ROUTER_LIST) {
        int itemH = 34;
        int gap = 2;
        int visible = 6;
        
        if (wifiCount == 0) {
            SymbianUI::drawEmptyState(SymbianUI::ICON_WIFI,
                                      "No networks found", "Select Scan to try again");
        } else {
            for (int i = 0; i < visible; i++) {
                int idx = routerScroll + i;
                if (idx >= wifiCount) break;
                
                int y = 56 + i * (itemH + gap);
                bool sel = (idx == routerSel);
                String ssid = WiFi.SSID(idx);
                int rssi = WiFi.RSSI(idx);
                int bars = getSignalBars(rssi);
                int encType = WiFi.encryptionType(idx);
                String value = String(rssi) + " dBm";
                SymbianUI::drawListRow(y, itemH, ssid, sel, value,
                                       encType == WIFI_AUTH_OPEN ? SymbianUI::ICON_WIFI
                                                                 : SymbianUI::ICON_LOCK);
                int barsX = 180;
                for (int b = 0; b < 4; ++b) {
                    int barH = 3 + b * 2;
                    tft.fillRect(barsX + b * 5, y + 25 - barH, 3, barH,
                                 b < bars ? (sel ? TFT_WHITE : SymbianUI::ACCENT)
                                          : SymbianUI::MUTED);
                }
            }
            
            // Network count
            tft.setTextColor(TFT_SILVER);
            tft.setTextDatum(MR_DATUM);
            tft.drawString(String(wifiCount) + " networks", 232, 286, 1);
        }
        
        // Professional Scrollbar
        if (wifiCount > visible) {
            int scrollH = 216;
            int itemTotalH = wifiCount * (itemH + gap);
            int barH = max(15, (scrollH * scrollH) / itemTotalH);  // Min height 15px
            int barY = 56 + (routerScroll * scrollH) / max(1, wifiCount - visible);
            
            // Scrollbar background
            tft.fillRect(236, 56, 4, scrollH, SymbianUI::MUTED);
            // Scrollbar thumb
            tft.fillRect(235, barY, 5, barH, SymbianUI::ACCENT);
        }
    }
    
    else if (routerState == ROUTER_DETAIL) {
        int itemH = 42;
        int gap = 2;
        int visible = 5;

        if (wifiCount == 0) {
            SymbianUI::drawEmptyState(SymbianUI::ICON_WIFI,
                                      "No networks found", "Select Deep Scan to retry");
        } else {
            for (int i = 0; i < visible; i++) {
                int row = routerScroll + i;
                if (row >= wifiCount || row >= (int)wifiOrder.size()) break;
                int idx = wifiOrder[row];

                int y = 56 + i * (itemH + gap);
                bool sel = (row == routerSel);
                int rssi = WiFi.RSSI(idx);
                int encType = WiFi.encryptionType(idx);
                String ssid = WiFi.SSID(idx);
                if (ssid.length() == 0) ssid = "<hidden>";
                // Line 1: SSID + RSSI, with lock/open icon.
                SymbianUI::drawListRow(y, itemH, UiLayout::ellipsize(ssid, 20), sel,
                                       String(rssi) + " dBm",
                                       encType == WIFI_AUTH_OPEN ? SymbianUI::ICON_WIFI
                                                                 : SymbianUI::ICON_LOCK);
                // Line 2: BSSID + channel + encryption detail.
                String detail = WiFi.BSSIDstr(idx) + "  Ch" +
                                String(WiFi.channel(idx)) + "  " +
                                getEncryptionName(encType);
                tft.setTextColor(sel ? TFT_WHITE : SymbianUI::DIM,
                                 sel ? SymbianUI::SELECT : SymbianUI::BG);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(UiLayout::ellipsize(detail, 36), 40, y + 30, 1);
            }

            tft.setTextColor(TFT_SILVER, SymbianUI::BG);
            tft.setTextDatum(MR_DATUM);
            tft.drawString(String(wifiCount) + " networks", 232, 286, 1);
        }

        // Scrollbar
        if (wifiCount > visible) {
            int scrollH = 216;
            int itemTotalH = wifiCount * (itemH + gap);
            int barH = max(15, (scrollH * scrollH) / itemTotalH);
            int barY = 56 + (routerScroll * scrollH) / max(1, wifiCount - visible);
            tft.fillRect(236, 56, 4, scrollH, SymbianUI::MUTED);
            tft.fillRect(235, barY, 5, barH, SymbianUI::ACCENT);
        }
    }

    else if (routerState == ROUTER_PROPS) {
        bool connected = WiFi.status() == WL_CONNECTED;
        SymbianUI::drawEmptyState(SymbianUI::ICON_WIFI,
                                  connected ? "WiFi connected" : "WiFi offline",
                                  connected ? WiFi.SSID() : "Select a network to connect");
        SymbianUI::drawSectionLabel(195, "Connection details");
        SymbianUI::drawInfoLine(225, "IP address", connected ? WiFi.localIP().toString() : "-");
        SymbianUI::drawInfoLine(245, "Signal", connected ? getSignalStrength(WiFi.RSSI()) : "-");
        SymbianUI::drawInfoLine(265, "RSSI", connected ? String(WiFi.RSSI()) + " dBm" : "-");
    }

    // Advanced Menu
    else if (routerState == ROUTER_ADVANCED) {
          const char* menu[] = {"Scanner Tool", "Packet Monitor", "ADB Devices",
                                "WiFi Sense", "ESPectre", "Fall Detect", "AI Diagnose"};
          const SymbianUI::Icon menuIcons[] = {SymbianUI::ICON_SEARCH,
              SymbianUI::ICON_WIFI, SymbianUI::ICON_TERMINAL, SymbianUI::ICON_DISPLAY,
              SymbianUI::ICON_TERMINAL, SymbianUI::ICON_WIFI, SymbianUI::ICON_TERMINAL};
          for (int i = 0; i < 7; ++i) {
              String value = ">";
              if (i == 3) value = WiFi.status() == WL_CONNECTED ? "Heatmap" : "Offline";
              if (i == 4) value = WiFi.status() == WL_CONNECTED ? "CSI" : "Offline";
              if (i == 5) value = "Alert";
              if (i == 6) value = aiModelLoaded() ? "Model" : "Load AI";
              SymbianUI::drawListRow(54 + i * 30, 28, menu[i], i == routerSel,
                                     value, menuIcons[i]);
          }
          SymbianUI::drawSectionLabel(270, "Tools");
          SymbianUI::drawInfoLine(292, "State", WiFi.status() == WL_CONNECTED ? "Ready" : "Offline");
    }

    // ESPectre CSI motion sensing
    else if (routerState == ROUTER_ESPECTRE) {
        espectreDraw();
        espectreRender();
        espectreRenderChart();
    }

    // WiFi Sense heatmap
    else if (routerState == ROUTER_SENSE) {
        senseRenderContent();
    }

    // AI connection diagnosis
    else if (routerState == ROUTER_DIAGNOSE) {
        SymbianUI::drawChrome("AI Diagnose", "", "Back");
        SymbianUI::drawSectionLabel(42, "Link analysis");
        tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
        tft.setTextDatum(TL_DATUM);
        int y = 62;
        String rem = aiDiagText;
        if (rem.length() == 0) {
            tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
            tft.drawString("Select to run AI diagnosis", 8, y, 1);
            tft.drawString("(needs a loaded model)", 8, y + 16, 1);
        } else {
            while (rem.length() && y < 286) {
                int nl = rem.indexOf('\n');
                String line = (nl == -1) ? rem : rem.substring(0, nl);
                if (nl == -1) rem = "";
                else rem = rem.substring(nl + 1);
                while (line.length() > 32) {
                    tft.drawString(UiLayout::ellipsize(line, 33), 8, y, 1);
                    line = line.substring(32);
                    y += 14;
                }
                if (line.length()) {
                    tft.drawString(line, 8, y, 1);
                    y += 14;
                }
            }
        }
        SymbianUI::drawSoftkeys("", "Back");
    }
    
    // Scanner Menu
    else if (routerState == ROUTER_SCANNER) {
        const char* scanIcons[] = {"🔍", "📡", "💾", "🔙"};
        const char* scanMenu[] = {"Quick Scan", "Deep Scan", "Saved Networks", "Back"};
        const char* scanDesc[] = {"Fast network scan", "Detailed scan with more info", "View saved networks", "Return to main menu"};
        const SymbianUI::Icon menuIcons[] = {SymbianUI::ICON_SEARCH,
            SymbianUI::ICON_WIFI, SymbianUI::ICON_BOOKMARK, SymbianUI::ICON_NONE};
        for (int i = 0; i < 4; ++i)
            SymbianUI::drawListRow(62 + i * 48, 44, scanMenu[i], i == routerSel,
                                   scanDesc[i], menuIcons[i]);
    }
    
    // Context Menu
    else if (routerState == ROUTER_CONTEXT) {
        drawRouterContext(routerCtxSel);
    }
    
    // Saved Networks
    else if (routerState == ROUTER_SAVED) {
        int itemH = 35;
        int gap = 6;
        int listW = 220;
        int listX = 10;
        
        if (savedNetworks.empty()) {
            SymbianUI::drawEmptyState(SymbianUI::ICON_BOOKMARK,
                                      "No saved networks", "Connect to a network first");
        } else {
            int visible = 5;
            for (int i = 0; i < visible; i++) {
                int idx = routerScroll + i;
                if (idx >= (int)savedNetworks.size()) break;
                
                int y = startY + i * (itemH + gap);
                bool sel = (idx == routerSel);
                
                String ssid = savedNetworks[idx].ssid;
                SymbianUI::drawListRow(y, itemH, ssid, sel, "Saved",
                                       SymbianUI::ICON_LOCK);
            }
            
            tft.setTextColor(TFT_SILVER);
            tft.setTextDatum(MR_DATUM);
            tft.drawString(String(savedNetworks.size()) + " saved", 230, startY - 5, 1);
        }
    }
}

void drawRouterContext(int sel) {
    // LEFT aligned context menu (not centered like POWER MENU)
    int menuW = 200;
    int menuH = 140;
    int menuX = (240 - menuW) / 2;  // Canh giữa màn hình 240
    int menuY = 80;
    
    SymbianUI::drawChrome("Network options", "Select", "Back");
    SymbianUI::drawSectionLabel(55, UiLayout::ellipsize(WiFi.SSID(targetWifiIdx), 28));
    const char* ctx[] = {"Connect", "Forget/Delete", "Properties", "Cancel"};
    const SymbianUI::Icon ctxIcons[] = {SymbianUI::ICON_WIFI, SymbianUI::ICON_LOCK,
        SymbianUI::ICON_INFO, SymbianUI::ICON_NONE};
    for (int i = 0; i < 4; ++i)
        SymbianUI::drawListRow(78 + i * 38, 34, ctx[i], i == sel, ">", ctxIcons[i]);
}

// ---- flicker-free navigation: redraw only the row that changed --------------
void drawRouterCtxRow(int idx, bool sel) {
    const char* ctx[] = {"Connect", "Forget/Delete", "Properties", "Cancel"};
    const SymbianUI::Icon ctxIcons[] = {SymbianUI::ICON_WIFI, SymbianUI::ICON_LOCK,
        SymbianUI::ICON_INFO, SymbianUI::ICON_NONE};
    SymbianUI::drawListRow(78 + idx * 38, 34, ctx[idx], sel, ">", ctxIcons[idx]);
}

void drawRouterScrollbar() {
    int scrollH = 216, visible = 6;
    if (routerState == ROUTER_DETAIL) visible = 5;
    int total = (routerState == ROUTER_SAVED) ? (int)savedNetworks.size() : wifiCount;
    if (total <= visible) return;
    int itemTotalH = total * ((routerState == ROUTER_DETAIL) ? 44 : ((routerState == ROUTER_SAVED) ? 41 : 36));
    int barH = max(15, (scrollH * scrollH) / itemTotalH);
    int barY = 56 + (routerScroll * scrollH) / max(1, total - visible);
    tft.fillRect(236, 56, 4, scrollH, SymbianUI::MUTED);
    tft.fillRect(235, barY, 5, barH, SymbianUI::ACCENT);
}

// Draw one row of the current router state at its computed position.
void drawRouterRow(int idx, bool sel) {
    if (routerState == ROUTER_MAIN) {
        const char* menu[] = {"Available Networks", "Saved Networks", "WiFi Status", "Advanced"};
        const SymbianUI::Icon icons[] = {SymbianUI::ICON_SEARCH, SymbianUI::ICON_BOOKMARK,
            SymbianUI::ICON_INFO, SymbianUI::ICON_SETTINGS};
        String value = ">";
        if (idx == 2) value = WiFi.status() == WL_CONNECTED ? "Connected" : "Offline";
        SymbianUI::drawListRow(58 + idx * 38, 34, menu[idx], sel, value, icons[idx]);
    }
    else if (routerState == ROUTER_SCANNER) {
        const char* scanMenu[] = {"Quick Scan", "Deep Scan", "Saved Networks", "Back"};
        const char* scanDesc[] = {"Fast network scan", "Detailed scan", "View saved", "Return"};
        const SymbianUI::Icon menuIcons[] = {SymbianUI::ICON_SEARCH, SymbianUI::ICON_WIFI,
            SymbianUI::ICON_BOOKMARK, SymbianUI::ICON_NONE};
        SymbianUI::drawListRow(62 + idx * 48, 44, scanMenu[idx], sel, scanDesc[idx], menuIcons[idx]);
    }
    else if (routerState == ROUTER_ADVANCED) {
        const char* menu[] = {"Scanner Tool", "Packet Monitor", "ADB Devices",
                              "WiFi Sense", "ESPectre", "Fall Detect", "AI Diagnose"};
        const SymbianUI::Icon menuIcons[] = {SymbianUI::ICON_SEARCH, SymbianUI::ICON_WIFI,
            SymbianUI::ICON_TERMINAL, SymbianUI::ICON_DISPLAY, SymbianUI::ICON_TERMINAL,
            SymbianUI::ICON_WIFI, SymbianUI::ICON_TERMINAL};
        String value = ">";
        if (idx == 3) value = WiFi.status() == WL_CONNECTED ? "Heatmap" : "Offline";
        if (idx == 4) value = WiFi.status() == WL_CONNECTED ? "CSI" : "Offline";
        if (idx == 5) value = "Alert";
        if (idx == 6) value = aiModelLoaded() ? "Model" : "Load AI";
        SymbianUI::drawListRow(54 + idx * 30, 28, menu[idx], sel, value, menuIcons[idx]);
    }
    else if (routerState == ROUTER_LIST) {
        int itemH = 34, gap = 2, visible = 6;
        int row = idx - routerScroll;
        if (idx < 0 || idx >= wifiCount || row < 0 || row >= visible) return;
        int y = 56 + row * (itemH + gap);
        int rssi = WiFi.RSSI(idx);
        int bars = getSignalBars(rssi);
        int encType = WiFi.encryptionType(idx);
        SymbianUI::drawListRow(y, itemH, WiFi.SSID(idx), sel,
                               String(rssi) + " dBm",
                               encType == WIFI_AUTH_OPEN ? SymbianUI::ICON_WIFI
                                                         : SymbianUI::ICON_LOCK);
        int barsX = 180;
        for (int b = 0; b < 4; ++b) {
            int barH = 3 + b * 2;
            tft.fillRect(barsX + b * 5, y + 25 - barH, 3, barH,
                         b < bars ? (sel ? TFT_WHITE : SymbianUI::ACCENT)
                                  : SymbianUI::MUTED);
        }
    }
    else if (routerState == ROUTER_DETAIL) {
        int itemH = 42, gap = 2, visible = 5;
        int row = idx - routerScroll;
        if (idx < 0 || idx >= wifiCount || idx >= (int)wifiOrder.size() ||
            row < 0 || row >= visible) return;
        int i = wifiOrder[idx];
        int y = 56 + row * (itemH + gap);
        int rssi = WiFi.RSSI(i);
        int encType = WiFi.encryptionType(i);
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "<hidden>";
        SymbianUI::drawListRow(y, itemH, UiLayout::ellipsize(ssid, 20), sel,
                               String(rssi) + " dBm",
                               encType == WIFI_AUTH_OPEN ? SymbianUI::ICON_WIFI
                                                         : SymbianUI::ICON_LOCK);
        String detail = WiFi.BSSIDstr(i) + "  Ch" + String(WiFi.channel(i)) + "  " +
                        getEncryptionName(encType);
        tft.setTextColor(sel ? TFT_WHITE : SymbianUI::DIM,
                         sel ? SymbianUI::SELECT : SymbianUI::BG);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(UiLayout::ellipsize(detail, 36), 40, y + 30, 1);
    }
    else if (routerState == ROUTER_SAVED) {
        int itemH = 35, gap = 6;
        int row = idx - routerScroll;
        if (idx < 0 || idx >= (int)savedNetworks.size() || row < 0 || row >= 5) return;
        int y = 62 + row * (itemH + gap);
        SymbianUI::drawListRow(y, itemH, savedNetworks[idx].ssid, sel, "Saved",
                               SymbianUI::ICON_LOCK);
    }
}

// Repaint every visible row of the current state (scroll changed case).
void drawRouterRows() {
    int n = 0;
    if (routerState == ROUTER_MAIN || routerState == ROUTER_SCANNER) {
        for (int i = 0; i < 4; ++i) drawRouterRow(i, i == routerSel);
        return;
    }
    if (routerState == ROUTER_ADVANCED) {
        for (int i = 0; i < 7; ++i) drawRouterRow(i, i == routerSel);
        return;
    }
    if (routerState == ROUTER_LIST) n = min(wifiCount - routerScroll, 6);
    else if (routerState == ROUTER_DETAIL) n = min(wifiCount - routerScroll, 5);
    else if (routerState == ROUTER_SAVED) n = min((int)savedNetworks.size() - routerScroll, 5);
    for (int i = 0; i < n; ++i) drawRouterRow(routerScroll + i, (routerScroll + i) == routerSel);
    drawRouterScrollbar();
}

void loopRouter() {

    // WiFi Sense heatmap: promiscuous RX feeds per-packet RSSI, redraw live.
    if (routerState == ROUTER_SENSE) {
        static unsigned long lastPush = 0, lastTick = 0;
        unsigned long now = millis();
        if (WiFi.status() == WL_CONNECTED) {
            if (now - lastPush >= SENSE_INTERVAL_MS) {
                lastPush = now;
                if (millis() - sLastPktMs > 250) {   // no packets: cached RSSI fallback
                    int8_t v = (int8_t)WiFi.RSSI();
                    if (v < -100) v = -100;
                    if (v > -10) v = -10;
                    sensePushSample(v);
                }
            }
        }
        if (now - lastTick >= 120) {
            lastTick = now;
            senseUpdateMotion();
            senseHeatmapUpdate();
            senseRenderContent();
        }
        if (isBackPressed()) {
            senseTeardown();
            routerState = ROUTER_ADVANCED;
            routerSel = 0;
            drawRouterApp();
            delay(200);
            return;
        }
        if (digitalRead(KEY_OPTION) == LOW) {
            senseCalibrate();
            delay(200);
            return;
        }
        delay(8);
        return;
    }

    // ESPectre CSI motion detector (self-contained loop, handles its own keys)
    else if (routerState == ROUTER_ESPECTRE) {
        espectreLoop();
        return;
    }

    // Fall detection alert (self-contained loop, handles its own keys)
    else if (routerState == ROUTER_FALL) {
        fallLoop();
        return;
    }

    // AI connection diagnosis (text screen, runs on select)
    else if (routerState == ROUTER_DIAGNOSE) {
        if (isBackPressed()) {
            routerState = ROUTER_ADVANCED;
            routerSel = 0;
            drawRouterApp();
            delay(200);
            return;
        }
        delay(8);
        return;
    }

    // Global Back Handler
    if (isBackPressed()) {
        if (routerState == ROUTER_CONTEXT) {
             routerState = routerCtxReturn;
             drawRouterApp();
        } else if (routerState == ROUTER_LIST || routerState == ROUTER_DETAIL || routerState == ROUTER_PROPS || routerState == ROUTER_ADVANCED || routerState == ROUTER_SAVED || routerState == ROUTER_SCANNER) {
             routerState = ROUTER_MAIN;
             drawRouterApp();
        }
        delay(200);
        return;
    }

    // Nav
    bool moved = false;
    int oldSel = routerSel;
    int oldScroll = routerScroll;
    if (buttonManager.isJustPressed(KEY_DOWN)) {
        if (routerState == ROUTER_CONTEXT) { routerCtxSel = (routerCtxSel + 1) % 4; drawRouterCtxRow((routerCtxSel + 3) % 4, false); drawRouterCtxRow(routerCtxSel, true); delay(150); return; }
        
        if (routerState == ROUTER_MAIN) routerSel = (routerSel + 1) % 4;
        else if (routerState == ROUTER_SCANNER) routerSel = (routerSel + 1) % 4;
        else if (routerState == ROUTER_ADVANCED) routerSel = (routerSel + 1) % 7;
        else if (routerState == ROUTER_LIST && wifiCount > 0) {
            if (routerSel < wifiCount - 1) {
                routerSel++;
                if (routerSel >= routerScroll + 5) routerScroll++;
            }
        }
        else if (routerState == ROUTER_DETAIL && wifiCount > 0) {
            if (routerSel < wifiCount - 1) {
                routerSel++;
                if (routerSel >= routerScroll + 5) routerScroll++;
            }
        }
        else if (routerState == ROUTER_SAVED && !savedNetworks.empty()) {
            if (routerSel < savedNetworks.size() - 1) {
                routerSel++;
                if (routerSel >= routerScroll + 5) routerScroll++;
            }
        }
        moved = true; delay(150);
    } 
    else if (buttonManager.isJustPressed(KEY_UP)) {
        if (routerState == ROUTER_CONTEXT) { routerCtxSel = (routerCtxSel + 3) % 4; drawRouterCtxRow((routerCtxSel + 1) % 4, false); drawRouterCtxRow(routerCtxSel, true); delay(150); return; }

        if (routerState == ROUTER_MAIN) routerSel = (routerSel + 3) % 4;
        else if (routerState == ROUTER_SCANNER) routerSel = (routerSel + 3) % 4;
        else if (routerState == ROUTER_ADVANCED) routerSel = (routerSel + 6) % 7;
        else if (routerState == ROUTER_LIST && wifiCount > 0) {
            if (routerSel > 0) {
                routerSel--;
                if (routerSel < routerScroll) routerScroll--;
            }
        }
        else if (routerState == ROUTER_DETAIL && wifiCount > 0) {
            if (routerSel > 0) {
                routerSel--;
                if (routerSel < routerScroll) routerScroll--;
            }
        }
        else if (routerState == ROUTER_SAVED && !savedNetworks.empty()) {
            if (routerSel > 0) {
                routerSel--;
                if (routerSel < routerScroll) routerScroll--;
            }
        }
        moved = true; delay(150);
    }

    if (moved) {
        bool scrollChanged = (routerScroll != oldScroll);
        if (scrollChanged) {
            drawRouterRows();
        } else {
            drawRouterRow(oldSel, false);
            drawRouterRow(routerSel, true);
        }
    }

    // START(center OK): Default Action
    if (isSelectPressed()) {
        if (routerState == ROUTER_MAIN) {
            if (routerSel == 0) { 
                routerState = ROUTER_SCANNER;
                routerSel = 0;
                drawRouterApp();
            }
            else if (routerSel == 1) { 
                loadSavedNetworks();
                routerState = ROUTER_SAVED;
                routerSel = 0;
                routerScroll = 0;
                drawRouterApp();
            }
            else if (routerSel == 2) { 
                routerState = ROUTER_PROPS; 
                drawRouterApp();
            }
            else if (routerSel == 3) { 
                routerState = ROUTER_ADVANCED; 
                routerSel = 0; 
                drawRouterApp();
            }
        }

        else if (routerState == ROUTER_ADVANCED) {
            if (routerSel == 3) { // WiFi Sense heatmap
                if (WiFi.status() == WL_CONNECTED) {
                    senseStart();
                    routerState = ROUTER_SENSE;
                    drawRouterApp();
                } else {
                    SymbianUI::drawMessageScreen("WiFi Sense",
                        SymbianUI::ICON_WIFI, "Not connected",
                        "Connect to a WiFi network first", "", "Back");
                    delay(1200);
                    drawRouterApp();
                }
            }
            else if (routerSel == 4) { // ESPectre CSI motion sensing
                if (WiFi.status() == WL_CONNECTED) {
                    if (espectreStart()) {
                        routerState = ROUTER_ESPECTRE;
                        drawRouterApp();
                    } else {
                        SymbianUI::drawMessageScreen("ESPectre",
                            SymbianUI::ICON_TERMINAL, "CSI failed",
                            "Wi-Fi stack rejected CSI", "", "Back");
                        delay(1200);
                        drawRouterApp();
                    }
                } else {
                    SymbianUI::drawMessageScreen("ESPectre",
                        SymbianUI::ICON_WIFI, "Not connected",
                        "Connect to a WiFi network first", "", "Back");
                    delay(1200);
                    drawRouterApp();
                }
            }
            else if (routerSel == 5) { // Fall Detect
                if (WiFi.status() == WL_CONNECTED) {
                    if (fallStart()) {
                        routerState = ROUTER_FALL;
                        drawRouterApp();
                    } else {
                        SymbianUI::drawMessageScreen("Fall Detect",
                            SymbianUI::ICON_TERMINAL, "Sensors failed",
                            "Wi-Fi stack rejected CSI", "", "Back");
                        delay(1200);
                        drawRouterApp();
                    }
                } else {
                    SymbianUI::drawMessageScreen("Fall Detect",
                        SymbianUI::ICON_WIFI, "Not connected",
                        "Connect to a WiFi network first", "", "Back");
                    delay(1200);
                    drawRouterApp();
                }
            }
            else if (routerSel == 6) { // AI Diagnose
                aiDiagRun();
                routerState = ROUTER_DIAGNOSE;
                drawRouterApp();
            }
        }

        // Scanner Mode
        else if (routerState == ROUTER_SCANNER) {
            if (routerSel == 0) { // Quick Scan
                scanWiFi();
            }
            else if (routerSel == 1) { // Deep Scan
                scanWiFiDetailed(); // Detailed scan: BSSID/channel/encryption/hidden
            }
            else if (routerSel == 2) { // Saved Networks
                loadSavedNetworks();
                routerState = ROUTER_SAVED;
                routerSel = 0;
                routerScroll = 0;
                drawRouterApp();
            }
            else if (routerSel == 3) { // Back
                routerState = ROUTER_MAIN;
                routerSel = 0;
                drawRouterApp();
            }
        }
        else if (routerState == ROUTER_LIST) {
            if (wifiCount > 0) {
                // Show context menu for selected WiFi
                targetWifiIdx = routerSel;
                targetWifiEncryption = WiFi.encryptionType(targetWifiIdx);
                routerCtxReturn = ROUTER_LIST;
                routerState = ROUTER_CONTEXT;
                routerCtxSel = 0;
                drawRouterContext(routerCtxSel);
            }
        }
        else if (routerState == ROUTER_DETAIL) {
            if (wifiCount > 0 && routerSel < (int)wifiOrder.size()) {
                // Map the sorted display row back to the raw scan index.
                targetWifiIdx = wifiOrder[routerSel];
                targetWifiEncryption = WiFi.encryptionType(targetWifiIdx);
                routerCtxReturn = ROUTER_DETAIL;
                routerState = ROUTER_CONTEXT;
                routerCtxSel = 0;
                drawRouterContext(routerCtxSel);
            }
        }
        // Handle Context Menu Selection
        else if (routerState == ROUTER_CONTEXT) {
            if (routerCtxSel == 0) { // Connect
                String ssid = WiFi.SSID(targetWifiIdx);
                if (targetWifiEncryption == WIFI_AUTH_OPEN) {
                    // No password needed
                    connectToWiFi(ssid, "");
                } else {
                    // Request password
                    routerPass = "";
                    if (inputWifiPassword(routerPass, ssid, targetWifiEncryption)) {
                        connectToWiFi(ssid, routerPass);
                    } else {
                        // Cancelled
                        routerState = routerCtxReturn;
                        drawRouterApp();
                    }
                }
            }
            else if (routerCtxSel == 1) { // Forget/Delete
                String ssid = WiFi.SSID(targetWifiIdx);
                // Remove from saved networks
                for (size_t i = 0; i < savedNetworks.size(); i++) {
                    if (savedNetworks[i].ssid == ssid) {
                        savedNetworks.erase(savedNetworks.begin() + i);
                        break;
                    }
                }
                saveSavedNetworks();
                
                SymbianUI::drawMessageScreen("Saved networks", SymbianUI::ICON_INFO,
                                             "Network forgotten", ssid, "", "Done",
                                             SymbianUI::GOOD);
                delay(500);
                routerState = routerCtxReturn;
                drawRouterApp();
            }
            else if (routerCtxSel == 2) { // Properties
                String ssid = WiFi.SSID(targetWifiIdx);
                int rssi = WiFi.RSSI(targetWifiIdx);
                int encType = WiFi.encryptionType(targetWifiIdx);
                SymbianUI::drawChrome("Network properties", "", "Back");
                SymbianUI::drawSectionLabel(58, UiLayout::ellipsize(ssid, 28));
                SymbianUI::drawInfoLine(91, "Signal", getSignalStrength(rssi));
                SymbianUI::drawInfoLine(118, "RSSI", String(rssi) + " dBm");
                SymbianUI::drawInfoLine(145, "Security", getEncryptionName(encType));
                SymbianUI::drawInfoLine(172, "Channel", String(WiFi.channel(targetWifiIdx)));
                
                // Wait for back button
                while (!isBackPressed()) {
                    delay(50);
                }
                routerState = routerCtxReturn;
                drawRouterApp();
            }
            else if (routerCtxSel == 3) { // Cancel
                routerState = routerCtxReturn;
                drawRouterApp();
            }
            delay(300);
        }
        else if (routerState == ROUTER_SAVED) {
            if (!savedNetworks.empty()) {
                String ssid = savedNetworks[routerSel].ssid;
                String pass = savedNetworks[routerSel].pass;
                connectToWiFi(ssid, pass);
            }
        }
        
        delay(300);
    }
    
    // Save Scan Option
    if (digitalRead(KEY_OPTION) == LOW && (routerState == ROUTER_LIST || routerState == ROUTER_DETAIL)) {
        saveScanToSD();
        drawRouterApp();
    }
}

#endif
