#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>
#include "HardwareConfig.h"
#include "UITheme.h"

// ═══════════════════════════════════════════════════════════
//  AppManager - App registry, lifecycle, and launcher
// ═══════════════════════════════════════════════════════════

// Forward declarations
class LegacyOS;
class DisplayManager;
class InputManager;
class FileSystem;
class NetMgr;
class LuaEngine;
class JSEngine;
class RetroEmu;

// App context passed to every app
struct AppContext {
  LegacyOS*       os;
  DisplayManager* display;
  InputManager*   input;
  UITheme*        theme;
  FileSystem*     fs;
  NetMgr*         network;
  LuaEngine*      lua;
  JSEngine*       js;
  RetroEmu*       retro;
};

// App descriptor
struct AppInfo {
  String   id;
  String   name;
  String   icon;       // Emoji or 1-2 char symbol
  uint16_t iconBg;     // Icon background color
  String   category;   // "system", "games", "tools", "scripts"
  bool     system;     // System app (can't be removed)
  
  // App entry point functions
  std::function<void(AppContext&)> onStart;
  std::function<void(AppContext&)> onResume;
  std::function<void(AppContext&)> onPause;
  std::function<void(AppContext&)> onStop;
  std::function<void(AppContext&)> onLoop;
};

enum class AppState { STOPPED, STARTING, RUNNING, PAUSED, STOPPING };

struct RunningApp {
  AppInfo*  info;
  AppState  state;
  uint32_t  startTime;
  uint32_t  cpuTime;
  size_t    memUsage;
  String    statusText;
};

// ─── App Manager ────────────────────────────────────────────
class AppManager {
public:
  std::vector<AppInfo>  apps;       // All installed apps
  RunningApp*           current;    // Currently running app
  AppContext            ctx;        // Shared context
  
  AppManager() : current(nullptr) {}
  
  void init(AppContext& context) {
    ctx = context;
    current = nullptr;
    _registerBuiltinApps();
    Serial.printf("[AppMgr] %d built-in apps registered\n", apps.size());
  }
  
  // Register an app
  void registerApp(AppInfo info) {
    apps.push_back(info);
  }
  
  // Launch app by ID
  bool launch(const String& appId) {
    for (auto& app : apps) {
      if (app.id == appId) {
        return _launch(&app);
      }
    }
    Serial.printf("[AppMgr] App not found: %s\n", appId.c_str());
    return false;
  }
  
  // Launch by index
  bool launchByIndex(int idx) {
    if (idx < 0 || idx >= (int)apps.size()) return false;
    return _launch(&apps[idx]);
  }
  
  // Loop current app
  void loopCurrentApp() {
    if (current && current->state == AppState::RUNNING) {
      if (current->info->onLoop) {
        current->info->onLoop(ctx);
        current->cpuTime += 1;
      }
    }
  }
  
  // Stop current app
  void stopCurrent() {
    if (current) {
      if (current->info->onStop) {
        current->info->onStop(ctx);
      }
      current->state = AppState::STOPPED;
      current = nullptr;
      Serial.println(F("[AppMgr] App stopped"));
    }
  }
  
  // Pause current app
  void pauseCurrent() {
    if (current && current->state == AppState::RUNNING) {
      if (current->info->onPause) {
        current->info->onPause(ctx);
      }
      current->state = AppState::PAUSED;
    }
  }
  
  bool isRunning() { return current != nullptr && current->state == AppState::RUNNING; }
  
  AppInfo* findApp(const String& id) {
    for (auto& a : apps) if (a.id == id) return &a;
    return nullptr;
  }
  
  // Get apps by category
  std::vector<AppInfo*> getByCategory(const String& cat) {
    std::vector<AppInfo*> result;
    for (auto& a : apps) {
      if (cat == "all" || a.category == cat) result.push_back(&a);
    }
    return result;
  }
  
private:
  RunningApp _runningApp;
  
  bool _launch(AppInfo* app) {
    // Stop any running app
    stopCurrent();
    
    _runningApp.info      = app;
    _runningApp.state     = AppState::STARTING;
    _runningApp.startTime = millis();
    _runningApp.cpuTime   = 0;
    _runningApp.memUsage  = ESP.getFreeHeap();
    _runningApp.statusText = "";
    current = &_runningApp;
    
    Serial.printf("[AppMgr] Launching: %s\n", app->name.c_str());
    
    if (app->onStart) {
      app->onStart(ctx);
    }
    current->state = AppState::RUNNING;
    return true;
  }
  
  // ─── Register all built-in apps ───────────────────────────
  void _registerBuiltinApps();
};


