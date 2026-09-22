#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ─────────────────────────────────────────────
//  OS States
// ─────────────────────────────────────────────
enum class OSState {
  BOOT_ANIMATION,
  HOME_SCREEN,
  APP_DRAWER,
  RUNNING_APP,
  NOTIFICATION_PANEL,
  QUICK_SETTINGS,
  LOCK_SCREEN
};

// ─────────────────────────────────────────────
//  Notification System
// ─────────────────────────────────────────────
struct Notification {
  String title;
  String message;
  String icon;
  uint32_t timestamp;
  bool read;
};

#include "HardwareConfig.h"
#include "DisplayManager.h"
#include "InputManager.h"
#include "AppManager.h"
#include "FileSystem.h"
#include "NetworkManager.h"
#include "LuaEngine.h"
#include "JSEngine.h"
#include "RetroEmu.h"
#include "UITheme.h"
#include "LoRaManager.h"

// ─────────────────────────────────────────────
//  LegacyOS Core Class
// ─────────────────────────────────────────────
class LegacyOS {
public:
  LegacyOS();
  ~LegacyOS();
  
  void init();
  void run();
  
  // Subsystems (public for apps to access)
  DisplayManager* display;
  InputManager*   input;
  AppManager*     appMgr;
  FileSystem*     fs;
  NetMgr* network;
  LuaEngine*      lua;
  JSEngine*       js;
  RetroEmu*       retro;
  UITheme*        theme;
  Preferences     prefs;

  // OS API
  void pushNotification(const String& title, const String& msg, const String& icon = "📢");
  void setState(OSState s);
  OSState getState() { return _state; }
  void reboot();
  void sleep();
  uint32_t uptime() { return millis() / 1000; }
  String getVersion() { return "LegacyOS v1.0"; }
  String getDevice()  { return "LEGACY-32 CLASSIC E524546"; }

  // Status bar
  int   getBattery()    { return _battery; }
  bool  isCharging()    { return _charging; }
  int   getWifiStrength();
  
private:
  OSState _state;
  int     _battery;
  bool    _charging;
  uint32_t _lastStatusUpdate;
  uint32_t _lastInputTime;
  
  std::vector<Notification> _notifications;
  int _notifScroll = 0;

  // Boot sequence
  void _bootAnimation();
  void _initHardware();
  void _initSubsystems();
  void _loadSettings();
  
  // Screen renderers
  void _renderHomeScreen();
  void _renderStatusBar();
  void _renderAppDrawer();
  void _renderNotificationPanel();
  void _renderQuickSettings();
  void _renderLockScreen();
  
  // Input handlers
  void _handleHomeInput();
  void _handleDrawerInput();
  void _handleNotifInput();
  void _handleQuickSettInput();
  
  // Home screen state
  int  _homeWallpaper;
  int  _appDrawerScroll;
  int  _appDrawerSelected;
  bool _notifPanelOpen;
  bool _quickSettOpen;
  uint32_t _notifPanelY;  // for animation
  
  void _registerScriptApps();
  void _updateBatteryStatus();
  void _drawClock();
  void _drawWeather();
  void _drawHomeWidgets();
};

#include "AppImplementations.h"
