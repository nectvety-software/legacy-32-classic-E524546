#include "include/LegacyOS.h"

// --- Built-in App Implementations (moved from header for link-once) ---
#include <TFT_eSPI.h>
#include <SD.h>
#include <SPIFFS.h>

// ─── Built-in App Implementations ────────────────────────────
// These are included AFTER class LegacyOS is fully defined,
// so they can call ctx.os->setState(), getDevice(), etc.

// ─── Settings App ─────────────────────────────────────────────
namespace AppSettings {
  static int menuIndex = 0;
  void spr_showSysInfo(AppContext& ctx);  // forward decl
  static const char* items[] = {
    "Display", "WiFi", "Bluetooth", "Audio",
    "Storage", "System Info", "About", "Reboot"
  };
  static const int itemCount = 8;
  
  void start(AppContext& ctx) {
    menuIndex = 0;
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(2);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("Settings", 10, 15);
    spr->drawLine(0, 28, 240, 28, CLR_GRAY3);
    
    for (int i = 0; i < itemCount; i++) {
      int y = 35 + i * 32;
      bool sel = (i == menuIndex);
      
      if (sel) {
        spr->fillRoundRect(5, y-2, 230, 28, 5, CLR_PRIMARY);
      }
      
      spr->setTextColor(sel ? CLR_WHITE : CLR_GRAY1);
      spr->setTextSize(1);
      spr->setTextDatum(ML_DATUM);
      spr->drawString(items[i], 16, y + 10);
      
      if (sel) {
        spr->drawString(">", 220, y + 10);
      }
    }
    
    spr->setTextColor(CLR_GRAY2);
    spr->setTextSize(1);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("A:Select  B:Back", 120, 285);
    
    ctx.display->pushContent();
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    
    if (ctx.input->up())   { if (menuIndex > 0) menuIndex--; }
    if (ctx.input->down()) { if (menuIndex < itemCount-1) menuIndex++; }
    if (ctx.input->b())    { ctx.os->setState(OSState::HOME_SCREEN); }
    
    if (ctx.input->start()) {
      if (menuIndex == 7) { ctx.os->reboot(); }
      else if (menuIndex == 5) {
        spr_showSysInfo(ctx);
      }
    }
  }
  
  void spr_showSysInfo(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    spr->setTextColor(CLR_PRIMARY);
    spr->setTextSize(2);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("System Info", 5, 10);
    spr->drawLine(0, 24, 240, 24, CLR_GRAY3);
    
    spr->setTextSize(1);
    spr->setTextColor(CLR_GRAY1);
    
    char buf[64];
    int y = 30;
    auto line = [&](const char* k, const char* v) {
      spr->setTextColor(CLR_GRAY2);
      spr->drawString(k, 5, y);
      spr->setTextColor(CLR_WHITE);
      spr->drawString(v, 110, y);
      y += 18;
    };
    
    line("Device:", ctx.os->getDevice().c_str());
    line("OS:", ctx.os->getVersion().c_str());
    
    snprintf(buf, 64, "%s", ESP.getChipModel());
    line("Chip:", buf);
    
    snprintf(buf, 64, "%d MHz", ESP.getCpuFreqMHz());
    line("CPU Freq:", buf);
    
    snprintf(buf, 64, "%.1f MB", ESP.getFlashChipSize() / 1048576.0f);
    line("Flash:", buf);
    
    snprintf(buf, 64, "%.1f MB", ESP.getPsramSize() / 1048576.0f);
    line("PSRAM:", buf);
    
    snprintf(buf, 64, "%d KB", ESP.getFreeHeap() / 1024);
    line("Free Heap:", buf);
    
    snprintf(buf, 64, "%d KB", ESP.getFreePsram() / 1024);
    line("Free PSRAM:", buf);
    
    snprintf(buf, 64, "%d s", ctx.os->uptime());
    line("Uptime:", buf);
    
    snprintf(buf, 64, "6.x");
    line("SDK:", buf);
    
    spr->setTextColor(CLR_GRAY2);
    spr->drawString("A: Back", 5, 285);
    ctx.display->pushContent();
    
    while (!ctx.input->a()) {
      ctx.input->update();
      delay(16);
    }
  }
}

// ─── File Manager App ──────────────────────────────────────
namespace AppFileManager {
  static String currentPath = "/";
  void _loadDir();  // forward decl
  static int scroll = 0, selected = 0;
  static std::vector<String> entries;
  
  void start(AppContext& ctx) {
    currentPath = "/";
    scroll = selected = 0;
    _loadDir();
  }
  
  void _loadDir() {
    entries.clear();
    if (currentPath != "/") entries.push_back("..");
    
    if (SD.begin()) {
      File dir = SD.open(currentPath.c_str());
      if (dir) {
        File f;
        while ((f = dir.openNextFile())) {
          entries.push_back(f.isDirectory() ? 
            String("[") + f.name() + "]" : f.name());
          f.close();
        }
        dir.close();
      }
    } else {
      entries.push_back("[No SD Card]");
    }
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 22, CLR_BG2);
    spr->setTextColor(CLR_PRIMARY);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("📁 " + currentPath, 5, 11);
    
    int visibleRows = 13;
    for (int i = 0; i < visibleRows && (i + scroll) < (int)entries.size(); i++) {
      int idx = i + scroll;
      int y = 25 + i * 20;
      bool sel = (idx == selected);
      
      if (sel) spr->fillRect(0, y-1, 234, 19, CLR_PRIMARY);
      
      bool isDir = entries[idx].startsWith("[");
      spr->setTextColor(sel ? CLR_WHITE : (isDir ? CLR_CYAN : CLR_GRAY1));
      spr->setTextSize(1);
      spr->setTextDatum(ML_DATUM);
      spr->drawString(entries[idx], 8, y + 6);
    }
    
    UITheme::drawScrollbar(spr, 236, 24, 256, entries.size(), visibleRows, scroll);
    
    spr->fillRect(0, 282, 240, 18, CLR_BG2);
    spr->setTextColor(CLR_GRAY2);
    spr->setTextSize(1);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("START:Open  A:Back", 120, 290);
    
    ctx.display->pushContent();
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    
    if (ctx.input->up())   { if (selected > 0) { selected--; if (selected < scroll) scroll--; }}
    if (ctx.input->down()) { if (selected < (int)entries.size()-1) { selected++; if (selected >= scroll+13) scroll++; }}
    if (ctx.input->b())    { ctx.os->setState(OSState::HOME_SCREEN); }
    if (ctx.input->start()) {
      if (!entries.empty()) {
        if (entries[selected] == "..") {
          int slash = currentPath.lastIndexOf('/');
          if (slash > 0) currentPath = currentPath.substring(0, slash);
          else currentPath = "/";
          selected = scroll = 0;
          _loadDir();
        } else if (entries[selected].startsWith("[")) {
          String dir = entries[selected];
          dir = dir.substring(1, dir.length()-1);
          if (currentPath == "/") currentPath = "/" + dir;
          else currentPath = currentPath + "/" + dir;
          selected = scroll = 0;
          _loadDir();
        }
      }
    }
  }
}

// ─── WiFi Manager App ──────────────────────────────────────
namespace AppWiFi {
  static int    menuSel = 0;
  static int    scanCount = 0;
  static String networks[20];
  static int    rssi[20];
  static bool   scanning = false;
  
  void start(AppContext& ctx) {
    menuSel = 0;
    scanning = false;
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(2);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("WiFi Manager", 5, 10);
    spr->drawLine(0, 26, 240, 26, CLR_GRAY3);
    
    bool connected = WiFi.status() == WL_CONNECTED;
    
    spr->fillRoundRect(5, 30, 230, 40, 8, CLR_BG2);
    spr->setTextColor(connected ? CLR_GREEN : CLR_ACCENT);
    spr->setTextSize(1);
    spr->drawString(connected ? "● Connected" : "○ Disconnected", 12, 42);
    if (connected) {
      spr->setTextColor(CLR_GRAY1);
      spr->drawString("IP: " + WiFi.localIP().toString(), 12, 55);
    }
    
    spr->setTextColor(CLR_GRAY2);
    spr->drawString(scanning ? "Scanning..." : "Networks:", 5, 82);
    
    for (int i = 0; i < min(scanCount, 8); i++) {
      int y = 94 + i * 22;
      bool sel = (i == menuSel);
      if (sel) spr->fillRoundRect(3, y-2, 234, 20, 4, CLR_BG2);
      spr->setTextColor(sel ? CLR_WHITE : CLR_GRAY1);
      spr->drawString(networks[i], 8, y + 6);
      
      int bars = map(rssi[i], -100, -50, 0, 4);
      bars = constrain(bars, 0, 4);
      UITheme::drawWifiIcon(spr, 210, y + 4, bars);
    }
    
    spr->fillRect(0, 282, 240, 18, CLR_BG2);
    spr->setTextColor(CLR_GRAY2);
    spr->setTextSize(1);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("SELECT:Scan  A:Connect  B:Back", 120, 290);
    
    ctx.display->pushContent();
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    
    if (ctx.input->up())     { if (menuSel > 0) menuSel--; }
    if (ctx.input->down())   { if (menuSel < scanCount-1) menuSel++; }
    if (ctx.input->b())      { WiFi.disconnect(); ctx.os->setState(OSState::HOME_SCREEN); }
    if (ctx.input->select()) {
      scanning = true;
      render(ctx);
      scanCount = WiFi.scanNetworks();
      for (int i = 0; i < scanCount && i < 20; i++) {
        networks[i] = WiFi.SSID(i);
        rssi[i]     = WiFi.RSSI(i);
      }
      scanning = false;
    }
  }
}

