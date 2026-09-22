#ifndef SETTINGS_APP_H
#define SETTINGS_APP_H

#include "component/Display.h"
#include <Preferences.h>
#include "component/Keyboard.h"
#include "component/Themes.h"
#include "component/Config.h" 
#include "component/ui_utils.h"
#include "ai_chat.h"

extern TFT_eSPI tft;

// Settings State
enum SettingsState { SET_MAIN, SET_THEMES, SET_MANAGEMENT, SET_AI };
SettingsState settingsState = SET_MAIN;

int settingsSel = 0;
int themeScroll = 0;
int settingsScroll = 0;
bool wifiEnabled = true;
bool btEnabled = true;

// AI model used for smart connection diagnosis / automation. Stored in NVS as
// a name or SD-relative path; defaults to "qeaf-esp-max".
String aiModelName = "qeaf-esp-max";
#define AI_MODEL_DEFAULT "qeaf-esp-max"

void drawSettingsApp();

// Resolve the configured AI model to an SD path, if a file matches.
String aiSettingsModelPath() {
  String n = aiModelName;
  n.trim();
  if (n.length() == 0) return "";
  // Absolute SD path, e.g. "/models/qeaf-esp-max.bin"
  if (n.startsWith("/")) {
    if (SD.exists(n)) return n;
    if (SD.exists("/sd" + n)) return "/sd" + n;
    return "";
  }
  // Bare name: try common locations.
  String cand[] = {"/" + n, "/models/" + n, "/models/" + n + ".bin",
                   "/sd/" + n, "/sd/models/" + n + ".bin", "/model/" + n + ".bin"};
  for (int i = 0; i < 6; ++i)
    if (SD.exists(cand[i])) return cand[i];
  return "";
}

// POCHITA OS keeps the WiFi radio available for scanning and networking.
// This does not force a connection; it only prevents WIFI_OFF mode.
void ensureWiFiAlwaysOn() {
    wifiEnabled = true;
    if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
}

void loadSettings() {
    Preferences prefs;
    prefs.begin("settings", false);
    // Older firmware may have persisted WiFi=false. Migrate it to always-on.
    wifiEnabled = true;
    btEnabled = prefs.getBool("bt", true);
    aiModelName = prefs.getString("aimodel", AI_MODEL_DEFAULT);
    if (aiModelName.length() == 0) aiModelName = AI_MODEL_DEFAULT;
    prefs.putBool("wifi", true);
    ensureWiFiAlwaysOn();
    prefs.end();

    Preferences themePrefs;
    themePrefs.begin("themes", true);
    int themeIndex = themePrefs.getInt("currentThemeIndex", 0);
    themePrefs.end();
    if (themeIndex < 0 || themeIndex >= themeCount) themeIndex = 0;
    currentTheme = systemThemes[themeIndex];
}

void saveSettings() {
    Preferences prefs;
    prefs.begin("settings", false);
    wifiEnabled = true;
    prefs.putBool("wifi", true);
    prefs.putBool("bt", btEnabled);
    prefs.putString("aimodel", aiModelName);
    prefs.end();
}

int getCurrentThemeIndex() {
    for (int i = 0; i < themeCount; i++) {
        if (String(systemThemes[i].name) == String(currentTheme.name)) {
            return i;
        }
    }
    return 0; // Default to first theme
}

void saveAllSettings() {
    // Save current settings
    saveSettings();
    
    // Save theme
    Preferences themePrefs;
    themePrefs.begin("themes", false);
    themePrefs.putInt("currentThemeIndex", getCurrentThemeIndex());
    themePrefs.end();
}

void restoreDefaultSettings() {
    // Reset to default values
    wifiEnabled = true;
    btEnabled = true;
    aiModelName = AI_MODEL_DEFAULT;
    
    ensureWiFiAlwaysOn();
    
    // Reset theme to default
    currentTheme = systemThemes[0];
    
    // Save defaults
    saveAllSettings();
}

void toggleWiFi() {
    // WiFi is a required system service and cannot be disabled.
    ensureWiFiAlwaysOn();
    saveSettings();
}

