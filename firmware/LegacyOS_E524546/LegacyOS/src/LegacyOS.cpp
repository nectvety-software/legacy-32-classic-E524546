/*
 * LegacyOS.cpp - Core OS implementation
 * LEGACY-32 CLASSIC E524546
 */

#include "include/LegacyOS.h"

// Global theme instance
UITheme gTheme;

// ─────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────
LegacyOS::LegacyOS() {
  display  = new DisplayManager();
  input    = new InputManager();
  appMgr   = new AppManager();
  fs       = new FileSystem();
  network  = new NetMgr();
  lua      = new LuaEngine();
  js       = new JSEngine();
  retro    = new RetroEmu();
  theme    = &gTheme;
  
  _state           = OSState::BOOT_ANIMATION;
  _battery         = 100;
  _charging        = false;
  _homeWallpaper   = 0;
  _appDrawerScroll  = 0;
  _appDrawerSelected = 0;
  _notifPanelOpen  = false;
  _quickSettOpen   = false;
  _notifPanelY     = 0;
  _lastStatusUpdate = 0;
  _lastInputTime   = 0;
}

LegacyOS::~LegacyOS() {
  delete display;
  delete input;
  delete appMgr;
  delete fs;
  delete network;
  delete lua;
  delete js;
  delete retro;
}

// ─────────────────────────────────────────────────────────────────
//  init() - Full hardware + subsystem initialization
// ─────────────────────────────────────────────────────────────────
void LegacyOS::init() {
  _initHardware();
  display->showBootSplash();
  display->showBootProgress(5, "Starting hardware...");
  delay(300);
  
  _initSubsystems();
  _loadSettings();
  
  display->showBootProgress(100, "Welcome!");
  delay(600);
  display->fadeOut();
  
  setState(OSState::HOME_SCREEN);
  display->fadeIn();
  
  Serial.println(F("[OS] LegacyOS boot complete!"));
  pushNotification("LegacyOS", "System ready", "✓");
}

// ─────────────────────────────────────────────────────────────────
//  run() - Main OS loop (call from Arduino loop())
// ─────────────────────────────────────────────────────────────────
void LegacyOS::run() {
  // Update input
  input->update();
  
  // Wake screen on any key
  if (input->anyKeyPressed) {
    display->wakeScreen();
  }
  
  // Auto-dim after 30s idle
  if (input->isIdle(30000)) {
    display->dimScreen();
  }
  
  // Update network status every 5s
  if (millis() - _lastStatusUpdate > 5000) {
    network->update();
    _updateBatteryStatus();
    _lastStatusUpdate = millis();
  }
  
  // Handle app running - pass control to app loop
  if (_state == OSState::RUNNING_APP) {
    appMgr->loopCurrentApp();
    
    // MENU button always exits app (hold for 1s)
    if (input->isHeld(KeyCode::MENU)) {
      appMgr->stopCurrent();
      setState(OSState::HOME_SCREEN);
    }
    return;
  }
  
  // Render current screen
  switch (_state) {
    case OSState::HOME_SCREEN:
      _renderHomeScreen();
      _handleHomeInput();
      break;
      
    case OSState::APP_DRAWER:
      _renderAppDrawer();
      _handleDrawerInput();
      break;
      
    case OSState::NOTIFICATION_PANEL:
      _renderNotificationPanel();
      _handleNotifInput();
      break;
      
    case OSState::QUICK_SETTINGS:
      _renderQuickSettings();
      _handleQuickSettInput();
      break;
      
    case OSState::LOCK_SCREEN:
      _renderLockScreen();
      if (input->anyKeyPressed) setState(OSState::HOME_SCREEN);
      break;
      
    default:
      break;
  }
  
  delay(16); // ~60fps
}

// ─────────────────────────────────────────────────────────────────
//  Hardware initialization
// ─────────────────────────────────────────────────────────────────
void LegacyOS::_initHardware() {
  display->init();
  input->init();
  
  Serial.printf("[OS] Chip: %s @ %d MHz\n", ESP.getChipModel(), ESP.getCpuFreqMHz());
  Serial.printf("[OS] Flash: %d MB | PSRAM: %d MB\n",
    ESP.getFlashChipSize()/1048576, ESP.getPsramSize()/1048576);
  Serial.printf("[OS] Free Heap: %d KB\n", ESP.getFreeHeap()/1024);
}