// ─── BLE App ───────────────────────────────────────────────
namespace AppBLE {
  static bool bleStarted = false;
  static std::vector<String> devices;
  static int menuSel = 0;
  
  void start(AppContext& ctx) {
    menuSel = 0;
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->setTextColor(CLR_CYAN);
    spr->setTextSize(2);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("Bluetooth", 5, 10);
    spr->drawLine(0, 26, 240, 26, CLR_GRAY3);
    
    spr->fillRoundRect(5, 30, 230, 35, 8, CLR_BG2);
    spr->setTextColor(bleStarted ? CLR_CYAN : CLR_GRAY2);
    spr->setTextSize(1);
    spr->drawString(bleStarted ? "BLE: Active" : "BLE: Off", 12, 42);
    spr->setTextColor(CLR_GRAY2);
    spr->drawString("Device: LEGACY-32-E524546", 12, 54);
    
    spr->setTextColor(CLR_GRAY2);
    spr->drawString("Discovered Devices:", 5, 78);
    
    for (int i = 0; i < (int)devices.size() && i < 8; i++) {
      int y = 90 + i * 22;
      bool sel = (i == menuSel);
      if (sel) spr->fillRoundRect(3, y-2, 234, 20, 4, CLR_BG2);
      spr->setTextColor(sel ? CLR_WHITE : CLR_GRAY1);
      spr->drawString(devices[i], 10, y + 6);
    }
    
    if (devices.empty()) {
      spr->setTextColor(CLR_GRAY3);
      spr->setTextDatum(MC_DATUM);
      spr->drawString("Press SELECT to scan", 120, 160);
    }
    
    spr->fillRect(0, 282, 240, 18, CLR_BG2);
    spr->setTextColor(CLR_GRAY2);
    spr->setTextSize(1);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("START:Scan  OPTION:BLE  A:Back", 120, 290);
    ctx.display->pushContent();
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    if (ctx.input->up())   { if (menuSel > 0) menuSel--; }
    if (ctx.input->down()) { if (menuSel < (int)devices.size()-1) menuSel++; }
    if (ctx.input->b())    { ctx.os->setState(OSState::HOME_SCREEN); }
    if (ctx.input->option()) {
      if (!bleStarted) {
        BLEDevice::init("LEGACY-32-E524546");
        bleStarted = true;
      } else {
        BLEDevice::deinit(true);
        bleStarted = false;
      }
    }
    if (ctx.input->start() && bleStarted) {
      BLEScan* scan = BLEDevice::getScan();
      scan->setActiveScan(true);
      BLEScanResults* results = scan->start(3);
      devices.clear();
      for (int i = 0; i < results->getCount(); i++) {
        BLEAdvertisedDevice d = results->getDevice(i);
        String name = d.getName().c_str();
        if (name.isEmpty()) name = d.getAddress().toString().c_str();
        devices.push_back(name);
      }
    }
  }
}

// ─── Calculator App ────────────────────────────────────────
namespace AppCalc {
  static String display_val = "0";
  static double operand1 = 0, operand2 = 0;
  static char   op = 0;
  static bool   newNum = true;
  static int    btnIdx = 0;
  
  static const char* btns[] = {
    "7","8","9","/",
    "4","5","6","*",
    "1","2","3","-",
    "0",".","=","+"
  };
  
  void start(AppContext& ctx) {
    display_val = "0";
    operand1 = operand2 = 0;
    op = 0; newNum = true; btnIdx = 0;
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 22, CLR_BG2);
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(1);
    spr->drawString("Calculator", 5, 12);
    
    spr->fillRect(0, 25, 240, 45, 0x0841);
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(3);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(display_val, 235, 48);
    
    for (int i = 0; i < 16; i++) {
      int row = i / 4, col = i % 4;
      int x = 5 + col * 58, y = 78 + row * 52;
      bool sel = (i == btnIdx);
      
      uint16_t bg = sel ? CLR_PRIMARY : (i == 14 ? CLR_GREEN : CLR_BG2);
      spr->fillRoundRect(x, y, 52, 44, 6, bg);
      spr->setTextColor(sel ? CLR_WHITE : 
        ((i >= 12 && i != 14) ? CLR_ORANGE : CLR_WHITE));
      spr->setTextSize(2);
      spr->setTextDatum(MC_DATUM);
      spr->drawString(btns[i], x+26, y+22);
    }
    
    spr->fillRect(0, 290, 240, 10, CLR_BG2);
    spr->setTextColor(CLR_GRAY3);
    spr->setTextSize(1);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("DPAD:Move  A:Select  B:Back", 120, 295);
    
    ctx.display->pushContent();
  }
  
  void pressBtn(int idx) {
    const char* b = btns[idx];
    if (b[0] >= '0' && b[0] <= '9') {
      if (newNum) { display_val = b; newNum = false; }
      else if (display_val.length() < 10) display_val += b;
    } else if (b[0] == '.') {
      if (display_val.indexOf('.') < 0) display_val += '.';
    } else if (b[0] == '=') {
      operand2 = display_val.toDouble();
      double res = 0;
      switch(op) {
        case '+': res = operand1 + operand2; break;
        case '-': res = operand1 - operand2; break;
        case '*': res = operand1 * operand2; break;
        case '/': res = operand2 != 0 ? operand1/operand2 : 0; break;
      }
      display_val = String(res, res == (int)res ? 0 : 4);
      newNum = true;
    } else {
      operand1 = display_val.toDouble();
      op = b[0];
      newNum = true;
    }
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    int cols = 4;
    if (ctx.input->up())    { if (btnIdx >= cols) btnIdx -= cols; }
    if (ctx.input->down())  { if (btnIdx < 12) btnIdx += cols; }
    if (ctx.input->left())  { if (btnIdx % cols > 0) btnIdx--; }
    if (ctx.input->right()) { if (btnIdx % cols < cols-1) btnIdx++; }
    if (ctx.input->start())     { pressBtn(btnIdx); }
    if (ctx.input->b())     { ctx.os->setState(OSState::HOME_SCREEN); }
    if (ctx.input->b())  { display_val = "0"; operand1 = 0; op = 0; newNum = true; }
  }
}

// ─── Terminal App ──────────────────────────────────────────
namespace AppTerminal {
  static std::vector<String> lines;
  static int scroll = 0;
  
  void print(const String& s) {
    lines.push_back(s);
    if (lines.size() > 200) lines.erase(lines.begin());
    scroll = max(0, (int)lines.size() - 13);
  }
  
  void start(AppContext& ctx) {
    lines.clear();
    scroll = 0;
    print("LegacyOS Terminal v1.0");
    print("Device: " + ctx.os->getDevice());
    print("Type: help");
    print("");
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BLACK);
    
    spr->fillRect(0, 0, 240, 14, CLR_GRAY3);
    spr->setTextColor(CLR_GREEN);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("$ Terminal", 4, 8);
    
    spr->setTextColor(CLR_GREEN);
    spr->setTextSize(1);
    for (int i = 0; i < 13 && (i + scroll) < (int)lines.size(); i++) {
      spr->drawString(lines[i + scroll], 4, 20 + i * 20);
    }
    
    int cy = 20 + min(13, (int)lines.size()) * 20;
    spr->setTextColor(CLR_GREEN);
    static bool blink = false;
    static uint32_t bt = 0;
    if (millis() - bt > 500) { blink = !blink; bt = millis(); }
    if (blink) spr->fillRect(4, cy, 6, 12, CLR_GREEN);
    
    spr->fillRect(0, 285, 240, 15, CLR_GRAY3);
    spr->setTextColor(CLR_GRAY1);
    spr->setTextSize(1);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("UP/DN:Scroll  START:Cmd  A:Exit", 120, 292);
    ctx.display->pushContent();
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    if (ctx.input->up())   { if (scroll > 0) scroll--; }
    if (ctx.input->down()) { if (scroll < (int)lines.size()-13) scroll++; }
    if (ctx.input->b())    { ctx.os->setState(OSState::HOME_SCREEN); }
    if (ctx.input->start()) {
      print("$ status");
      char buf[60];
      snprintf(buf, 60, "Heap: %d KB | PSRAM: %d KB",
        ESP.getFreeHeap()/1024, ESP.getFreePsram()/1024);
      print(buf);
      snprintf(buf, 60, "Uptime: %d s | CPU: %d MHz",
        ctx.os->uptime(), ESP.getCpuFreqMHz());
      print(buf);
      scroll = max(0, (int)lines.size() - 13);
    }
  }
}

// ─── Lua IDE App ─────────────────────────────────────────────
namespace AppLuaIDE {
  static std::vector<String> output;
  static int scroll = 0;
  static int fileScroll = 0, fileSelected = 0;
  static std::vector<String> luaFiles;
  static bool showOutput = false;
  