void toggleBT() {
    btEnabled = !btEnabled;
    saveSettings(); // Auto-save
}

void drawSettingsApp() {
    {
        String modernTitle = "Settings";
        if (settingsState == SET_THEMES) modernTitle = "Themes";
        else if (settingsState == SET_MANAGEMENT) modernTitle = "Manage settings";
        else if (settingsState == SET_AI) modernTitle = "AI Model";
        SymbianUI::drawChrome(modernTitle, "Select", "Back");

        if (settingsState == SET_MAIN) {
            const char* labels[] = {"WiFi", "Bluetooth", "Display", "System", "AI Model"};
            const SymbianUI::Icon icons[] = {
                SymbianUI::ICON_WIFI, SymbianUI::ICON_BLUETOOTH,
                SymbianUI::ICON_DISPLAY, SymbianUI::ICON_SYSTEM, SymbianUI::ICON_TERMINAL
            };
            for (int i = 0; i < 5; ++i) {
                String value;
                if (i == 0) value = wifiEnabled ? "ON" : "OFF";
                else if (i == 1) value = btEnabled ? "ON" : "OFF";
                else if (i == 2) value = currentTheme.name;
                else if (i == 3) value = ">";
                else value = UiLayout::ellipsize(aiModelName, 12);
                SymbianUI::drawListRow(52 + i * 36, 32, labels[i],
                                       i == settingsSel, value, icons[i]);
            }
            SymbianUI::drawSectionLabel(236, "Device preferences");
            SymbianUI::drawInfoLine(260, "AI model", UiLayout::ellipsize(aiModelName, 22));
            SymbianUI::drawInfoLine(284, "Display", "240 x 320");
        } else if (settingsState == SET_AI) {
            String models[24];
            int total = 0;
            if (SD.exists("/")) total = aiScanModels(models, 24);
            int rows = 1 + total;   // [default] + scanned models
            int first = constrain(settingsScroll, 0, max(0, rows - 6));
            int r = 0;
            for (int row = 0; row < 6 && first + row < rows; ++row) {
                int index = first + row;
                String label;
                bool isDefault = false;
                if (index == 0) {
                    label = String("Default: ") + AI_MODEL_DEFAULT;
                    isDefault = (aiModelName == AI_MODEL_DEFAULT);
                } else {
                    label = models[index - 1];
                    isDefault = (aiModelName == label);
                }
                SymbianUI::drawListRow(56 + row * 32, 30,
                                       UiLayout::ellipsize(label, 24),
                                       index == settingsSel,
                                       isDefault ? "Using" : ">");
            }
            drawScrollBar(235, 62, 4, 221, rows, first, 6, SymbianUI::ACCENT);
            SymbianUI::drawSectionLabel(270, "AI model");
            SymbianUI::drawInfoLine(292, "Diagnose", "WiFi advanced");
        } else if (settingsState == SET_THEMES) {
            constexpr int visible = 7;
            int first = constrain(themeScroll, 0, max(0, themeCount - visible));
            for (int row = 0; row < visible; ++row) {
                int index = first + row;
                if (index >= themeCount) break;
                SymbianUI::drawListRow(56 + row * 32, 30,
                                       systemThemes[index].name,
                                       index == settingsSel);
            }
            drawScrollBar(235, 62, 4, 221, themeCount, first, visible,
                          SymbianUI::ACCENT);
        } else {
            const char* labels[] = {"Save settings", "Restore defaults", "Export", "Back"};
            for (int i = 0; i < 4; ++i)
                SymbianUI::drawListRow(58 + i * 38, 34, labels[i],
                                       i == settingsSel);
        }
    }
    return;

    tft.fillScreen(BG_COLOR);
    
    // Header Bar - Nokia Symbian style
    tft.fillRect(0, 0, UiLayout::WIDTH, 18, 0x8410);
    tft.drawLine(0, 17, UiLayout::WIDTH, 17, 0x4208);
    tft.setTextColor(0x0000);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("SETTINGS", 5, 9, 1);
    
    // Title - Nokia Symbian style
    tft.fillRoundRect(25, 23, 190, 26, 6, 0x2145);
    tft.drawRoundRect(25, 23, 190, 26, 6, 0xFBE0);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    String title = "SETTINGS";
    if (settingsState == SET_THEMES) title = "THEMES";
    else if (settingsState == SET_MANAGEMENT) title = "MANAGE";
    tft.drawString(title, UiLayout::CENTER_X, 36, 2);
    
    int startY = 55;
    
    if (settingsState == SET_MAIN) {
        const char* menu[] = {"WiFi", "Bluetooth", "Themes", "Manage"};
        int menuCount = 4;
        
        int itemH = 42;
        int gap = 6;
        
        for (int i = 0; i < menuCount; i++) {
            int y = startY + i * (itemH + gap);
            bool sel = (i == settingsSel);
            
            if (sel) {
                tft.fillRoundRect(20, y, 200, itemH, 6, HL_COLOR);
                tft.drawRoundRect(20, y, 200, itemH, 6, THEME_COLOR);
            } else {
                tft.drawRoundRect(20, y, 200, itemH, 6, 0x444444);
            }
            
            // Toggle/Value
            String val = "";
            if (i == 0) val = wifiEnabled ? "ON" : "OFF";
            else if (i == 1) val = btEnabled ? "ON" : "OFF";
            else if (i == 2) val = currentTheme.name;
            else if (i == 3) val = ">";
            
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(sel ? TFT_WHITE : TFT_SILVER);
            tft.drawString(menu[i], 35, y + itemH/2, 2);
            
            // Value/Status
            tft.setTextDatum(MR_DATUM);
            if (i == 0 || i == 1) {
                tft.setTextColor(sel ? (val == "ON" ? TFT_GREEN : TFT_RED) : TFT_SILVER);
                tft.drawString(val, 210, y + itemH/2, 2);
            } else {
                tft.setTextColor(sel ? THEME_COLOR : 0x632C);
                tft.drawString(val, 210, y + itemH/2, 2);
            }
        }
        
        // Help
        tft.setTextColor(0x632C);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Move: Up/Down   Open: OK", UiLayout::CENTER_X, 283, 1);
    }
    else if (settingsState == SET_THEMES) {
        // Theme list
        constexpr int visibleThemes = 7;
        int itemH = 28;
        int gap = 4;
        int first = constrain(themeScroll, 0, max(0, themeCount - visibleThemes));

        for (int row = 0; row < visibleThemes; row++) {
            int i = first + row;
            if (i >= themeCount) break;
            int y = startY + row * (itemH + gap);
            bool sel = (i == settingsSel);
            
            if (sel) {
                tft.fillRoundRect(20, y, 200, itemH, 4, systemThemes[i].primary);
            }
            tft.drawRoundRect(20, y, 200, itemH, 4, sel ? TFT_WHITE : 0x3186);
            
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(sel ? TFT_WHITE : TFT_SILVER);
            tft.drawString(UiLayout::ellipsize(systemThemes[i].name, 22), 30, y + itemH/2, 2);
        }
        
        // Scrollbar
        drawScrollBar(UiLayout::WIDTH - 5, startY, 4,
                      visibleThemes * (itemH + gap) - gap,
                      themeCount, first, visibleThemes, THEME_COLOR);
    }
    
    else if (settingsState == SET_MANAGEMENT) {
        const char* mgmtMenu[] = {"Save", "Restore", "Export", "Back"};
        int menuCount = 4;
        
        int itemH = 42;
        int gap = 6;
        
        for (int i = 0; i < menuCount; i++) {
            int y = startY + i * (itemH + gap);
            bool sel = (i == settingsSel);
            
            if (sel) {
                tft.fillRoundRect(20, y, 200, itemH, 6, HL_COLOR);
                tft.drawRoundRect(20, y, 200, itemH, 6, THEME_COLOR);
            } else {
                tft.drawRoundRect(20, y, 200, itemH, 6, 0x444444);
            }
            
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(sel ? TFT_WHITE : TFT_SILVER);
            tft.drawString(mgmtMenu[i], 35, y + itemH/2, 2);
        }
        
        tft.setTextColor(TFT_SILVER);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("OK: Action   Back: Return", UiLayout::CENTER_X, 283, 1);
    }
}