// ─────────────────────────────────────────────────────────────────
//  Subsystem initialization
// ─────────────────────────────────────────────────────────────────
void LegacyOS::_initSubsystems() {
  display->showBootProgress(15, "Mounting filesystem...");
  fs->init();
  
  display->showBootProgress(30, "Loading settings...");
  prefs.begin("legacyos", false);
  network->init(prefs);
  
  display->showBootProgress(45, "Init scripting engines...");
  lua->init();
  js->init();
  
  display->showBootProgress(60, "Init emulation layer...");
  retro->init();
  
  display->showBootProgress(75, "Registering apps...");
  AppContext ctx = { this, display, input, theme, fs, network, lua, js, retro };
  appMgr->init(ctx);
  
  // Register Lua/JS/Retro apps
  _registerScriptApps();
  
  display->showBootProgress(88, "Connecting WiFi...");
  // Try auto WiFi connect (non-blocking timeout)
  WiFi.mode(WIFI_STA);
  if (!network->wifiAutoConnect()) {
    Serial.println(F("[OS] No saved WiFi profile"));
  }
  
  display->showBootProgress(95, "Finalizing...");
}

void LegacyOS::_registerScriptApps() {
  AppInfo* luaApp = appMgr->findApp("lua");
  if (luaApp) { luaApp->onStart = AppLuaIDE::start; luaApp->onLoop = AppLuaIDE::loop; }

  AppInfo* jsApp = appMgr->findApp("js");
  if (jsApp) { jsApp->onStart = AppJSRun::start; jsApp->onLoop = AppJSRun::loop; }

  AppInfo* retroApp = appMgr->findApp("retro");
  if (retroApp) { retroApp->onStart = AppRetroEmu::start; retroApp->onLoop = AppRetroEmu::loop; }

  AppInfo* loraApp = appMgr->findApp("lora");
  if (loraApp) { loraApp->onStart = AppLoRa::start; loraApp->onLoop = AppLoRa::loop; }

  AppInfo* monApp = appMgr->findApp("sysmon");
  if (monApp) { monApp->onStart = AppSysMon::start; monApp->onLoop = AppSysMon::loop; }

  AppInfo* edApp = appMgr->findApp("editor");
  if (edApp) { edApp->onStart = AppTextEditor::start; edApp->onLoop = AppTextEditor::loop; }

  appMgr->registerApp({"music", "Music", "P", CLR_APP_PURPLE, "tools", true,
    AppMusicPlayer::start, nullptr, nullptr, nullptr, AppMusicPlayer::loop});

  appMgr->registerApp({"snake", "Snake", "~", CLR_APP_GREEN, "games", true,
    AppSnake::start, nullptr, nullptr, nullptr, AppSnake::loop});
}

void LegacyOS::_loadSettings() {
  _homeWallpaper = prefs.getInt("wallpaper", 0);
  theme->mode    = (UITheme::ThemeMode)prefs.getInt("theme", 0);
}

// ─────────────────────────────────────────────────────────────────
//  STATE MANAGEMENT
// ─────────────────────────────────────────────────────────────────
void LegacyOS::setState(OSState s) {
  OSState prev = _state;
  _state = s;
  
  // Transitions
  if (prev == OSState::RUNNING_APP && s == OSState::HOME_SCREEN) {
    appMgr->stopCurrent();
    display->clearContent();
    display->clearStatus();
  }
  
  if (s == OSState::RUNNING_APP) {
    // App will take over the display
    display->clearContent(CLR_BG);
  }
  
  if (s == OSState::HOME_SCREEN) {
    _appDrawerScroll = 0;
    _appDrawerSelected = 0;
  }
  
  Serial.printf("[OS] State: %d -> %d\n", (int)prev, (int)s);
}