  void _scanFiles() {
    luaFiles.clear();
    if (SD.begin()) {
      File dir = SD.open("/scripts/lua");
      if (dir) {
        File f;
        while ((f = dir.openNextFile())) {
          String name = f.name();
          if (name.endsWith(".lua")) luaFiles.push_back(name);
          f.close();
        }
        dir.close();
      }
    }
    if (luaFiles.empty()) luaFiles.push_back("(No .lua files on SD)");
  }
  
  void start(AppContext& ctx) {
    output.clear();
    scroll = fileScroll = fileSelected = 0;
    showOutput = false;
    _scanFiles();
  }
  
  void loop(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 18, CLR_APP_YELLOW);
    spr->setTextColor(CLR_BLACK);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("Lua IDE", 5, 10);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(showOutput ? "Output" : "Files", 235, 10);
    
    if (!showOutput) {
      spr->setTextColor(CLR_GRAY2);
      spr->setTextSize(1);
      spr->drawString("SD: /scripts/lua/", 5, 25);
      
      for (int i = 0; i < 12 && (i + fileScroll) < (int)luaFiles.size(); i++) {
        int idx = i + fileScroll;
        int y = 33 + i * 22;
        bool sel = (idx == fileSelected);
        if (sel) spr->fillRoundRect(3, y-1, 234, 20, 4, CLR_APP_YELLOW);
        spr->setTextColor(sel ? CLR_BLACK : CLR_GRAY1);
        spr->drawString(luaFiles[idx], 8, y + 7);
      }
      
      spr->fillRect(0, 282, 240, 18, CLR_BG2);
      spr->setTextColor(CLR_GRAY2);
      spr->setTextDatum(MC_DATUM);
      spr->drawString("START:Run  OPTION:Rescan  A:Back", 120, 290);
    } else {
      spr->setTextColor(CLR_GREEN);
      spr->setTextSize(1);
      for (int i = 0; i < 14 && (i + scroll) < (int)output.size(); i++) {
        spr->drawString(output[i + scroll], 4, 22 + i * 19);
      }
      spr->fillRect(0, 282, 240, 18, CLR_BG2);
      spr->setTextColor(CLR_GRAY2);
      spr->setTextDatum(MC_DATUM);
      spr->drawString("UP/DN:Scroll  B:Back to files", 120, 290);
    }
    
    ctx.display->pushContent();
    
    if (!showOutput) {
      if (ctx.input->up())   { if (fileSelected > 0) { fileSelected--; if (fileSelected < fileScroll) fileScroll--; }}
      if (ctx.input->down()) { if (fileSelected < (int)luaFiles.size()-1) { fileSelected++; if (fileSelected >= fileScroll+12) fileScroll++; }}
      if (ctx.input->option()) { _scanFiles(); }
      if (ctx.input->b())    { ctx.os->setState(OSState::HOME_SCREEN); }
      if (ctx.input->start() && !luaFiles[fileSelected].startsWith("(")) {
        output.clear();
        output.push_back("Running: " + luaFiles[fileSelected]);
        output.push_back("---");
        
        ctx.lua->onPrint = [&](const String& s) { output.push_back(s); };
        
        String path = "/scripts/lua/" + luaFiles[fileSelected];
        LuaResult r = ctx.lua->executeFile(path);
        
        output.push_back("---");
        output.push_back(r.success ? "OK" : "ERR: " + r.error);
        output.push_back("Time: " + String(r.execTime, 3) + "s");
        
        showOutput = true;
        scroll = 0;
      }
    } else {
      if (ctx.input->up())   { if (scroll > 0) scroll--; }
      if (ctx.input->down()) { if (scroll < (int)output.size()-14) scroll++; }
      if (ctx.input->a())    { showOutput = false; }
    }
  }
}

// ─── JS Runner App ────────────────────────────────────────
namespace AppJSRun {
  static std::vector<String> output;
  static int scroll = 0, fileScroll = 0, fileSelected = 0;
  static std::vector<String> jsFiles;
  static bool showOutput = false;
  
  void _scanFiles() {
    jsFiles.clear();
    if (SD.begin()) {
      File dir = SD.open("/scripts/js");
      if (dir) {
        File f;
        while ((f = dir.openNextFile())) {
          String n = f.name();
          if (n.endsWith(".js")) jsFiles.push_back(n);
          f.close();
        }
        dir.close();
      }
    }
    if (jsFiles.empty()) jsFiles.push_back("(No .js files on SD)");
  }
  
  void start(AppContext& ctx) {
    output.clear();
    scroll = fileScroll = fileSelected = 0;
    showOutput = false;
    _scanFiles();
  }
  
  void loop(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 18, CLR_APP_GREEN);
    spr->setTextColor(CLR_BLACK);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("JS Runner", 5, 10);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(showOutput ? "Output" : "Files", 235, 10);
    
    if (!showOutput) {
      spr->setTextColor(CLR_GRAY2);
      spr->setTextDatum(ML_DATUM);
      spr->drawString("SD: /scripts/js/", 5, 25);
      
      for (int i = 0; i < 12 && (i + fileScroll) < (int)jsFiles.size(); i++) {
        int idx = i + fileScroll;
        int y = 33 + i * 22;
        bool sel = (idx == fileSelected);
        if (sel) spr->fillRoundRect(3, y-1, 234, 20, 4, CLR_APP_GREEN);
        spr->setTextColor(sel ? CLR_BLACK : CLR_GRAY1);
        spr->drawString(jsFiles[idx], 8, y + 7);
      }
      
      spr->fillRect(0, 282, 240, 18, CLR_BG2);
      spr->setTextColor(CLR_GRAY2);
      spr->setTextDatum(MC_DATUM);
      spr->drawString("START:Run  OPTION:Rescan  A:Back", 120, 290);
    } else {
      spr->setTextColor(CLR_APP_GREEN);
      for (int i = 0; i < 14 && (i + scroll) < (int)output.size(); i++) {
        spr->drawString(output[i + scroll], 4, 22 + i * 19);
      }
      spr->fillRect(0, 282, 240, 18, CLR_BG2);
      spr->setTextColor(CLR_GRAY2);
      spr->setTextDatum(MC_DATUM);
      spr->drawString("UP/DN:Scroll  B:Back", 120, 290);
    }
    
    ctx.display->pushContent();
    
    if (!showOutput) {
      if (ctx.input->up())   { if (fileSelected > 0) { fileSelected--; if (fileSelected < fileScroll) fileScroll--; }}
      if (ctx.input->down()) { if (fileSelected < (int)jsFiles.size()-1) { fileSelected++; if (fileSelected >= fileScroll+12) fileScroll++; }}
      if (ctx.input->option()) { _scanFiles(); }
      if (ctx.input->b())    { ctx.os->setState(OSState::HOME_SCREEN); }
      if (ctx.input->start() && !jsFiles[fileSelected].startsWith("(")) {
        output.clear();
        output.push_back("Running: " + jsFiles[fileSelected]);
        output.push_back("---");
        
        ctx.js->onPrint = [&](const String& s) { output.push_back(s); };
        
        String path = "/scripts/js/" + jsFiles[fileSelected];
        JSResult r = ctx.js->executeFile(path);
        
        output.push_back("---");
        output.push_back(r.success ? "Done" : "ERR: " + r.error);
        output.push_back("Time: " + String(r.execTime, 3) + "s");
        
        showOutput = true; scroll = 0;
      }
    } else {
      if (ctx.input->up())   { if (scroll > 0) scroll--; }
      if (ctx.input->down()) { if (scroll < (int)output.size()-14) scroll++; }
      if (ctx.input->a())    { showOutput = false; }
    }
  }
}

// ─── Retro Emulator App ──────────────────────────────────
namespace AppRetroEmu {
  static std::vector<RomInfo> roms;
  static int scroll = 0, selected = 0;
  static bool emulating = false;
  static bool scanning  = false;
  static EmuType filterType = EmuType::NONE;
  
  void start(AppContext& ctx) {
    scroll = selected = 0;
    emulating = false;
    scanning  = true;
    
    roms = ctx.retro->scanROMs();
    scanning = false;
    
    if (roms.empty()) {
      RomInfo placeholder;
      placeholder.name = "No ROMs found";
      placeholder.path = "";
      placeholder.type = EmuType::NONE;
      placeholder.size = 0;
      roms.push_back(placeholder);
    }
    
    Serial.printf("[RetroEmu] Found %d ROMs\n", roms.size());
  }
  
  void renderROMList(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 22, CLR_APP_RED);
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("Retro Emulator", 5, 12);
    spr->setTextDatum(MR_DATUM);
    char cntBuf[12];
    snprintf(cntBuf, 12, "%d ROMs", (int)roms.size());
    spr->drawString(cntBuf, 235, 12);
    
    const char* tabs[] = {"ALL","GB","NES","GBA"};
    EmuType tabTypes[] = {EmuType::NONE, EmuType::GAMEBOY, EmuType::NES, EmuType::GBA};
    for (int i = 0; i < 4; i++) {
      int tx = 5 + i * 58;
      bool active = (filterType == tabTypes[i]);
      spr->fillRoundRect(tx, 24, 54, 14, 3, active ? CLR_APP_RED : CLR_BG2);
      spr->setTextColor(active ? CLR_WHITE : CLR_GRAY2);
      spr->setTextDatum(MC_DATUM);
      spr->drawString(tabs[i], tx+27, 30);
    }
    