void drawSettingsRow(int idx, bool sel) {
    if (settingsState == SET_MAIN) {
        const char* labels[] = {"WiFi", "Bluetooth", "Display", "System", "AI Model"};
        const SymbianUI::Icon icons[] = {
            SymbianUI::ICON_WIFI, SymbianUI::ICON_BLUETOOTH,
            SymbianUI::ICON_DISPLAY, SymbianUI::ICON_SYSTEM, SymbianUI::ICON_TERMINAL
        };
        String value;
        if (idx == 0) value = wifiEnabled ? "ON" : "OFF";
        else if (idx == 1) value = btEnabled ? "ON" : "OFF";
        else if (idx == 2) value = currentTheme.name;
        else if (idx == 3) value = ">";
        else value = UiLayout::ellipsize(aiModelName, 12);
        SymbianUI::drawListRow(52 + idx * 36, 32, labels[idx],
                               sel, value, icons[idx]);
    }
    else if (settingsState == SET_AI) {
        String models[24];
        int total = 0;
        if (SD.exists("/")) total = aiScanModels(models, 24);
        int rows = 1 + total;
        int index = idx;                       // absolute item index
        if (index < 0 || index >= rows) return;
        int first = constrain(settingsScroll, 0, max(0, rows - 6));
        int row = index - first;
        if (row < 0 || row >= 6) return;
        String label;
        bool isDefault = false;
        if (index == 0) {
            label = String("Default: ") + AI_MODEL_DEFAULT;
            isDefault = (aiModelName == AI_MODEL_DEFAULT);
        } else {
            label = models[index - 1];
            isDefault = (aiModelName == label);
        }
        SymbianUI::drawListRow(56 + row * 32, 30,
                               UiLayout::ellipsize(label, 24),
                               index == settingsSel, isDefault ? "Using" : ">");
    }
    else if (settingsState == SET_THEMES) {
        int first = constrain(themeScroll, 0, max(0, themeCount - 7));
        int row = idx - first;
        if (idx < 0 || idx >= themeCount || row < 0 || row >= 7) return;
        SymbianUI::drawListRow(56 + row * 32, 30,
                               systemThemes[idx].name, sel);
    }
    else if (settingsState == SET_MANAGEMENT) {
        const char* labels[] = {"Save settings", "Restore defaults", "Export", "Back"};
        SymbianUI::drawListRow(58 + idx * 38, 34, labels[idx], sel);
    }
}