// ─────────────────────────────────────────────────────────────────
//  HOME SCREEN
// ─────────────────────────────────────────────────────────────────
void LegacyOS::_renderHomeScreen() {
  auto* spr = display->getContent();
  spr->fillSprite(CLR_BG);
  
  // Wallpaper gradient
  for (int y = 0; y < 300; y += 3) {
    uint16_t c = spr->alphaBlend(map(y, 0, 300, 0, 180), CLR_BG, CLR_BG2);
    spr->drawFastHLine(0, y, 240, c);
  }
  
  // Time widget - large
  String timeStr = network->getTimeString();
  spr->setTextColor(CLR_WHITE);
  spr->setTextSize(4);
  spr->setTextDatum(MC_DATUM);
  spr->drawString(timeStr, 120, 60);
  
  // Date
  spr->setTextColor(CLR_GRAY1);
  spr->setTextSize(1);
  spr->drawString(network->getDateString(), 120, 90);
  
  // Notification badge if any
  if (!_notifications.empty()) {
    int unread = 0;
    for (auto& n : _notifications) if (!n.read) unread++;
    if (unread > 0) {
      spr->fillCircle(215, 6, 8, CLR_ACCENT);
      spr->setTextColor(CLR_WHITE);
      spr->setTextDatum(MC_DATUM);
      spr->setTextSize(1);
      spr->drawString(String(min(unread, 9)), 215, 6);
    }
  }
  
  // Quick-access dock (bottom 3 apps)
  spr->fillRoundRect(5, 248, 230, 50, 12, 0x2104);
  const char* dock[] = {"⚙", "W", "F"};
  const char* dockLabel[] = {"Settings","WiFi","Files"};
  uint16_t dockClr[] = {CLR_APP_BLUE, CLR_APP_GREEN, CLR_APP_TEAL};
  for (int i = 0; i < 3; i++) {
    int x = 15 + i * 76;
    spr->fillRoundRect(x, 253, 42, 40, 8, dockClr[i]);
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(2);
    spr->setTextDatum(MC_DATUM);
    spr->drawString(dock[i], x + 21, 270);
    spr->setTextSize(1);
    spr->drawString(dockLabel[i], x + 21, 287);
  }
  
  // App drawer handle
  spr->fillRoundRect(95, 233, 50, 8, 4, CLR_GRAY3);
  
  // Status widgets
  _drawHomeWidgets();
  
  display->pushContent();
  _renderStatusBar();
}

void LegacyOS::_drawHomeWidgets() {
  auto* spr = display->getContent();
  
  // WiFi widget
  spr->fillRoundRect(5, 108, 110, 55, 8, CLR_BG2);
  spr->setTextColor(CLR_CYAN);
  spr->setTextSize(1);
  spr->setTextDatum(ML_DATUM);
  spr->drawString(network->wifiConnected ? "WiFi" : "WiFi Off", 12, 120);
  if (network->wifiConnected) {
    spr->setTextColor(CLR_WHITE);
    String ssid = network->currentSSID;
    if (ssid.length() > 12) ssid = ssid.substring(0, 12) + "..";
    spr->drawString(ssid, 12, 133);
    spr->setTextColor(CLR_GRAY2);
    spr->drawString(network->localIP, 12, 147);
  } else {
    spr->setTextColor(CLR_GRAY3);
    spr->drawString("Not connected", 12, 133);
  }
  
  // BLE widget
  spr->fillRoundRect(125, 108, 110, 55, 8, CLR_BG2);
  spr->setTextColor(CLR_PRIMARY);
  spr->drawString(network->bleEnabled ? "BLE On" : "BLE Off", 132, 120);
  if (network->bleEnabled && network->bleConnected) {
    spr->setTextColor(CLR_GREEN);
    spr->drawString("Connected", 132, 133);
  } else {
    spr->setTextColor(CLR_GRAY3);
    spr->drawString(network->bleEnabled ? "Advertising" : "Disabled", 132, 133);
  }
  
  // Storage widget
  spr->fillRoundRect(5, 170, 230, 55, 8, CLR_BG2);
  spr->setTextColor(CLR_YELLOW);
  spr->drawString("Storage", 12, 182);
  
  if (fs->sdMounted) {
    spr->setTextColor(CLR_WHITE);
    char buf[40];
    snprintf(buf, 40, "SD: %llu MB / %llu MB",
      fs->sdUsedBytes/1048576, fs->sdTotalBytes/1048576);
    spr->drawString(buf, 12, 194);
    int sdPct = (int)(fs->sdUsedBytes * 100 / max(fs->sdTotalBytes, (uint64_t)1));
    UITheme::drawProgressBar(spr, 12, 208, 210, 8, sdPct, CLR_YELLOW, CLR_GRAY3);
  } else {
    spr->setTextColor(CLR_GRAY3);
    spr->drawString("No SD card", 12, 194);
  }
}