    int visRows = 11;
    for (int i = 0; i < visRows && (i + scroll) < (int)roms.size(); i++) {
      int idx = i + scroll;
      int y = 42 + i * 22;
      bool sel = (idx == selected);
      
      const RomInfo& r = roms[idx];
      if (sel) spr->fillRoundRect(3, y-1, 234, 20, 4, CLR_APP_RED);
      
      const char* badge = "??";
      uint16_t badgeC = CLR_GRAY2;
      switch(r.type) {
        case EmuType::GAMEBOY: badge = "GB";  badgeC = CLR_APP_GREEN;  break;
        case EmuType::NES:     badge = "NES"; badgeC = CLR_APP_RED;    break;
        case EmuType::GBA:     badge = "GBA"; badgeC = CLR_APP_PURPLE; break;
        default: break;
      }
      spr->fillRoundRect(y+3-42+3, y+2, 24, 14, 3, sel ? CLR_WHITE : badgeC);
      spr->setTextColor(sel ? badgeC : CLR_WHITE);
      spr->setTextSize(1);
      spr->setTextDatum(MC_DATUM);
      spr->drawString(badge, y+3-42+3+12, y+9);
      
      spr->setTextColor(sel ? CLR_WHITE : CLR_GRAY1);
      spr->setTextDatum(ML_DATUM);
      spr->drawString(r.name, 35, y + 8);
      
      if (r.size > 0) {
        char sz[12];
        snprintf(sz, 12, "%dK", r.size/1024);
        spr->setTextColor(sel ? CLR_WHITE : CLR_GRAY3);
        spr->setTextDatum(MR_DATUM);
        spr->drawString(sz, 233, y + 8);
      }
    }
    
    UITheme::drawScrollbar(spr, 236, 42, 238, roms.size(), visRows, scroll);
    
    spr->fillRect(0, 284, 240, 16, CLR_BG2);
    spr->setTextColor(CLR_GRAY2);
    spr->setTextSize(1);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("START:Launch  OPTION:Rescan  A:Exit", 120, 291);
    
    ctx.display->pushContent();
  }
  
  void renderEmulating(AppContext& ctx) {
    ctx.display->tft.fillScreen(CLR_BLACK);
    
    if (!ctx.retro->running) {
      ctx.display->tft.setTextColor(CLR_WHITE);
      ctx.display->tft.setTextSize(2);
      ctx.display->tft.setTextDatum(MC_DATUM);
      ctx.display->tft.drawString(ctx.retro->currentROM.name, 120, 130);
      ctx.display->tft.setTextSize(1);
      ctx.display->tft.setTextColor(CLR_GRAY2);
      ctx.display->tft.drawString(RetroEmu::emuName(ctx.retro->activeEmu), 120, 155);
      ctx.display->tft.drawString("Install emulator library", 120, 170);
      ctx.display->tft.drawString("for full emulation", 120, 183);
      ctx.display->tft.setTextColor(CLR_GRAY3);
      ctx.display->tft.drawString("A: Exit emulator", 120, 220);
    }
  }
  
  void loop(AppContext& ctx) {
    if (!emulating) {
      renderROMList(ctx);
      
      if (ctx.input->up())   { if (selected > 0) { selected--; if (selected < scroll) scroll--; }}
      if (ctx.input->down()) { if (selected < (int)roms.size()-1) { selected++; if (selected >= scroll+11) scroll++; }}
      if (ctx.input->b())    { ctx.os->setState(OSState::HOME_SCREEN); }
      if (ctx.input->option()) { roms = ctx.retro->scanROMs(); selected = scroll = 0; }
      if (ctx.input->start() && roms[selected].path.length() > 0) {
        if (ctx.retro->loadROM(roms[selected])) {
          ctx.retro->startEmulation(&ctx.display->tft);
          emulating = true;
        }
      }
    } else {
      uint8_t btns = ctx.retro->getButtonState(ctx.input);
      ctx.retro->tick(&ctx.display->tft, btns);
      renderEmulating(ctx);
      
      if (ctx.input->a()) {
        ctx.retro->stop();
        emulating = false;
        HW::setBacklight(BACKLIGHT_MAX);
        ctx.display->tft.fillScreen(CLR_BG);
      }
      delay(16);
    }
  }
}

// ─── LoRa App ────────────────────────────────────────────────
namespace AppLoRa {
  static LoRaManager lora;
  static std::vector<String> log;
  static int  scroll = 0, menuSel = 0;
  static bool showLog = true;
  static String txMsg = "Hello LoRa!";
  
  void start(AppContext& ctx) {
    log.clear(); scroll = 0; menuSel = 0;
    lora.init();
    log.push_back("LoRa Radio " + String(LORA_FREQ) + " MHz");
    log.push_back(lora.available ? "Status: Ready" : "Status: No module");
    log.push_back("Connect SX1276/78 module");
    log.push_back("and set pins in LoRaManager.h");
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 20, CLR_APP_ORANGE);
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("LoRa Radio", 5, 11);
    spr->setTextColor(lora.available ? CLR_GREEN : CLR_ACCENT);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(lora.available ? "●READY" : "○OFFLINE", 235, 11);
    
    spr->fillRect(0, 21, 240, 16, CLR_BG2);
    spr->setTextColor(CLR_GRAY2);
    spr->setTextDatum(MC_DATUM);
    spr->setTextSize(1);
    spr->drawString(lora.getStatus(), 120, 28);
    
    spr->fillRect(0, 38, 240, 210, CLR_BLACK);
    spr->setTextColor(CLR_APP_ORANGE);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    for (int i = 0; i < 11 && (i + scroll) < (int)log.size(); i++) {
      bool isTx = log[i+scroll].startsWith("TX:");
      bool isRx = log[i+scroll].startsWith("RX:");
      spr->setTextColor(isTx ? CLR_CYAN : isRx ? CLR_GREEN : CLR_GRAY2);
      spr->drawString(log[i + scroll], 4, 44 + i * 18);
    }
    
    spr->fillRoundRect(3, 252, 234, 20, 4, CLR_BG2);
    spr->setTextColor(CLR_WHITE);
    spr->setTextDatum(ML_DATUM);
    spr->drawString(">" + txMsg, 8, 261);
    
    spr->fillRect(0, 275, 240, 25, CLR_BG2);
    spr->setTextColor(CLR_GRAY2);
    spr->setTextSize(1);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("START:TX  A:Back", 120, 287);
    
    ctx.display->pushContent();
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    
    if (ctx.input->up())   { if (scroll > 0) scroll--; }
    if (ctx.input->down()) { if (scroll < (int)log.size()-11) scroll++; }
    if (ctx.input->b())    { ctx.os->setState(OSState::HOME_SCREEN); }
    
    if (ctx.input->start()) {
      if (lora.available) {
        lora.send(txMsg);
        log.push_back("TX: " + txMsg);
      } else {
        log.push_back("ERR: No LoRa module");
      }
      scroll = max(0, (int)log.size() - 11);
    }
    
    LoRaPacket pkt;
    if (lora.receive(pkt)) {
      log.push_back("RX[" + String(pkt.rssi) + "dBm]: " + pkt.data);
      scroll = max(0, (int)log.size() - 11);
    }
  }
}

// ─── System Monitor App ─────────────────────────────────────
namespace AppSysMon {
  static uint8_t  heapHistory[60]   = {};
  static uint8_t  psramHistory[60]  = {};
  static uint8_t  batHistory[60]    = {};
  static int      histHead = 0;
  static uint32_t lastSample = 0;
  
  static int  page = 0;

  void _sample(AppContext& ctx) {
    if (millis() - lastSample < 1000) return;
    lastSample = millis();
    
    size_t heapMax  = 320 * 1024;
    size_t psramMax = 8 * 1024 * 1024;
    
    heapHistory[histHead]  = (uint8_t)((float)ESP.getFreeHeap()  / heapMax  * 100);
    psramHistory[histHead] = (uint8_t)((float)ESP.getFreePsram() / psramMax * 100);
    batHistory[histHead]   = (uint8_t)ctx.os->getBattery();
    histHead = (histHead + 1) % 60;
  }
  
  void _drawGraph(TFT_eSprite* spr, uint8_t* data, int ox, int oy, int w, int h,
                  uint16_t lineC, const char* label) {
    spr->drawRect(ox, oy, w, h, CLR_GRAY3);
    spr->setTextColor(CLR_GRAY2);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString(label, ox + 2, oy + 2);
    
    for (int g = 1; g < 4; g++) {
      int gy = oy + h * g / 4;
      for (int gx = ox; gx < ox + w; gx += 4) spr->drawPixel(gx, gy, CLR_GRAY3);
    }
    
    int prev = -1;
    for (int i = 0; i < 60; i++) {
      int idx = (histHead + i) % 60;
      int px = ox + i * w / 60;
      int py = oy + h - (data[idx] * h / 100) - 1;
      if (prev >= 0) spr->drawLine(ox + (i-1)*w/60, prev, px, py, lineC);
      prev = py;
    }
    
    int curIdx = (histHead + 59) % 60;
    char buf[8]; snprintf(buf, 8, "%d%%", data[curIdx]);
    spr->setTextColor(lineC);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(buf, ox + w - 2, oy + 2);
  }
  