void drawSettingsRows() {
    if (settingsState == SET_MAIN || settingsState == SET_MANAGEMENT) {
        for (int i = 0; i < 5; ++i) drawSettingsRow(i, i == settingsSel);
        return;
    }
    if (settingsState == SET_AI) {
        String models[24];
        int total = 0;
        if (SD.exists("/")) total = aiScanModels(models, 24);
        int rows = 1 + total;
        int first = constrain(settingsScroll, 0, max(0, rows - 6));
        int n = min(rows - first, 6);
        for (int i = 0; i < n; ++i)
            drawSettingsRow(first + i, (first + i) == settingsSel);
        drawScrollBar(235, 62, 4, 221, rows, first, 6, SymbianUI::ACCENT);
    }
    if (settingsState == SET_THEMES) {
        int first = constrain(themeScroll, 0, max(0, themeCount - 7));
        int n = min(themeCount - first, 7);
        for (int i = 0; i < n; ++i)
            drawSettingsRow(first + i, (first + i) == settingsSel);
        drawScrollBar(235, 62, 4, 221, themeCount, first, 7, SymbianUI::ACCENT);
    }
}

void loopSettings() {
    static bool settingsLoaded = false;
    if (!settingsLoaded) {
        loadSettings();
        settingsLoaded = true;
    }
    
    bool moved = false;
    // B button for going back
    if (buttonManager.isJustPressed(KEY_A)) {
        if (settingsState == SET_THEMES || settingsState == SET_MANAGEMENT ||
            settingsState == SET_AI) {
            SettingsState previousState = settingsState;
            settingsState = SET_MAIN;
            if (previousState == SET_THEMES) settingsSel = 2;
            else if (previousState == SET_AI) settingsSel = 4;
            else settingsSel = 3;
            drawSettingsApp();
            delay(200);
            return;
        } else {
            extern void drawLauncherContent();
            extern SystemMode currentMode;
            currentMode = MODE_LAUNCHER;
            drawLauncherContent();
            delay(200);
            return;
        }
    }
    
    if (buttonManager.isJustPressed(KEY_A)) {
        if (settingsState == SET_THEMES || settingsState == SET_AI ||
            settingsState == SET_MANAGEMENT) {
            SettingsState prev = settingsState;
            settingsState = SET_MAIN;
            if (prev == SET_AI) settingsSel = 4; else settingsSel = 2; 
            drawSettingsApp();
            delay(200);
            return;
        } else {
             extern void drawLauncherContent(); // ensure visibility
             extern SystemMode currentMode; // ensure visibility
             // Assuming MODE_LAUNCHER is available via Config.h
             currentMode = MODE_LAUNCHER;
             drawLauncherContent();
             delay(200);
             return;
        }
    }

    int oldSel = settingsSel;
    int oldScroll = settingsScroll;
    int oldThemeScroll = themeScroll;

    if (buttonManager.isJustPressed(KEY_DOWN)) {
        if (settingsState == SET_MAIN) {
            settingsSel = (settingsSel + 1) % 5;
            // Update scroll if needed
            int itemHeight = 45;
            int itemGap = 5;
            int headerHeight = 50;
            int maxBottom = 235;
            int visibleItems = (maxBottom - headerHeight) / (itemHeight + itemGap);
            if (visibleItems < 4) visibleItems = 1;
            
            if (settingsSel >= settingsScroll + visibleItems) {
                settingsScroll = settingsSel - visibleItems + 1;
            }
        }
        else if (settingsState == SET_THEMES) {
             if (settingsSel < themeCount - 1) {
                 settingsSel++;
                  if (settingsSel >= themeScroll + 7) themeScroll++;
             }
        }
        else if (settingsState == SET_MANAGEMENT) {
            int menuCount = 4;
            if (settingsSel < menuCount - 1) {
                settingsSel++;
                
                // Update scroll if needed
                int itemHeight = 45;
                int itemGap = 8;
                int headerHeight = 50;
                int maxBottom = 235;
                int visibleItems = (maxBottom - headerHeight) / (itemHeight + itemGap);
                if (visibleItems < 1) visibleItems = 1;
                
                if (settingsSel >= settingsScroll + visibleItems) {
                    settingsScroll = settingsSel - visibleItems + 1;
                }
            }
        }
        else if (settingsState == SET_AI) {
            String models[24];
            int total = 0;
            if (SD.exists("/")) total = aiScanModels(models, 24);
            int rows = 1 + total;
            if (settingsSel < rows - 1) {
                settingsSel++;
                if (settingsSel >= settingsScroll + 6) settingsScroll++;
            }
        }
        moved = true; delay(150);
    }
    else if (buttonManager.isJustPressed(KEY_UP)) {
        if (settingsState == SET_MAIN) {
            settingsSel = (settingsSel + 4) % 5;
            // Update scroll if needed
            if (settingsSel < settingsScroll) {
                settingsScroll = settingsSel;
            }
        }
        else if (settingsState == SET_THEMES) {
             if (settingsSel > 0) {
                 settingsSel--;
                 if (settingsSel < themeScroll) themeScroll--;
             }
        }
        else if (settingsState == SET_MANAGEMENT) {
            if (settingsSel > 0) {
                settingsSel--;
                if (settingsSel < settingsScroll) {
                    settingsScroll = settingsSel;
                }
            } else {
                settingsSel = 3; // Wrap to last item
            }
        }
        else if (settingsState == SET_AI) {
            String models[24];
            int total = 0;
            if (SD.exists("/")) total = aiScanModels(models, 24);
            int rows = 1 + total;
            if (settingsSel > 0) {
                settingsSel--;
                if (settingsSel < settingsScroll) settingsScroll--;
            } else {
                settingsSel = rows - 1; // Wrap
                settingsScroll = max(0, settingsSel - 6 + 1);
            }
        }
        moved = true; delay(150);
    }
    
    if (moved) {
        bool scrollChanged = (settingsScroll != oldScroll || themeScroll != oldThemeScroll);
        if (scrollChanged) {
            drawSettingsRows();
        } else {
            drawSettingsRow(oldSel, false);
            drawSettingsRow(settingsSel, true);
        }
    }
    
    if (buttonManager.isJustPressed(KEY_START)) {
        if (settingsState == SET_MAIN) {
            if (settingsSel == 0) { toggleWiFi(); drawSettingsApp(); }
            else if (settingsSel == 1) { toggleBT(); drawSettingsApp(); }
            else if (settingsSel == 2) { 
                settingsState = SET_THEMES; 
                settingsSel = 0; 
                themeScroll = 0;
                drawSettingsApp(); 
            }
            else if (settingsSel == 3) { 
                settingsState = SET_MANAGEMENT; 
                settingsSel = 0;
                drawSettingsApp(); 
            }
            else if (settingsSel == 4) { 
                settingsState = SET_AI; 
                settingsSel = 0; 
                settingsScroll = 0;
                drawSettingsApp(); 
            }
        } 
        else if (settingsState == SET_AI) {
            String models[24];
            int total = 0;
            if (SD.exists("/")) total = aiScanModels(models, 24);
            int rows = 1 + total;
            if (settingsSel == 0) {
                aiModelName = AI_MODEL_DEFAULT;
            } else if (settingsSel >= 1 && settingsSel < rows) {
                aiModelName = models[settingsSel - 1];
            }
            saveSettings();
            drawSettingsApp();
        }
        else if (settingsState == SET_THEMES) {
            currentTheme = systemThemes[settingsSel];
            saveAllSettings();
            drawSettingsApp();
        }
        else if (settingsState == SET_MANAGEMENT) {
            if (settingsSel == 0) { // Save Settings
                saveAllSettings();
                SymbianUI::drawMessageScreen("Settings", SymbianUI::ICON_SETTINGS,
                                             "Settings saved", "All changes stored", "", "Done",
                                             SymbianUI::GOOD);
                delay(1500);
                drawSettingsApp();
            }
            else if (settingsSel == 1) { // Restore Default
                SymbianUI::drawChrome("Settings", "Yes", "No");
                SymbianUI::drawDialog("Restore defaults", "Reset all settings?",
                                      "Yes", "No", true,
                                      SymbianUI::ICON_SETTINGS, SymbianUI::BAD);
                
                // Wait for user confirmation
                delay(200);
                while (true) {
                    if (isSelectPressed()) {
                        restoreDefaultSettings();
                        SymbianUI::drawMessageScreen("Settings", SymbianUI::ICON_SETTINGS,
                                                     "Defaults restored", "Settings were reset", "", "Done",
                                                     SymbianUI::WARN);
                        delay(1500);
                        drawSettingsApp();
                        break;
                    }
                    else if (isBackPressed()) {
                        drawSettingsApp();
                        break;
                    }
                    delay(50);
                }
            }
            else if (settingsSel == 2) { // Export Settings
                saveAllSettings(); // Ensure latest settings are saved first
                SymbianUI::drawMessageScreen("Settings", SymbianUI::ICON_FILE,
                                             "Settings exported", "Saved to device", "", "Done",
                                             SymbianUI::GOOD);
                delay(1500);
                drawSettingsApp();
            }
            else if (settingsSel == 3) { // Back
                settingsState = SET_MAIN;
                settingsSel = 3;
                drawSettingsApp();
            }
        }
        delay(300);
    }
}

#endif