void LegacyOS::_handleHomeInput() {
  // DOWN or START -> App drawer
  if (input->down() || input->start()) {
    setState(OSState::APP_DRAWER);
    return;
  }
  
  // UP -> Notification panel
  if (input->up()) {
    setState(OSState::NOTIFICATION_PANEL);
    return;
  }
  
  // MENU -> Quick settings
  if (input->option()) {
    setState(OSState::QUICK_SETTINGS);
    return;
  }
  
  // A -> Settings shortcut
  if (input->select()) {
    appMgr->launch("settings");
    setState(OSState::RUNNING_APP);
    return;
  }
  
  // B -> Lock screen
  if (input->isHeld(KeyCode::B)) {
    setState(OSState::LOCK_SCREEN);
    return;
  }
}

// ─────────────────────────────────────────────────────────────────
//  STATUS BAR
// ─────────────────────────────────────────────────────────────────
void LegacyOS::_renderStatusBar() {
  auto* spr = display->getStatus();
  spr->fillSprite(CLR_STATUS_BG);
  
  // Time
  String t = network->getTimeString();
  spr->setTextColor(CLR_WHITE);
  spr->setTextSize(1);
  spr->setTextDatum(ML_DATUM);
  spr->drawString(t, 5, 12);
  
  // Right side icons
  int rx = 235;
  
  // Battery
  UITheme::drawBattery(spr, rx - 20, 6, _battery, _charging);
  rx -= 24;
  
  // WiFi signal
  if (network->wifiEnabled) {
    UITheme::drawWifiIcon(spr, rx - 18, 6, network->getSignalStrength());
    rx -= 22;
  }
  
  // BLE indicator
  if (network->bleEnabled) {
    spr->setTextColor(CLR_CYAN);
    spr->setTextSize(1);
    spr->drawString("B", rx - 8, 12);
    rx -= 12;
  }
  
  // Notification dot
  if (!_notifications.empty()) {
    spr->fillCircle(rx - 4, 10, 3, CLR_ACCENT);
    rx -= 10;
  }
  
  // SD card indicator
  if (fs->sdMounted) {
    spr->setTextColor(CLR_GREEN);
    spr->setTextSize(1);
    spr->drawString("SD", rx - 12, 12);
    rx -= 16;
  }
  
  display->pushStatus();
}