  void renderOverview(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 20, CLR_APP_TEAL);
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("System Monitor", 5, 11);
    
    uint32_t up = ctx.os->uptime();
    char upBuf[24];
    snprintf(upBuf, 24, "Up: %02d:%02d:%02d", up/3600, (up%3600)/60, up%60);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(upBuf, 235, 11);
    
    _drawGraph(spr, heapHistory,  5, 25, 225, 50, CLR_CYAN,   "Heap Free");
    _drawGraph(spr, psramHistory, 5, 82, 225, 50, CLR_PRIMARY,"PSRAM Free");
    _drawGraph(spr, batHistory,   5,139, 225, 50, CLR_GREEN,  "Battery");
    
    spr->fillRect(0, 195, 240, 1, CLR_GRAY3);
    
    auto stat = [&](int col, int row, const char* k, const char* v, uint16_t vc) {
      int x = col * 120 + 5, y = 200 + row * 26;
      spr->setTextColor(CLR_GRAY2); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
      spr->drawString(k, x, y);
      spr->setTextColor(vc);
      spr->drawString(v, x, y + 11);
    };
    
    char buf[20];
    snprintf(buf, 20, "%d MHz", ESP.getCpuFreqMHz());
    stat(0, 0, "CPU Freq", buf, CLR_CYAN);
    
    snprintf(buf, 20, "%s", ESP.getChipModel());
    stat(1, 0, "Chip", buf, CLR_WHITE);
    
    snprintf(buf, 20, "%d KB", ESP.getFreeHeap()/1024);
    stat(0, 1, "Free Heap", buf, CLR_GREEN);
    
    snprintf(buf, 20, "%d KB", ESP.getFreePsram()/1024);
    stat(1, 1, "Free PSRAM", buf, CLR_PRIMARY);
    
    snprintf(buf, 20, "%d%%", ctx.os->getBattery());
    stat(0, 2, "Battery", buf, CLR_YELLOW);
    
    snprintf(buf, 20, "%d", uxTaskGetNumberOfTasks());
    stat(1, 2, "FreeRTOS Tasks", buf, CLR_ORANGE);
    
    spr->fillRect(0, 280, 240, 20, CLR_BG2);
    for (int p = 0; p < 4; p++) {
      spr->fillCircle(100 + p*12, 289, p == page ? 4 : 2, 
                      p == page ? CLR_APP_TEAL : CLR_GRAY3);
    }
    spr->setTextColor(CLR_GRAY2); spr->setTextDatum(MC_DATUM);
    spr->drawString("L/R:Page  B:Exit", 120, 298);
    
    ctx.display->pushContent();
  }
  
  void renderNetwork(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 20, CLR_APP_BLUE);
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString("Network Status", 5, 11);
    
    auto netRow = [&](int y, const char* k, const String& v, uint16_t c) {
      spr->setTextColor(CLR_GRAY2); spr->setTextDatum(ML_DATUM);
      spr->drawString(k, 8, y);
      spr->setTextColor(c);
      spr->drawString(v, 110, y);
    };
    
    int y = 30;
    bool wok = WiFi.status() == WL_CONNECTED;
    netRow(y, "WiFi Status:", wok ? "Connected" : "Offline", wok ? CLR_GREEN : CLR_ACCENT); y+=20;
    netRow(y, "SSID:", wok ? WiFi.SSID() : "---", CLR_WHITE); y+=20;
    netRow(y, "IP Address:", wok ? WiFi.localIP().toString() : "---", CLR_CYAN); y+=20;
    netRow(y, "Gateway:", wok ? WiFi.gatewayIP().toString() : "---", CLR_GRAY1); y+=20;
    netRow(y, "DNS:", wok ? WiFi.dnsIP().toString() : "---", CLR_GRAY1); y+=20;
    netRow(y, "RSSI:", wok ? String(WiFi.RSSI())+" dBm" : "---", CLR_YELLOW); y+=20;
    netRow(y, "MAC:", WiFi.macAddress(), CLR_GRAY2); y+=20;
    
    bool bok = ctx.network->bleEnabled;
    netRow(y, "BLE:", bok ? "Active" : "Off", bok ? CLR_CYAN : CLR_GRAY3); y+=20;
    if (bok) {
      netRow(y, "BLE Device:", "LEGACY-32-E524546", CLR_GRAY1); y+=20;
      netRow(y, "BLE Paired:", ctx.network->bleConnected ? "Yes" : "No",
             ctx.network->bleConnected ? CLR_GREEN : CLR_GRAY3);
    }
    
    spr->fillRect(0, 280, 240, 20, CLR_BG2);
    for (int p = 0; p < 4; p++)
      spr->fillCircle(100 + p*12, 289, p == page ? 4 : 2, p == page ? CLR_APP_BLUE : CLR_GRAY3);
    spr->setTextColor(CLR_GRAY2); spr->setTextDatum(MC_DATUM);
    spr->drawString("L/R:Page  B:Exit", 120, 298);
    
    ctx.display->pushContent();
  }
  
  void renderTasks(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 20, CLR_APP_PURPLE);
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString("FreeRTOS Tasks", 5, 11);
    char ntbuf[10]; snprintf(ntbuf, 10, "%d", uxTaskGetNumberOfTasks());
    spr->setTextDatum(MR_DATUM); spr->drawString(ntbuf, 235, 11);
    
    UBaseType_t nt = uxTaskGetNumberOfTasks();
    TaskStatus_t* tasks = (TaskStatus_t*)pvPortMalloc(nt * sizeof(TaskStatus_t));
    if (tasks) {
      uint32_t totalRuntime;
      nt = uxTaskGetSystemState(tasks, nt, &totalRuntime);
      
      spr->setTextColor(CLR_GRAY3);
      spr->drawString("NAME          PRI  STACK  STATE", 4, 24);
      spr->drawLine(0, 32, 240, 32, CLR_GRAY3);
      
      for (UBaseType_t i = 0; i < nt && i < 11; i++) {
        int y = 36 + i * 22;
        bool running = tasks[i].eCurrentState == eRunning;
        
        if (running) spr->fillRect(0, y-2, 240, 20, CLR_BG2);
        
        char row[40];
        const char* stateStr = "?";
        switch(tasks[i].eCurrentState) {
          case eRunning:   stateStr = "RUN"; break;
          case eReady:     stateStr = "RDY"; break;
          case eBlocked:   stateStr = "BLK"; break;
          case eSuspended: stateStr = "SUS"; break;
          case eDeleted:   stateStr = "DEL"; break;
        }
        snprintf(row, 40, "%-13s%3d %6d  %s",
          tasks[i].pcTaskName,
          tasks[i].uxCurrentPriority,
          tasks[i].usStackHighWaterMark,
          stateStr);
        
        spr->setTextColor(running ? CLR_GREEN : CLR_GRAY1);
        spr->setTextDatum(ML_DATUM);
        spr->drawString(row, 4, y + 8);
      }
      vPortFree(tasks);
    } else {
      spr->setTextColor(CLR_ACCENT);
      spr->setTextDatum(MC_DATUM);
      spr->drawString("Malloc failed", 120, 140);
    }
    
    spr->fillRect(0, 280, 240, 20, CLR_BG2);
    for (int p = 0; p < 4; p++)
      spr->fillCircle(100 + p*12, 289, p == page ? 4 : 2, p == page ? CLR_APP_PURPLE : CLR_GRAY3);
    ctx.display->pushContent();
  }
  
  void renderMemory(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 20, CLR_APP_ORANGE);
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString("Memory Detail", 5, 11);
    
    auto memBar = [&](int y, const char* label, uint64_t used, uint64_t total, uint16_t c) {
      spr->setTextColor(CLR_GRAY2); spr->setTextDatum(ML_DATUM);
      spr->drawString(label, 5, y);
      char buf[30];
      snprintf(buf, 30, "%llu / %llu KB", used/1024, total/1024);
      spr->setTextColor(CLR_WHITE);
      spr->drawString(buf, 5, y + 12);
      int pct = (int)(used * 100 / max(total, (uint64_t)1));
      uint16_t bc = pct > 80 ? CLR_ACCENT : pct > 60 ? CLR_YELLOW : c;
      UITheme::drawProgressBar(spr, 5, y+24, 230, 10, pct, bc, CLR_GRAY3);
      char pp[6]; snprintf(pp, 6, "%d%%", pct);
      spr->setTextColor(CLR_WHITE); spr->setTextDatum(MR_DATUM);
      spr->drawString(pp, 235, y + 12);
    };
    
    memBar(26,  "Heap (DRAM)",
      ESP.getHeapSize() - ESP.getFreeHeap(), ESP.getHeapSize(), CLR_CYAN);
    
    memBar(72,  "PSRAM",
      ESP.getPsramSize() - ESP.getFreePsram(), ESP.getPsramSize(), CLR_PRIMARY);
    
    memBar(118, "Flash",
      ESP.getFlashChipSize() - ESP.getFreeSketchSpace(), ESP.getFlashChipSize(), CLR_ORANGE);
    
    if (SPIFFS.begin(false)) {
      memBar(164, "SPIFFS",
        SPIFFS.usedBytes(), SPIFFS.totalBytes(), CLR_YELLOW);
    }
    
    spr->fillRect(0, 280, 240, 20, CLR_BG2);
    for (int p = 0; p < 4; p++)
      spr->fillCircle(100 + p*12, 289, p == page ? 4 : 2, p == page ? CLR_APP_ORANGE : CLR_GRAY3);
    ctx.display->pushContent();
  }
  
  void start(AppContext& ctx) {
    page = 0;
    memset(heapHistory, 0, sizeof(heapHistory));
    memset(psramHistory, 0, sizeof(psramHistory));
    memset(batHistory, 100, sizeof(batHistory));
    histHead = 0; lastSample = 0;
  }
  
  void loop(AppContext& ctx) {
    _sample(ctx);
    
    switch(page) {
      case 0: renderOverview(ctx); break;
      case 1: renderMemory(ctx);   break;
      case 2: renderNetwork(ctx);  break;
      case 3: renderTasks(ctx);    break;
    }
    
    if (ctx.input->left())  { if (page > 0) page--; }
    if (ctx.input->right()) { if (page < 3) page++; }
    if (ctx.input->b())     { ctx.os->setState(OSState::HOME_SCREEN); }
  }
}

