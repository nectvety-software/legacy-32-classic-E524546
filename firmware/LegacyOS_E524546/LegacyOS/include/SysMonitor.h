#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "UITheme.h"
#include "HardwareConfig.h"

// ═══════════════════════════════════════════════════════════
//  System Monitor - Real-time hardware performance dashboard
// ═══════════════════════════════════════════════════════════

namespace AppSysMon {
  // History ring buffers (60 samples)
  static uint8_t  heapHistory[60]   = {};
  static uint8_t  psramHistory[60]  = {};
  static uint8_t  batHistory[60]    = {};
  static int      histHead = 0;
  static uint32_t lastSample = 0;
  
  static int  page = 0;  // 0=Overview, 1=Memory, 2=Network, 3=Tasks

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
    
    // Grid lines
    for (int g = 1; g < 4; g++) {
      int gy = oy + h * g / 4;
      for (int gx = ox; gx < ox + w; gx += 4) spr->drawPixel(gx, gy, CLR_GRAY3);
    }
    
    // Plot
    int prev = -1;
    for (int i = 0; i < 60; i++) {
      int idx = (histHead + i) % 60;
      int px = ox + i * w / 60;
      int py = oy + h - (data[idx] * h / 100) - 1;
      if (prev >= 0) spr->drawLine(ox + (i-1)*w/60, prev, px, py, lineC);
      prev = py;
    }
    
    // Current value
    int curIdx = (histHead + 59) % 60;
    char buf[8]; snprintf(buf, 8, "%d%%", data[curIdx]);
    spr->setTextColor(lineC);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(buf, ox + w - 2, oy + 2);
  }
  
  void renderOverview(AppContext& ctx) {
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    // Header
    spr->fillRect(0, 0, 240, 20, CLR_APP_TEAL);
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(1);
    spr->setTextDatum(ML_DATUM);
    spr->drawString("System Monitor", 5, 11);
    
    // Uptime
    uint32_t up = ctx.os->uptime();
    char upBuf[24];
    snprintf(upBuf, 24, "Up: %02d:%02d:%02d", up/3600, (up%3600)/60, up%60);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(upBuf, 235, 11);
    
    // Graphs
    _drawGraph(spr, heapHistory,  5, 25, 225, 50, CLR_CYAN,   "Heap Free");
    _drawGraph(spr, psramHistory, 5, 82, 225, 50, CLR_PRIMARY,"PSRAM Free");
    _drawGraph(spr, batHistory,   5,139, 225, 50, CLR_GREEN,  "Battery");
    
    // System stats grid
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
    
    // Page dots
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
    
    // TaskStatus array
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
    
    // SPIFFS
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