// ─────────────────────────────────────────────────────────────────
//  APP DRAWER
// ─────────────────────────────────────────────────────────────────
void LegacyOS::_renderAppDrawer() {
  auto* spr = display->getContent();
  spr->fillSprite(CLR_BG);
  
  // Header
  spr->fillRect(0, 0, 240, 22, CLR_BG2);
  spr->setTextColor(CLR_WHITE);
  spr->setTextSize(1);
  spr->setTextDatum(ML_DATUM);
  spr->drawString("All Apps", 8, 12);
  spr->setTextColor(CLR_GRAY2);
  char cntBuf[16];
  snprintf(cntBuf, 16, "%d apps", (int)appMgr->apps.size());
  spr->setTextDatum(MR_DATUM);
  spr->drawString(cntBuf, 232, 12);
  
  // 4-column grid
  int cols = 4;
  int rows = 3;
  int startIdx = _appDrawerScroll * cols;
  
  for (int i = 0; i < cols * rows; i++) {
    int appIdx = startIdx + i;
    if (appIdx >= (int)appMgr->apps.size()) break;
    
    AppInfo& app = appMgr->apps[appIdx];
    int col = i % cols;
    int row = i / cols;
    int x = ICON_START_X + col * ICON_SPACING_X;
    int y = 28 + row * ICON_SPACING_Y;
    
    bool sel = (appIdx == _appDrawerSelected);
    UITheme::drawAppIcon(spr, x, y, app.icon.c_str(), app.name.c_str(), 
                         app.iconBg, sel);
  }
  
  // Page indicator dots
  int totalPages = (appMgr->apps.size() + cols*rows - 1) / (cols*rows);
  int curPage = _appDrawerScroll;
  for (int p = 0; p < totalPages && p < 8; p++) {
    int dx = 120 - totalPages*5 + p*10;
    spr->fillCircle(dx, 228, 2, p == curPage ? CLR_PRIMARY : CLR_GRAY3);
  }
  
  // Bottom bar
  spr->fillRect(0, 235, 240, 18, CLR_BG2);
  spr->setTextColor(CLR_GRAY2);
  spr->setTextSize(1);
  spr->setTextDatum(MC_DATUM);
  
  if (appMgr->apps.size() > (size_t)_appDrawerSelected) {
    spr->drawString(appMgr->apps[_appDrawerSelected].name + "  A:Open  B:Home", 120, 243);
  }
  
  display->pushContent();
  _renderStatusBar();
}

void LegacyOS::_handleDrawerInput() {
  int cols = 4;
  int rows = 3;
  int pageSize = cols * rows;
  int totalApps = appMgr->apps.size();
  int startIdx = _appDrawerScroll * pageSize;
  
  if (input->right()) {
    if (_appDrawerSelected < totalApps - 1) {
      _appDrawerSelected++;
      if (_appDrawerSelected >= startIdx + pageSize) _appDrawerScroll++;
    }
  }
  if (input->left()) {
    if (_appDrawerSelected > 0) {
      _appDrawerSelected--;
      if (_appDrawerSelected < startIdx) {
        _appDrawerScroll = max(0, _appDrawerScroll - 1);
      }
    }
  }
  if (input->down()) {
    int newSel = _appDrawerSelected + cols;
    if (newSel < totalApps) {
      _appDrawerSelected = newSel;
      if (_appDrawerSelected >= startIdx + pageSize) _appDrawerScroll++;
    }
  }
  if (input->up()) {
    int newSel = _appDrawerSelected - cols;
    if (newSel >= 0) {
      _appDrawerSelected = newSel;
      if (_appDrawerSelected < startIdx) _appDrawerScroll = max(0, _appDrawerScroll - 1);
    } else if (_appDrawerScroll == 0) {
      setState(OSState::HOME_SCREEN);
      return;
    }
  }
  
  if (input->b()) {
    setState(OSState::HOME_SCREEN);
    return;
  }
  
  if (input->start()) {
    if (_appDrawerSelected < totalApps) {
      String id = appMgr->apps[_appDrawerSelected].id;
      appMgr->launch(id);
      setState(OSState::RUNNING_APP);
    }
    return;
  }
}