// ─── Text Editor App ─────────────────────────────────────────
namespace AppTextEditor {
  static String   content = "";
  static String   filename = "untitled.txt";
  static int      cursorPos = 0;
  static int      scrollLine = 0;
  static bool     modified = false;
  static bool     showKeyboard = false;
  static int      kbRow = 0, kbCol = 0;
  static bool     kbShift = false;
  
  static const char* kbRows[] = {
    "1234567890-=",
    "qwertyuiop[]",
    "asdfghjkl;'",
    "zxcvbnm,./"
  };
  static const char* kbRowsShift[] = {
    "!@#$%^&*()_+",
    "QWERTYUIOP{}",
    "ASDFGHJKL:\"",
    "ZXCVBNM<>?"
  };
  
  std::vector<String> _getLines() {
    std::vector<String> lines;
    String cur = "";
    for (char c : content) {
      if (c == '\n') { lines.push_back(cur); cur = ""; }
      else cur += c;
    }
    lines.push_back(cur);
    return lines;
  }
  
  void renderEditor(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 18, CLR_GRAY3);
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString((modified ? "* " : "  ") + filename, 4, 10);
    spr->setTextDatum(MR_DATUM);
    spr->setTextColor(CLR_GRAY2);
    spr->drawString(String(content.length()) + "b", 234, 10);
    
    spr->fillRect(0, 19, 240, 230, CLR_BLACK);
    auto lines = _getLines();
    
    int cLine = 0, cCol = 0;
    int pos = 0;
    for (int i = 0; i < (int)content.length() && pos < cursorPos; i++, pos++) {
      if (content[i] == '\n') { cLine++; cCol = 0; }
      else cCol++;
    }
    
    if (cLine < scrollLine) scrollLine = cLine;
    if (cLine >= scrollLine + 12) scrollLine = cLine - 11;
    
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    for (int l = 0; l < 12 && (l + scrollLine) < (int)lines.size(); l++) {
      int ly = 22 + l * 18;
      int lineNum = l + scrollLine;
      
      char ln[5]; snprintf(ln, 5, "%3d", lineNum+1);
      spr->setTextColor(CLR_GRAY3);
      spr->drawString(ln, 2, ly);
      
      spr->setTextColor(CLR_WHITE);
      spr->drawString(lines[lineNum], 26, ly);
      
      if (lineNum == cLine) {
        int cx = 26 + cCol * 6;
        static bool curBlink = false;
        static uint32_t bt = 0;
        if (millis() - bt > 400) { curBlink = !curBlink; bt = millis(); }
        if (curBlink) spr->drawLine(cx, ly-1, cx, ly+10, CLR_PRIMARY);
      }
    }
    
    spr->fillRect(0, 250, 240, 14, CLR_BG2);
    char posStr[24];
    snprintf(posStr, 24, "Ln:%d Col:%d | %d lines", cLine+1, cCol+1, (int)lines.size());
    spr->setTextColor(CLR_GRAY2); spr->setTextDatum(ML_DATUM);
    spr->drawString(posStr, 4, 257);
    
    spr->fillRect(0, 265, 240, 35, CLR_BG2);
    spr->setTextColor(CLR_GRAY2); spr->setTextDatum(MC_DATUM);
    spr->drawString("SELECT:Keyboard  OPTION:Save", 120, 272);
    spr->drawString("START:Enter  B:Del  A:Exit", 120, 284);
    
    ctx.display->pushContent();
  }
  
  void renderKeyboard(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillRect(0, 0, 240, 130, CLR_BLACK);
    auto lines = _getLines();
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    for (int l = 0; l < 7 && (l + scrollLine) < (int)lines.size(); l++) {
      spr->drawString(lines[l + scrollLine], 2, 4 + l * 18);
    }
    
    spr->fillRect(0, 130, 240, 170, CLR_BG2);
    spr->drawLine(0, 130, 240, 130, CLR_GRAY3);
    
    const char** rows = kbShift ? kbRowsShift : kbRows;
    
    for (int r = 0; r < 4; r++) {
      int rowLen = strlen(rows[r]);
      int startX = (240 - rowLen * 18) / 2;
      
      for (int c = 0; c < rowLen; c++) {
        int x = startX + c * 18;
        int y = 135 + r * 34;
        bool sel = (r == kbRow && c == kbCol);
        
        spr->fillRoundRect(x, y, 16, 28, 3, sel ? CLR_PRIMARY : CLR_GRAY3);
        spr->setTextColor(CLR_WHITE); spr->setTextDatum(MC_DATUM);
        char ch[2] = {rows[r][c], 0};
        spr->drawString(ch, x+8, y+14);
      }
    }
    
    spr->fillRoundRect(5,  264, 40, 22, 3, kbShift ? CLR_PRIMARY : CLR_GRAY3);
    spr->fillRoundRect(50, 264, 60, 22, 3, CLR_GRAY3);
    spr->fillRoundRect(115,264, 40, 22, 3, CLR_GRAY3);
    spr->fillRoundRect(160,264, 35, 22, 3, CLR_GRAY3);
    spr->fillRoundRect(200,264, 35, 22, 3, CLR_APP_RED);
    
    spr->setTextColor(CLR_WHITE); spr->setTextDatum(MC_DATUM);
    spr->drawString("SHF", 25, 274);
    spr->drawString("SPACE", 80, 274);
    spr->drawString("ENT", 135, 274);
    spr->drawString("DEL", 177, 274);
    spr->drawString("ESC", 217, 274);
    
    ctx.display->pushContent();
  }
  
  void handleKeyboardInput(AppContext& ctx) {
    const char** rows = kbShift ? kbRowsShift : kbRows;
    int rowLen = strlen(rows[kbRow]);
    
    if (ctx.input->up())    { if (kbRow > 0) { kbRow--; kbCol = min(kbCol, (int)strlen(rows[kbRow])-1); }}
    if (ctx.input->down())  { if (kbRow < 3) { kbRow++; kbCol = min(kbCol, (int)strlen(rows[kbRow])-1); }}
    if (ctx.input->left())  { if (kbCol > 0) kbCol--; }
    if (ctx.input->right()) { if (kbCol < rowLen-1) kbCol++; }
    
    if (ctx.input->start()) {
      char ch = rows[kbRow][kbCol];
      content = content.substring(0, cursorPos) + ch + content.substring(cursorPos);
      cursorPos++;
      modified = true;
    }
    if (ctx.input->menu()) {
      content = content.substring(0, cursorPos) + " " + content.substring(cursorPos);
      cursorPos++; modified = true;
    }
    if (ctx.input->option()) {
      content = content.substring(0, cursorPos) + "\n" + content.substring(cursorPos);
      cursorPos++; modified = true;
    }
    if (ctx.input->b()) {
      if (cursorPos > 0) {
        content = content.substring(0, cursorPos-1) + content.substring(cursorPos);
        cursorPos--; modified = true;
      }
    }
    if (ctx.input->a())  { kbShift = !kbShift; }
    if (ctx.input->select()) { showKeyboard = false; }
  }
  
  void handleEditorInput(AppContext& ctx) {
    if (ctx.input->left())  { if (cursorPos > 0) cursorPos--; }
    if (ctx.input->right()) { if (cursorPos < (int)content.length()) cursorPos++; }
    if (ctx.input->up()) {
      int nl = content.lastIndexOf('\n', max(0, cursorPos-1));
      if (nl >= 0) cursorPos = max(0, nl - 1);
    }
    if (ctx.input->down()) {
      int nl = content.indexOf('\n', cursorPos);
      if (nl >= 0) cursorPos = min((int)content.length(), nl + 1);
    }
    if (ctx.input->select()) { showKeyboard = true; kbRow = 1; kbCol = 0; }
    if (ctx.input->b()) {
      if (cursorPos > 0) {
        content = content.substring(0, cursorPos-1) + content.substring(cursorPos);
        cursorPos--; modified = true;
      }
    }
    if (ctx.input->option()) {
      if (ctx.fs->sdMounted) {
        ctx.fs->writeFile("/saves/" + filename, content);
        modified = false;
        ctx.display->showToast("Saved: " + filename);
      } else {
        ctx.display->showToast("No SD card!");
      }
    }
    if (ctx.input->b()) { ctx.os->setState(OSState::HOME_SCREEN); }
  }
  
  void start(AppContext& ctx) {
    content = ""; cursorPos = 0; scrollLine = 0;
    modified = false; showKeyboard = false;
    filename = "untitled.txt";
    kbRow = 1; kbCol = 0; kbShift = false;
  }
  
  void loop(AppContext& ctx) {
    if (showKeyboard) {
      renderKeyboard(ctx);
      handleKeyboardInput(ctx);
    } else {
      renderEditor(ctx);
      handleEditorInput(ctx);
    }
  }
}

// I2S pins (optional - connect I2S DAC like MAX98357A)
#define I2S_BCLK   -1
#define I2S_LRC    -1
#define I2S_DOUT   -1

// ─── Music Player App ─────────────────────────────────────
namespace AppMusicPlayer {
  static std::vector<Track> playlist;
  static int    currentTrack = 0;
  static int    scroll = 0;
  static bool   playing = false;
  static bool   paused  = false;
  static int    volume  = 75;
  static uint32_t playStart = 0;
  static uint32_t elapsed   = 0;
  static int    visualizer[12] = {};
  static uint32_t vizUpdate = 0;
  
  void _scanMusic() {
    playlist.clear();
    if (!SD.begin()) return;
    
    const char* exts[] = {".mp3", ".wav", ".flac", ".ogg", nullptr};
    File dir = SD.open("/music");
    if (!dir) return;
    
    File f;
    while ((f = dir.openNextFile())) {
      String name = f.name();
      bool match = false;
      for (int i = 0; exts[i]; i++) {
        if (name.endsWith(exts[i])) { match = true; break; }
      }
      if (match) {
        Track t;
        t.name = name.substring(0, name.lastIndexOf('.'));
        t.path = "/music/" + name;
        t.size = f.size();
        t.duration = t.size / 16000;
        playlist.push_back(t);
      }
      f.close();
    }
    dir.close();
    
    if (playlist.empty()) {
      playlist.push_back({"No music files", "", 0, 0});
      playlist.push_back({"Add .mp3 to /music/", "", 0, 0});
    }
  }
  
  void _updateVisualizer() {
    if (millis() - vizUpdate < 80) return;
    vizUpdate = millis();
    for (int i = 0; i < 12; i++) {
      if (playing && !paused) {
        int target = random(2, 20);
        visualizer[i] = (visualizer[i] * 3 + target) / 4;
      } else {
        visualizer[i] = max(1, visualizer[i] - 2);
      }
    }
  }
  
  String _formatTime(uint32_t sec) {
    char buf[8];
    snprintf(buf, 8, "%d:%02d", sec/60, sec%60);
    return String(buf);
  }
  
  void render(AppContext& ctx) {
    _updateVisualizer();
    
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    for (int y = 0; y < 80; y++) {
      uint16_t c = spr->alphaBlend(y*3, CLR_BG, CLR_APP_PURPLE);
      spr->drawFastHLine(0, y, 240, c);
    }
    
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(MC_DATUM);
    spr->drawString("Music Player", 120, 10);
    
    spr->fillRoundRect(84, 18, 72, 72, 10, CLR_APP_PURPLE);
    spr->setTextSize(3); spr->setTextDatum(MC_DATUM);
    spr->drawString(playing ? "♪" : "♫", 120, 52);
    
    String trackName = playlist.empty() ? "No tracks" :
      (currentTrack < (int)playlist.size() ? playlist[currentTrack].name : "---");
    if (trackName.length() > 20) trackName = trackName.substring(0, 20) + "..";
    
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(MC_DATUM);
    spr->drawString(trackName, 120, 97);
    spr->setTextColor(CLR_GRAY2);
    char trackNum[16];
    snprintf(trackNum, 16, "%d / %d", currentTrack+1, (int)playlist.size());
    spr->drawString(trackNum, 120, 109);
    
    uint32_t dur = playlist.empty() ? 0 :
      (currentTrack < (int)playlist.size() ? playlist[currentTrack].duration : 0);
    if (playing && !paused) elapsed = (millis() - playStart) / 1000;
    
    spr->fillRect(5, 120, 230, 3, CLR_GRAY3);
    if (dur > 0) {
      int px = min(230, (int)(230 * elapsed / dur));
      spr->fillRect(5, 120, px, 3, CLR_PRIMARY);
      spr->fillCircle(5 + px, 121, 4, CLR_WHITE);
    }
    
    spr->setTextColor(CLR_GRAY2);
    spr->setTextDatum(ML_DATUM);
    spr->drawString(_formatTime(elapsed), 6, 128);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(dur > 0 ? _formatTime(dur) : "--:--", 234, 128);
    
    for (int i = 0; i < 12; i++) {
      int bx = 10 + i * 19;
      int bh = max(2, visualizer[i]);
      uint16_t bc = playing && !paused ? 
        spr->color565(0, 180, 255 - i*15) : CLR_GRAY3;
      spr->fillRect(bx, 158 - bh, 14, bh, bc);
    }
    
    int cy = 170;
    spr->fillTriangle(28, cy+10, 28, cy+30, 15, cy+20, CLR_GRAY1);
    spr->fillRect(10, cy+10, 4, 20, CLR_GRAY1);
    if (playing && !paused) {
      spr->fillRect(100, cy+8, 10, 24, CLR_WHITE);
      spr->fillRect(116, cy+8, 10, 24, CLR_WHITE);
    } else {
      spr->fillTriangle(100, cy+8, 100, cy+32, 128, cy+20, CLR_WHITE);
    }
    spr->fillTriangle(172, cy+10, 172, cy+30, 185, cy+20, CLR_GRAY1);
    spr->fillRect(186, cy+10, 4, 20, CLR_GRAY1);
    spr->fillRect(205, cy+12, 18, 18, CLR_ACCENT);
    
    spr->setTextColor(CLR_GRAY2); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString("Vol", 5, 212);
    UITheme::drawProgressBar(spr, 28, 209, 170, 8, volume, CLR_PRIMARY, CLR_GRAY3);
    char vbuf[6]; snprintf(vbuf, 6, "%d%%", volume);
    spr->setTextColor(CLR_GRAY1); spr->drawString(vbuf, 202, 212);
    
    spr->fillRect(0, 225, 240, 1, CLR_GRAY3);
    for (int i = 0; i < 4 && (i + scroll) < (int)playlist.size(); i++) {
      int idx = i + scroll;
      bool cur = (idx == currentTrack);
      int y = 228 + i * 18;
      if (cur) {
        spr->fillRect(0, y-1, 240, 17, CLR_BG2);
        spr->setTextColor(CLR_PRIMARY);
      } else {
        spr->setTextColor(CLR_GRAY2);
      }
      spr->setTextDatum(ML_DATUM);
      spr->drawString((cur ? "▶ " : "  ") + playlist[idx].name, 4, y + 6);
    }
    
    spr->fillRect(0, 298, 240, 2, CLR_BG2);
    spr->setTextColor(CLR_GRAY3); spr->setTextDatum(MC_DATUM);
    spr->drawString("L/R:Track  A:Play  B/+VOL:-VOL", 120, 296);
    
    ctx.display->pushContent();
  }
  
  void start(AppContext& ctx) {
    playing = paused = false;
    volume = 75; elapsed = 0;
    currentTrack = scroll = 0;
    memset(visualizer, 1, sizeof(visualizer));
    _scanMusic();
    
    if (I2S_BCLK >= 0) {
      Serial.println(F("[Music] I2S audio init - install ESP8266Audio lib"));
    } else {
      Serial.println(F("[Music] No I2S pins - set I2S_BCLK/LRC/DOUT"));
    }
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    
    if (ctx.input->left())   {
      if (currentTrack > 0) { currentTrack--; playing = false; elapsed = 0; }
    }
    if (ctx.input->right())  {
      if (currentTrack < (int)playlist.size()-1) { currentTrack++; playing = false; elapsed = 0; }
    }
    if (ctx.input->up())     { if (volume < 100) volume += 5; }
    if (ctx.input->down())   { if (volume > 0) volume -= 5; }
    if (ctx.input->start()) {
      if (!playing) {
        playing = true; paused = false;
        playStart = millis() - elapsed * 1000;
      } else {
        paused = !paused;
      }
    }
    if (ctx.input->b()) {
      playing = false; paused = false; elapsed = 0;
    }
    if (ctx.input->option())   { _scanMusic(); }
    if (ctx.input->b())      { ctx.os->setState(OSState::HOME_SCREEN); }
  }
}

// ─── Snake Game App ─────────────────────────────────────────
namespace AppSnake {
  #define GRID_W   20
  #define GRID_H   20
  #define CELL_SZ  12
  #define GRID_OX  20
  #define GRID_OY  20

  static std::vector<Point> snake;
  static Point  food;
  static int    dir = 0;
  static int    nextDir = 0;
  static bool   alive = true;
  static int    score = 0;
  static int    highScore = 0;
  static uint32_t lastMove = 0;
  static int    speed = 200;
  static bool   started = false;
  static int    level = 1;
  