// ─────────────────────────────────────────────────────────────────
//  NOTIFICATION PANEL
// ─────────────────────────────────────────────────────────────────
void LegacyOS::_renderNotificationPanel() {
  auto* spr = display->getContent();
  spr->fillSprite(CLR_BG);
  
  // Semi-transparent overlay effect
  spr->fillRoundRect(0, 0, 240, 280, 0, CLR_BG2);
  
  // Header
  spr->fillRect(0, 0, 240, 22, CLR_PRIMARY);
  spr->setTextColor(CLR_WHITE);
  spr->setTextSize(1);
  spr->setTextDatum(ML_DATUM);
  spr->drawString("Notifications", 8, 12);
  
  char nbuf[8];
  snprintf(nbuf, 8, "%d", (int)_notifications.size());
  spr->setTextDatum(MR_DATUM);
  spr->drawString(nbuf, 232, 12);
  
  if (_notifications.empty()) {
    spr->setTextColor(CLR_GRAY2);
    spr->setTextDatum(MC_DATUM);
    spr->drawString("No notifications", 120, 140);
  } else {
    int visRows = 11;
    for (int i = 0; i < visRows && (i + _notifScroll) < (int)_notifications.size(); i++) {
      int idx = i + _notifScroll;
      Notification& n = _notifications[idx];
      int y = 26 + i * 24;
      
      uint16_t bg = n.read ? CLR_BG : CLR_BG2;
      spr->fillRoundRect(3, y, 234, 22, 4, bg);
      
      // Icon
      spr->setTextColor(CLR_PRIMARY);
      spr->setTextSize(1);
      spr->setTextDatum(ML_DATUM);
      spr->drawString(n.icon, 8, y + 10);
      
      // Title + message
      spr->setTextColor(n.read ? CLR_GRAY2 : CLR_WHITE);
      spr->drawString(n.title, 24, y + 7);
      spr->setTextColor(CLR_GRAY2);
      spr->drawString(n.message, 24, y + 16);
      
      if (!n.read) {
        spr->fillCircle(232, y + 10, 3, CLR_ACCENT);
      }
    }
  }
  
  spr->fillRect(0, 282, 240, 18, CLR_BG2);
  spr->setTextColor(CLR_GRAY2);
  spr->setTextSize(1);
  spr->setTextDatum(MC_DATUM);
  spr->drawString("B:Clear All  A:Close", 120, 290);
  
  display->pushContent();
  _renderStatusBar();
}

void LegacyOS::_handleNotifInput() {
  if (input->up())   { if (_notifScroll > 0) _notifScroll--; }
  if (input->down()) { if (_notifScroll < (int)_notifications.size()-11) _notifScroll++; }
  if (input->b())    { setState(OSState::HOME_SCREEN); }
  if (input->b()) { _notifications.clear(); _notifScroll = 0; }
  if (input->start() && _notifScroll < (int)_notifications.size()) {
    _notifications[_notifScroll].read = true;
  }
}

// ─────────────────────────────────────────────────────────────────
//  QUICK SETTINGS
// ─────────────────────────────────────────────────────────────────
void LegacyOS::_renderQuickSettings() {
  auto* spr = display->getContent();
  spr->fillSprite(CLR_BG);
  spr->fillRoundRect(0, 0, 240, 300, 0, 0x1082);
  
  spr->fillRect(0, 0, 240, 20, CLR_PRIMARY);
  spr->setTextColor(CLR_WHITE);
  spr->setTextSize(1);
  spr->setTextDatum(ML_DATUM);
  spr->drawString("Quick Settings", 8, 12);
  
  // Toggle tiles
  struct QSTile { const char* icon; const char* name; bool* active; uint16_t clr; };
  QSTile tiles[] = {
    {"W", "WiFi",    &network->wifiEnabled,  CLR_APP_BLUE},
    {"B", "BLE",     &network->bleEnabled,   CLR_APP_PURPLE},
    {"S", "SD Card", &fs->sdMounted,         CLR_APP_GREEN},
    {"D", "Dark",    nullptr,                CLR_APP_TEAL},
  };
  
  for (int i = 0; i < 4; i++) {
    int col = i % 2, row = i / 2;
    int x = 10 + col * 115;
    int y = 28 + row * 75;
    
    bool on = tiles[i].active ? *tiles[i].active : true;
    uint16_t bg = on ? tiles[i].clr : CLR_GRAY3;
    
    spr->fillRoundRect(x, y, 105, 65, 10, bg);
    spr->setTextColor(CLR_WHITE);
    spr->setTextSize(3);
    spr->setTextDatum(ML_DATUM);
    spr->drawString(tiles[i].icon, x + 8, y + 22);
    spr->setTextSize(1);
    spr->drawString(tiles[i].name, x + 8, y + 50);
    
    // On/Off indicator
    spr->setTextColor(on ? CLR_WHITE : CLR_GRAY2);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(on ? "ON" : "OFF", x + 100, y + 50);
  }
  
  // Brightness slider
  spr->setTextColor(CLR_WHITE);
  spr->setTextSize(1);
  spr->setTextDatum(ML_DATUM);
  spr->drawString("Brightness", 10, 183);
  UITheme::drawProgressBar(spr, 10, 193, 220, 12, 100, CLR_YELLOW, CLR_GRAY3);
  
  // Volume slider
  spr->drawString("Volume", 10, 215);
  UITheme::drawProgressBar(spr, 10, 225, 220, 12, 75, CLR_CYAN, CLR_GRAY3);
  
  // Device info bar
  spr->fillRect(0, 245, 240, 18, CLR_BG2);
  char info[50];
  snprintf(info, 50, "Bat: %d%%  Heap: %dK  Up: %ds",
    _battery, ESP.getFreeHeap()/1024, (int)(millis()/1000));
  spr->setTextColor(CLR_GRAY2);
  spr->setTextDatum(MC_DATUM);
  spr->drawString(info, 120, 253);
  
  spr->fillRect(0, 264, 240, 18, CLR_BG2);
  spr->setTextColor(CLR_GRAY2);
  spr->drawString("OPTION:Settings  A:Home", 120, 272);
  
  display->pushContent();
  _renderStatusBar();
}