  void _placeFood() {
    bool ok = false;
    while (!ok) {
      food = {random(GRID_W), random(GRID_H)};
      ok = true;
      for (auto& p : snake) {
        if (p.x == food.x && p.y == food.y) { ok = false; break; }
      }
    }
  }
  
  void _reset() {
    snake.clear();
    snake.push_back({GRID_W/2,   GRID_H/2});
    snake.push_back({GRID_W/2-1, GRID_H/2});
    snake.push_back({GRID_W/2-2, GRID_H/2});
    dir = nextDir = 0;
    alive = true; score = 0; level = 1;
    speed = 200;
    _placeFood();
  }
  
  bool _moveSnake() {
    dir = nextDir;
    Point head = snake.front();
    Point nh = head;
    
    switch(dir) {
      case 0: nh.x++; break;
      case 1: nh.y++; break;
      case 2: nh.x--; break;
      case 3: nh.y--; break;
    }
    
    if (nh.x < 0 || nh.x >= GRID_W || nh.y < 0 || nh.y >= GRID_H) return false;
    
    for (auto& p : snake) if (p.x == nh.x && p.y == nh.y) return false;
    
    snake.insert(snake.begin(), nh);
    
    if (nh.x == food.x && nh.y == food.y) {
      score += 10 * level;
      if (score > highScore) highScore = score;
      if (snake.size() % 5 == 0) {
        level++;
        speed = max(60, speed - 20);
      }
      _placeFood();
    } else {
      snake.pop_back();
    }
    return true;
  }
  
  void render(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    spr->fillRect(0, 0, 240, 18, CLR_BG2);
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString("SNAKE", 5, 10);
    char scoreBuf[24];
    snprintf(scoreBuf, 24, "Score:%d Hi:%d Lv:%d", score, highScore, level);
    spr->setTextColor(CLR_YELLOW); spr->setTextDatum(MR_DATUM);
    spr->drawString(scoreBuf, 235, 10);
    
    spr->drawRect(GRID_OX-1, GRID_OY-1,
      GRID_W*CELL_SZ+2, GRID_H*CELL_SZ+2, CLR_GRAY3);
    
    for (int gx = 0; gx < GRID_W; gx += 4) {
      for (int gy = 0; gy < GRID_H; gy += 4) {
        spr->drawPixel(GRID_OX + gx*CELL_SZ, GRID_OY + gy*CELL_SZ, CLR_GRAY3);
      }
    }
    
    static int pulse = 0;
    static bool pdir = true;
    if (pdir) { if (++pulse >= 5) pdir = false; } else { if (--pulse <= 0) pdir = true; }
    
    int fx = GRID_OX + food.x * CELL_SZ;
    int fy = GRID_OY + food.y * CELL_SZ;
    spr->fillCircle(fx + CELL_SZ/2, fy + CELL_SZ/2, 4 + pulse/2, CLR_ACCENT);
    spr->fillCircle(fx + CELL_SZ/2, fy + CELL_SZ/2, 2, CLR_WHITE);
    
    for (int i = 0; i < (int)snake.size(); i++) {
      int sx = GRID_OX + snake[i].x * CELL_SZ;
      int sy = GRID_OY + snake[i].y * CELL_SZ;
      
      float ratio = (float)i / snake.size();
      uint16_t c = spr->color565(0, (int)(200 - ratio * 100), (int)(50 + ratio * 100));
      
      if (i == 0) {
        spr->fillRoundRect(sx+1, sy+1, CELL_SZ-2, CELL_SZ-2, 3, CLR_GREEN);
        int ex1 = sx + (dir == 0 ? 7 : dir == 2 ? 2 : 3);
        int ey1 = sy + (dir == 1 ? 7 : dir == 3 ? 2 : 2);
        int ex2 = sx + (dir == 0 ? 7 : dir == 2 ? 2 : 7);
        int ey2 = sy + (dir == 1 ? 7 : dir == 3 ? 2 : 7);
        spr->fillCircle(ex1, ey1, 1, CLR_BLACK);
        spr->fillCircle(ex2, ey2, 1, CLR_BLACK);
      } else {
        spr->fillRoundRect(sx+1, sy+1, CELL_SZ-2, CELL_SZ-2, 2, c);
      }
    }
    
    if (!alive) {
      spr->fillRoundRect(40, 90, 160, 80, 10, CLR_BG2);
      spr->drawRoundRect(40, 90, 160, 80, 10, CLR_ACCENT);
      spr->setTextColor(CLR_ACCENT); spr->setTextSize(2); spr->setTextDatum(MC_DATUM);
      spr->drawString("GAME OVER", 120, 110);
      spr->setTextColor(CLR_WHITE); spr->setTextSize(1);
      char sb[20]; snprintf(sb, 20, "Score: %d", score);
      spr->drawString(sb, 120, 130);
      spr->setTextColor(CLR_GRAY2);
      spr->drawString("START: Play Again", 120, 148);
      spr->drawString("A: Exit", 120, 162);
    }
    
    if (!started) {
      spr->fillRoundRect(40, 100, 160, 60, 10, CLR_BG2);
      spr->setTextColor(CLR_GREEN); spr->setTextSize(2); spr->setTextDatum(MC_DATUM);
      spr->drawString("SNAKE", 120, 115);
      spr->setTextColor(CLR_GRAY1); spr->setTextSize(1);
      spr->drawString("Press A to start", 120, 138);
      spr->drawString("D-Pad to move", 120, 150);
    }
    
    spr->fillRect(0, 280, 240, 20, CLR_BG2);
    spr->setTextColor(CLR_GRAY2); spr->setTextDatum(ML_DATUM);
    spr->drawString("Speed:", 5, 289);
    UITheme::drawProgressBar(spr, 50, 285, 100, 8, map(speed, 60, 200, 100, 0), CLR_GREEN, CLR_GRAY3);
    spr->setTextColor(CLR_GRAY3); spr->setTextDatum(MR_DATUM);
    spr->drawString("A:Exit", 235, 289);
    
    ctx.display->pushContent();
  }
  
  void start(AppContext& ctx) {
    _reset();
    started = false;
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    
    if (!started) {
      if (ctx.input->start()) { started = true; lastMove = millis(); }
      if (ctx.input->b()) ctx.os->setState(OSState::HOME_SCREEN);
      return;
    }
    
    if (!alive) {
      if (ctx.input->start()) { _reset(); started = true; lastMove = millis(); }
      if (ctx.input->b()) ctx.os->setState(OSState::HOME_SCREEN);
      return;
    }
    
    if (ctx.input->right() && dir != 2) nextDir = 0;
    if (ctx.input->down()  && dir != 3) nextDir = 1;
    if (ctx.input->left()  && dir != 0) nextDir = 2;
    if (ctx.input->up()    && dir != 1) nextDir = 3;
    if (ctx.input->b()) { ctx.os->setState(OSState::HOME_SCREEN); return; }
    
    if (millis() - lastMove >= (uint32_t)speed) {
      lastMove = millis();
      alive = _moveSnake();
    }
  }
}

// ─── App Manager registration ─────────────────────────────
void AppManager::_registerBuiltinApps() {
  registerApp({"settings", "Settings", "⚙", CLR_APP_BLUE, "system", true,
    AppSettings::start, nullptr, nullptr, nullptr, AppSettings::loop});
  registerApp({"files", "Files", "F", CLR_APP_TEAL, "system", true,
    AppFileManager::start, nullptr, nullptr, nullptr, AppFileManager::loop});
  registerApp({"wifi", "WiFi", "W", CLR_APP_BLUE, "system", true,
    AppWiFi::start, nullptr, nullptr, nullptr, AppWiFi::loop});
  registerApp({"ble", "Bluetooth", "B", CLR_APP_PURPLE, "system", true,
    AppBLE::start, nullptr, nullptr, nullptr, AppBLE::loop});
  registerApp({"calc", "Calc", "=", CLR_APP_ORANGE, "tools", true,
    AppCalc::start, nullptr, nullptr, nullptr, AppCalc::loop});
  registerApp({"terminal", "Terminal", ">", CLR_BLACK, "tools", true,
    AppTerminal::start, nullptr, nullptr, nullptr, AppTerminal::loop});
  registerApp({"retro", "RetroEmu", "R", CLR_APP_RED, "games", true,
    nullptr, nullptr, nullptr, nullptr, nullptr});
  registerApp({"lua", "Lua IDE", "L", CLR_APP_YELLOW, "scripts", true,
    nullptr, nullptr, nullptr, nullptr, nullptr});
  registerApp({"js", "JS Run", "J", CLR_APP_GREEN, "scripts", true,
    nullptr, nullptr, nullptr, nullptr, nullptr});
  registerApp({"lora", "LoRa", "~", CLR_APP_ORANGE, "tools", true,
    nullptr, nullptr, nullptr, nullptr, nullptr});
  registerApp({"sysmon", "Monitor", "M", CLR_APP_TEAL, "system", true,
    nullptr, nullptr, nullptr, nullptr, nullptr});
  registerApp({"editor", "Editor", "E", CLR_GRAY3, "tools", true,
    nullptr, nullptr, nullptr, nullptr, nullptr});
}