void LegacyOS::_handleQuickSettInput() {
  if (input->a() || input->option()) {
    if (input->option()) {
      appMgr->launch("settings");
      setState(OSState::RUNNING_APP);
    } else {
      setState(OSState::HOME_SCREEN);
    }
  }
}

// ─────────────────────────────────────────────────────────────────
//  LOCK SCREEN
// ─────────────────────────────────────────────────────────────────
void LegacyOS::_renderLockScreen() {
  auto* spr = display->getContent();
  spr->fillSprite(CLR_BLACK);
  
  // Gradient background
  for (int y = 0; y < 300; y += 4) {
    spr->drawFastHLine(0, y, 240, spr->color565(0, 0, y/4));
  }
  
  spr->setTextColor(CLR_WHITE);
  spr->setTextSize(4);
  spr->setTextDatum(MC_DATUM);
  spr->drawString(network->getTimeString(), 120, 100);
  
  spr->setTextColor(CLR_GRAY1);
  spr->setTextSize(1);
  spr->drawString(network->getDateString(), 120, 135);
  
  // Lock icon
  spr->drawRoundRect(105, 160, 30, 40, 4, CLR_GRAY2);
  spr->fillRect(108, 172, 24, 24, CLR_GRAY2);
  spr->fillCircle(120, 184, 5, CLR_BG);
  spr->fillRect(118, 183, 4, 10, CLR_BG);
  
  spr->setTextColor(CLR_GRAY2);
  spr->drawString("Press any key to unlock", 120, 230);
  
  UITheme::drawBattery(spr, 108, 260, _battery, _charging);
  
  display->pushContent();
  display->getStatus()->fillSprite(CLR_BLACK);
  display->pushStatus();
}

// ─────────────────────────────────────────────────────────────────
//  UTILITY
// ─────────────────────────────────────────────────────────────────
void LegacyOS::pushNotification(const String& title, const String& msg, const String& icon) {
  Notification n;
  n.title     = title;
  n.message   = msg;
  n.icon      = icon;
  n.timestamp = millis();
  n.read      = false;
  
  _notifications.push_back(n);
  if (_notifications.size() > 50) _notifications.erase(_notifications.begin());
  
  Serial.printf("[Notif] %s: %s\n", title.c_str(), msg.c_str());
}

void LegacyOS::_updateBatteryStatus() {
  _battery  = HW::batteryPercent();
  // Charging detection would require hardware voltage comparison
  _charging = false;
}

int LegacyOS::getWifiStrength() {
  return network->getSignalStrength();
}

void LegacyOS::reboot() {
  prefs.end();
  display->fadeOut();
  Serial.println(F("[OS] Rebooting..."));
  delay(500);
  ESP.restart();
}

void LegacyOS::sleep() {
  display->fadeOut();
  Serial.println(F("[OS] Deep sleep..."));
  esp_deep_sleep_start();
}

// Additional app registrations are handled inline in _registerScriptApps above
// Music and Snake are added via registerApp() calls
