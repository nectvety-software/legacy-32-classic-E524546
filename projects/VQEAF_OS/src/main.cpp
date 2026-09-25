#include <Arduino.h>
#include "core/BuildVersion.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <esp_heap_caps.h>
#include "BoardConfig.h"
#include "core/BuildSanity.h"
#include "core/Types.h"
#include "core/InputManager.h"
#include "core/SymbianUI.h"
#if defined(VQEAF_PERF_DIAG)
#include "core/UiPerfCounter.h"
#endif
#if defined(VQEAF_ICON_SELFTEST)
#include "core/VqeafIconSelfTest.h"
#endif
#include "core/TextKeyboard.h"
#include "services/SettingsStore.h"
#include "services/StorageService.h"
#include "services/MusicService.h"
#include "services/NotificationService.h"
#include "services/SystemService.h"
#include "services/WiFiProfileStore.h"
#include "services/ShellService.h"
#include "services/BoardDiagnostics.h"
#include "services/TrustedTls.h"
#include "services/ThemeFileService.h"
#include "core/GlobalShortcutPolicy.h"
#include "services/AppInstallerService.h"
#include "services/QeappDataService.h"
#include "apps/Apps.h"
#include "apps/PixelSnakeApp.h"

static TFT_eSPI tft;
static VqeafUI ui(tft); // compatibility adapter over the existing UI implementation
static InputManager input;
static SettingsStore settings;
static StorageService storage;
static MusicService music;
static TextKeyboard keyboard;
static NotificationService notifications;
static SystemService systemService;
static WiFiProfileStore wifiProfiles;
static WiFiConnectionService wifiConnection;
static BrowserService browserService;
static ImageViewerService imageViewer;
static ShellService shellService;
static ThemeFileService themeFiles;
static AppInstallerService appInstaller;
static QeappDataService appData;
static AppContext appCtx{ui, storage, settings, music, keyboard, notifications, systemService, wifiProfiles, wifiConnection, browserService, imageViewer, shellService, themeFiles, appInstaller, appData};

static LauncherApp launcher; // 3x4 menu from photo
static ExplorerApp explorer; // optional Retro-Go-inspired tab/list explorer
static WiFiApp wifiApp;
static BleApp bleApp;
static FilesApp filesApp;
static CollectionApp collectionApp;
static MusicApp musicApp;
static CalculatorApp calculatorApp;
static StopwatchApp stopwatchApp;
static GalleryApp galleryApp;
static TextViewerApp textViewerApp;
static BrowserApp browserApp;
static ShellApp shellApp;
static RecoveryApp recoveryApp;
static SettingsApp settingsApp;
static ThemesApp themesApp;
static ApplicationsApp applicationsApp;
static AppInstallerApp appInstallerApp;
static QuickPanelApp quickPanelApp;
static TaskSwitcherApp taskSwitcherApp;
static NotificationCenterApp notificationApp;
static NotesApp notesApp;
static PixelSnakeApp pixelSnakeApp;

static ScreenId screen = ScreenId::Splash;
static ScreenId detailReturn = ScreenId::Applications;
static uint32_t splashAt = 0;
static uint32_t lastClockRefresh = 0;
static uint32_t lastActivityAt = 0;
static uint32_t lockAt = 0;
static int idleShortcut = 0;
static bool ntpConfigured = false;
static bool timeSyncedNotified = false;
static uint32_t lastTimeCheck = 0;
static bool lockDimmed = false;
static bool wifiStateInitialized = false;
static bool lastWifiConnected = false;
static String lastIdleWiFiText;
static bool lastIdleWiFiConnected = false;
static uint32_t lastIdleWiFiRefresh = 0;
static uint32_t lastAppWifiBadgeRefresh = 0;

static bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

static void setBacklight(uint8_t percent) {
  percent = constrain(percent, 0, 100);
  analogWrite(Board::TFT_LEDK_PIN, map(percent, 0, 100, 0, 255));
}

static void restoreBacklight() {
  setBacklight(settings.data().brightness);
}

static String wifiHomeText() {
  if (wifiConnected()) {
    // 240px: clip is performed in idleNetworkStatus using rendered width.
    return String("WiFi ") + WiFi.SSID() + "  " + String(WiFi.RSSI()) + "dBm";
  }
  if (systemService.safeMode()) return "WiFi: Safe Mode (manual)";
  if (!settings.data().wifiAuto) return "WiFi: auto-connect off";
  switch (wifiConnection.autoPhase()) {
    case WiFiConnectionService::AutoPhase::Scheduled: return "WiFi: preparing scan";
    case WiFiConnectionService::AutoPhase::Scanning: return "WiFi: scanning saved...";
    case WiFiConnectionService::AutoPhase::Connecting:
      return String("WiFi: joining ") + wifiConnection.requestedSSID();
    case WiFiConnectionService::AutoPhase::Waiting:
      return String("WiFi: retry in ") + String(wifiConnection.retrySeconds()) + "s";
    case WiFiConnectionService::AutoPhase::Paused:
      return wifiConnection.phase() == WiFiConnectionService::Phase::Failed ?
          "WiFi: connection failed" : "WiFi: manual / offline";
    default: return wifiProfiles.count() ? "WiFi: disconnected" : "WiFi: no saved networks";
  }
}

static void drawIdle() {
  lastIdleWiFiText = wifiHomeText();
  lastIdleWiFiConnected = wifiConnected();
  ui.idleHome(lastIdleWiFiConnected, false, false, settings.data().hour12,
              idleShortcut, notifications.unreadCount(), music.playing(), lastIdleWiFiText);
  lastIdleWiFiRefresh = millis();
}

static void updateIdleWiFiStatus() {
  if (screen != ScreenId::Idle) return;
  const uint32_t now = millis();
  // Signal strength is informational; redraw it at most every 5s while
  // connected, while boot scan/retry countdown can update once per second.
  const uint32_t interval = wifiConnected() ? 5000UL : 1000UL;
  if ((uint32_t)(now - lastIdleWiFiRefresh) < interval &&
      lastIdleWiFiConnected == wifiConnected()) return;
  const String next = wifiHomeText();
  const bool live = wifiConnected();
  if (next != lastIdleWiFiText || live != lastIdleWiFiConnected) {
    ui.idleNetworkStatus(next, live);
    lastIdleWiFiText = next;
    lastIdleWiFiConnected = live;
  }
  lastIdleWiFiRefresh = now;
}

static void drawLock() {
  ui.lockScreen(wifiConnected(), false, false, settings.data().hour12,
                notifications.unreadCount(), lockDimmed);
}

static void drawClock(bool full = true) {
  if (full) {
    ui.chrome("Clock", wifiConnected(), false, false, settings.data().hour12);
    ui.clockFace(settings.data().hour12, true);
    ui.softkeys("", "", "Back");
  } else {
    ui.chrome("Clock", wifiConnected(), false, false, settings.data().hour12);
    ui.clockFace(settings.data().hour12, false);
  }
}

static bool disabledInSafeMode(ScreenId s) {
  return s == ScreenId::BLE || s == ScreenId::Music || s == ScreenId::Browser;
}

static bool shouldShowOpening(ScreenId from, ScreenId to) {
  if (!SystemService::isTaskScreen(to)) return false;
  return from == ScreenId::Idle || from == ScreenId::Launcher || from == ScreenId::Explorer ||
         from == ScreenId::Applications || from == ScreenId::Collection ||
         from == ScreenId::Files || from == ScreenId::QuickPanel ||
         from == ScreenId::TaskSwitcher || from == ScreenId::AppInstaller;
}

static void enterScreen(ScreenId s, bool animate = true, bool resume = false) {
  ScreenId from = screen;
  // A mere WiFi-list browse must not permanently disable saved-AP recovery;
  // explicit manual Connect/Disconnect still takes priority.
  if (from == ScreenId::WiFi && s != ScreenId::WiFi) {
    wifiConnection.resumeAutoAfterBrowsing();
  }
  String packageOpeningName;
  if (s == ScreenId::PackageApp) {
    Qeapp::Meta meta;
    if (!appInstaller.get(appCtx.pendingPackageId, meta)) {
      notifications.push("Applications", "Installed application unavailable");
      s = ScreenId::Applications;
    } else {
      packageOpeningName = meta.name;
      appCtx.pendingPackageLaunch = true;
      if (!strcmp(meta.id, "snake_pixel") && !strcmp(meta.type, "text")) {
        if (pixelSnakeApp.enter(appCtx)) s = ScreenId::Snake;
        else { notifications.push("Pixel Snake", "Invalid signed game settings"); s = ScreenId::Applications; }
        // Browser and Text Viewer normally consume this flag. Snake does not.
        appCtx.pendingPackageLaunch = false;
      } else if (!strcmp(meta.type, "web")) {
        appCtx.pendingBrowserUrl = meta.entry;
        s = ScreenId::Browser;
      } else {
        appCtx.pendingOpenPath = appInstaller.installedPath(meta.id) + "/payload.txt";
        s = ScreenId::TextViewer;
      }
    }
    appCtx.pendingPackageId = "";
  }

  if (systemService.safeMode() && disabledInSafeMode(s)) {
    notifications.push("Safe Mode", String(SystemService::screenName(s)) + " is disabled in Safe Mode");
    s = ScreenId::Recovery;
    resume = false;
    appCtx.pendingPackageLaunch = false;
    appCtx.pendingBrowserUrl = "";
  }

  if (from == ScreenId::Launcher || s == ScreenId::Launcher ||
      from == ScreenId::Explorer || s == ScreenId::Explorer) ui.invalidateChrome();
  if (animate && from != ScreenId::Launcher && s != ScreenId::Launcher &&
      from != ScreenId::Explorer && s != ScreenId::Explorer &&
      screen != ScreenId::Splash && s != ScreenId::Lock) ui.transitionOut();
  if (animate && shouldShowOpening(from, s) && s != ScreenId::Recovery) {
    ui.chrome("Opening", wifiConnected(), false, false, settings.data().hour12);
    ui.openingApp(packageOpeningName.length() ? packageOpeningName : SystemService::screenName(s),
                  packageOpeningName.length() ? "App" : SystemService::screenIcon(s), resume);
    // Keep interstitial visible without a 170ms input/audio service stall.
    delay(32);
  }

  if (from == ScreenId::Files && (s == ScreenId::Themes || s == ScreenId::AppInstaller)) {
    Serial.printf("[VQEAF][FILE] %s => %s\n", s == ScreenId::Themes ? "VQEAF" : "QEAPP",
                  s == ScreenId::Themes ? "Themes" : "App installer");
  }
  screen = s;
  if (s != ScreenId::Idle && s != ScreenId::Lock && s != ScreenId::Splash) ui.clearContent();
  switch (s) {
    case ScreenId::Idle:
      restoreBacklight(); drawIdle(); break;
    case ScreenId::Launcher:
      if (!resume) launcher.enter(); launcher.draw(appCtx); break;
    case ScreenId::Explorer:
      if (!resume) explorer.enter(); explorer.draw(appCtx); break;
    case ScreenId::WiFi:
      if (!resume) wifiApp.enter(appCtx); wifiApp.draw(appCtx); break;
    case ScreenId::BLE:
      if (!resume) bleApp.enter(appCtx); bleApp.draw(appCtx); break;
    case ScreenId::Music:
      if (!resume) musicApp.enter(appCtx); musicApp.draw(appCtx); break;
    case ScreenId::Calculator:
      if (!resume) calculatorApp.enter(); calculatorApp.draw(appCtx); break;
    case ScreenId::Stopwatch:
      if (!resume) stopwatchApp.enter(); stopwatchApp.draw(appCtx); break;
    case ScreenId::Files:
      if (!resume) filesApp.enter(appCtx); filesApp.draw(appCtx); break;
    case ScreenId::Collection:
      if (!resume) collectionApp.enter(appCtx); collectionApp.draw(appCtx); break;
    case ScreenId::Gallery:
      if (!resume) galleryApp.enter(appCtx); galleryApp.draw(appCtx); break;
    case ScreenId::TextViewer:
      if (!resume) textViewerApp.enter(appCtx); textViewerApp.draw(appCtx); break;
    case ScreenId::Browser:
      if (!resume) browserApp.enter(appCtx); browserApp.draw(appCtx); break;
    case ScreenId::Snake:
      pixelSnakeApp.draw(appCtx); break;
    case ScreenId::Shell:
      if (!resume) shellApp.enter(appCtx); shellApp.draw(appCtx); break;
    case ScreenId::Settings:
      if (!resume) settingsApp.enter(); settingsApp.draw(appCtx); break;
    case ScreenId::Themes:
      if (!resume) themesApp.enter(appCtx, from); themesApp.draw(appCtx); break;
    case ScreenId::Applications:
      if (!resume) applicationsApp.enter(appCtx); applicationsApp.draw(appCtx); break;
    case ScreenId::AppInstaller:
      if (!resume) appInstallerApp.enter(appCtx, from); appInstallerApp.draw(appCtx); break;
    case ScreenId::QuickPanel:
      if (!resume) quickPanelApp.enter(); quickPanelApp.draw(appCtx); break;
    case ScreenId::TaskSwitcher:
      taskSwitcherApp.enter(appCtx, from); taskSwitcherApp.draw(appCtx); break;
    case ScreenId::Notifications:
      if (!resume) notificationApp.enter(appCtx, from); notificationApp.draw(appCtx); break;
    case ScreenId::Notes:
      if (!resume) notesApp.enter(); notesApp.draw(appCtx); break;
    case ScreenId::Recovery:
      if (!resume) recoveryApp.enter(); recoveryApp.draw(appCtx); break;
    case ScreenId::Lock:
      lockAt = millis(); lockDimmed = false; restoreBacklight(); drawLock(); break;
    case ScreenId::Clock:
      drawClock(true); break;
    case ScreenId::SystemInfo: {
      ui.chrome("System info", wifiConnected(), false, false, settings.data().hour12);
      String heap = String((unsigned long)(ESP.getFreeHeap()/1024)) + " KB free heap";
      String psram = String((unsigned long)(ESP.getFreePsram()/1024)) + " KB free PSRAM";
      String reset = String("Reset: ") + systemService.lastResetReasonText();
      String mode = systemService.safeMode() ? "Mode: SAFE" : "Mode: Normal";
      ui.message("ESP32-S3 N16R8", heap, psram, reset);
      TFT_eSPI &d=ui.display(); ThemeColors c=ui.c(); d.setTextColor(c.dim,c.bg); d.setTextFont(1);
      d.setCursor(12,151); d.print(mode);
      d.setCursor(12,169); d.print(String("Boots: ") + systemService.bootCount() + "  Uptime: " + systemService.uptimeText());
      d.setCursor(12,187); d.print(String("Min heap: ") + String(systemService.minFreeHeap()/1024UL) + " KB");
      d.setCursor(12,205); d.print(String("Saved WiFi: ") + wifiProfiles.count() + "  Alerts: " + notifications.count());
      d.setCursor(12,223); d.print(String("Crash streak: ") + systemService.consecutiveCrashes());
      ui.softkeys("", "", "Back");
      break;
    }
    case ScreenId::About:
      ui.chrome("About", wifiConnected(), false, false, settings.data().hour12);
      ui.message("VQEAF OS", "v2.4 App Manager", "ESP32-S3 / 240x320 portrait", "Browser, .vqeaf, .qeapp");
      ui.softkeys("", "", "Back");
      break;
    default: break;
  }
  systemService.recordScreen(s);
  lastClockRefresh = millis();
}

static void drawSplash() {
  ui.clear();
  TFT_eSPI &d = ui.display(); ThemeColors c = ui.c();
  d.setTextColor(c.text,c.bg); d.setTextFont(2); d.setTextSize(2);
  int sw = d.textWidth("VQEAF"); d.setCursor((Board::SCREEN_W-sw)/2,88); d.print("VQEAF");
  d.setTextSize(1); d.setTextFont(2); d.setCursor(99,126); d.print("OS");
  d.setTextFont(1); d.setCursor(66,158); d.print("ESP32-S3 / 240x320");
  d.drawRect(25,205,190,12,c.dim);
  d.fillRect(27,207,160,8,c.accent);
  d.setCursor(52,229); d.print(VQEAF_OS_VERSION_TEXT);
}

static ScreenId idleShortcutTarget() {
  if (idleShortcut == 0) return ScreenId::WiFi;
  if (idleShortcut == 1) return ScreenId::Music;
  return ScreenId::Files;
}

static void updateWirelessNotifications() {
  bool connected = wifiConnected();
  if (!wifiStateInitialized) {
    wifiStateInitialized = true;
    lastWifiConnected = connected;
    return;
  }
  if (connected != lastWifiConnected) {
    if (connected) {
      notifications.push("WiFi connected", WiFi.SSID() + "  " + WiFi.localIP().toString());
      wifiProfiles.setLastSSID(WiFi.SSID());
    }
    else notifications.push("WiFi disconnected", "Network connection ended");
    lastWifiConnected = connected;
    // Idle is anti-flicker: repaint only WiFi status line after a link change.
    if (screen == ScreenId::Idle) updateIdleWiFiStatus();
    else if (screen == ScreenId::Explorer) explorer.refreshStatus(appCtx);
    else if (screen == ScreenId::Launcher) ui.refreshWifiBadge(connected, settings.data().hour12);
    else if (screen != ScreenId::Splash && screen != ScreenId::Lock && !keyboard.active())
      ui.refreshWifiBadge(connected, settings.data().hour12);
  }
}

static void updateAutoLock() {
  if (screen == ScreenId::Splash || screen == ScreenId::Lock || keyboard.active()) return;
  uint16_t timeout = settings.data().lockTimeoutSec;
  if (!timeout) return;
  if (millis() - lastActivityAt >= uint32_t(timeout) * 1000UL) {
    notifications.push("Keypad locked", "Automatic lock after inactivity");
    enterScreen(ScreenId::Lock, false);
  }
}

// Serial bench console: a small allowlisted diagnostic reader, separate from
// the interactive on-screen Shell. Never accepts arbitrary file paths or URL
// credentials; use `diag help` at 115200 baud. Trigger tests only while idle.
static char diagLine[96] = {0};
static uint8_t diagUsed = 0;
static void diagCommand(const String &cmd) {
  String c = cmd; c.trim();
  if (c == "diag help") {
    Serial.println("[S3DIAG] diag sd status | diag sd rw | diag tls valid|expired|wrong|self|host <domain>");
    Serial.println("[S3DIAG] SD removal: stop media, unplug, observe event, reinsert, diag sd rw");
    return;
  }
  if (c == "diag sd status") {
    const String status = BoardDiagnostics::sdStatus(storage);
    Serial.printf("[S3DIAG][SD] status %s\n", status.c_str());
    return;
  }
  if (c == "diag sd rw") {
    if (music.playing()) { Serial.println("[S3DIAG][SD] INCONCLUSIVE: stop audio first"); return; }
    String detail;
    auto verdict = BoardDiagnostics::testSdReadWrite(storage, detail);
    Serial.printf("[S3DIAG][SD] test=rw result=%s detail=%s\n", BoardDiagnostics::label(verdict), detail.c_str());
    return;
  }
  if (c.startsWith("diag tls ")) {
    String which = c.substring(9);
    bool accept = true;
    String host;
    if (which == "valid") host = "valid-isrgrootx1.letsencrypt.org";
    else if (which == "expired") { host = "expired.badssl.com"; accept = false; }
    else if (which == "wrong") { host = "wrong.host.badssl.com"; accept = false; }
    else if (which == "self") { host = "self-signed.badssl.com"; accept = false; }
    else if (which.startsWith("host ")) host = which.substring(5);
    if (!host.length()) { Serial.println("[S3DIAG] Unknown TLS case; diag help"); return; }
    String detail;
    auto verdict = BoardDiagnostics::testTls(host, accept, detail);
    Serial.printf("[S3DIAG][TLS] verdict=%s detail=%s\n", BoardDiagnostics::label(verdict), detail.c_str());
    return;
  }
  Serial.println("[S3DIAG] Invalid command; diag help");
}
static void diagPoll() {
  uint8_t count = 0;
  while (Serial.available() && count++ < 16) {
    const int val = Serial.read();
    if (val < 0) break;
    const char ch = char(val);
    if (ch == '\r' || ch == '\n') {
      if (diagUsed) {
        diagLine[diagUsed] = 0;
        // Never echo secrets; only the fixed allowlisted diag namespace.
        if (strncmp(diagLine, "diag ", 5) == 0) diagCommand(diagLine);
        diagUsed = 0;
      }
    } else if (ch >= 32 && ch <= 126) {
      if (diagUsed < sizeof(diagLine) - 1) diagLine[diagUsed++] = ch;
      else diagUsed = 0; // no unbounded serial line accumulation
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  AppInstallerService::printBootInstallDiagnostics();
#ifdef ARDUINO_ARCH_ESP32
  // Emit observable board/memory evidence to the 115200 serial capture.
  // USB CDC is enabled in platformio.ini; these checks only log, never
  // format NVS/SD or silently toggle recovery mode.
  Serial.printf("[VQEAF][BUILD] target=ESP32-S3 N16R8 display=240x320 portrait\n");
  const uint32_t flashBytes = ESP.getFlashChipSize();
  const uint32_t psramBytes = ESP.getPsramSize();
  Serial.printf("[VQEAF][MEM] flash=%lu psram=%lu free_heap=%lu\n",
                (unsigned long)flashBytes, (unsigned long)psramBytes,
                (unsigned long)ESP.getFreeHeap());
  if (flashBytes < 16UL * 1024UL * 1024UL)
    Serial.println("[VQEAF][WARN] flash below N16R8 requirement");
  if (psramBytes < 8UL * 1024UL * 1024UL)
    Serial.println("[VQEAF][WARN] PSRAM below N16R8 requirement");
#endif
  input.begin();
  settings.begin();
  systemService.begin();
  // A or DOWN held at boot forces the recovery screen; no GPIO changes.
  const bool physicalRecovery = digitalRead(Board::KEY_A) == LOW ||
                                digitalRead(Board::KEY_DOWN) == LOW;
  if (physicalRecovery) systemService.setSafeMode(true);
  wifiProfiles.begin();
  wifiConnection.begin(wifiProfiles);
  bool browserCoreOk = false;
  ui.setTheme(settings.data().theme == ThemeId::External ? ThemeId::Classic : settings.data().theme);
  ui.begin();
  restoreBacklight();
#if defined(VQEAF_ICON_SELFTEST)
  // Diagnostics variant: independent 192-case CRC + direct TFT SPI timing.
  // Always boot the normal OS afterwards; never persist test state.
  VqeafIconSelfTest::run(tft);
#endif

  drawSplash();
  splashAt = millis();
  lastActivityAt = millis();

  bool sdOk = storage.begin();
  if (sdOk) {
    storage.ensureSystemLayout();
    Serial.println("SD layout: /System/{Cache,Themes,Apps,Downloads,Logs,Temp} + /Media + /Documents");
  }
  // Field diagnostics at 115200 baud: do not report a runtime as healthy
  // when the card mounts read-only, the inbox is missing, or the clock is unset.
  if (sdOk) {
    const char *requiredDirs[] = {StoragePaths::THEMES,StoragePaths::APPS_INBOX,
       StoragePaths::APPS_INSTALLED,StoragePaths::CACHE_WEB};
    for (const char *path : requiredDirs) {
      File probe=storage.fs().open(path,FILE_READ);
      bool ready=probe && probe.isDirectory();
      if (probe)probe.close();
      Serial.printf("[VQEAF][CORE][SD] path=%s status=%s\n",path,ready?"OK":"MISSING_OR_READONLY");
    }
  }else Serial.println("[VQEAF][CORE][SD] NOT_MOUNTED: browser downloads, apps and themes unavailable");
  browserCoreOk = systemService.safeMode() ? false : browserService.begin(&storage);
  Serial.printf("[VQEAF][CORE][BROWSER] state=%s safe_mode=%d wifi=%d clock=%s\n",
      browserCoreOk?"READY":"UNAVAILABLE",systemService.safeMode(),WiFi.status(),
      TrustedTls::timeValidAt(time(nullptr))?"VALID":"NTP_PENDING");
  if (settings.data().theme == ThemeId::External) {
    ThemeColors themePalette;
    LauncherStyle launcherSkin;
    String loadedName, loadError;
    if (themeFiles.load(storage, settings.selectedThemePath(), themePalette, loadedName, loadError, &launcherSkin)) {
      ui.setExternalTheme(themePalette, &launcherSkin);
      ui.clear();
      Serial.printf("Custom theme restored: %s\n", loadedName.c_str());
    } else {
      // Missing card/corrupt theme: safe visual fallback; keep the saved SD
      // path in Preferences so reinstalling the card permits recovery next boot.
      ui.setTheme(ThemeId::Classic);
      ui.clear();
      notifications.push("Theme unavailable", loadError + " - using VQEAF Night");
      Serial.printf("Custom theme unavailable: %s\n", loadError.c_str());
    }
  }
  if (sdOk) {
    const int availableThemes=themeFiles.scan(storage);
    Serial.printf("[VQEAF][CORE][THEMES] validated=%d (malformed skipped)\n",availableThemes);
  }
  Serial.printf("SD: %s\n", sdOk ? "mounted" : "not mounted");
  Serial.printf("[S3DIAG][BOOT] sd=%s errors=%u\n", sdOk ? "mounted" : "offline", storage.ioErrors());
  Serial.println("[S3DIAG] Type diag help at 115200; only test idle media");
  notifications.push("Storage", sdOk ? "microSD mounted" : "microSD not mounted");
  appInstaller.begin(storage);
  Serial.printf("[VQEAF][CORE][QEAPP] verified_installed=%d (signature required)\n",appInstaller.count());
  appData.begin(storage, appInstaller);
  const auto recovery=appInstaller.recoveryStats();
  if(recovery.restored||recovery.blocked||recovery.discardedStages){
    notifications.push("App recovery",String("Restored ")+int(recovery.restored)+
       ", pending "+int(recovery.blocked));
    Serial.printf("[QEAPP][RECOVERY] restored=%u finalized=%u blocked=%u stale=%u\n",
       recovery.restored,recovery.finalized,recovery.blocked,recovery.discardedStages);
  }
  shellService.begin(storage, systemService, &wifiConnection);
  notifications.push("Shell", "System shell ready");

  bool audioOk = false;
  if (!systemService.safeMode()) {
    audioOk = music.begin(&storage.fs());
    music.setVolume(settings.data().volume);
  }
  Serial.printf("Audio: %s (safe=%d BCLK=%d WS=%d DOUT=%d)\n", audioOk ? "ready" : "not started",
                systemService.safeMode(), Board::AUDIO_BCLK, Board::AUDIO_WS, Board::AUDIO_DOUT);
  notifications.push("Audio", systemService.safeMode() ? "Disabled in Safe Mode" : (audioOk ? "I2S output ready" : "I2S output unavailable"));
  notifications.push("Browser", systemService.safeMode() ? "Disabled in Safe Mode" : (browserCoreOk ? "Qeafbrowser core ready" : "Browser memory unavailable"));
  if (systemService.safeMode()) notifications.push("Safe Mode", "Minimal services are active");

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false); // OS NVS controls credentials; failed joins never enter SDK flash.
  // Scanning is asynchronous; every saved AP is considered by RSSI rather
  // than automatically joining lastSSID before the scan. Safe Mode is silent.
  wifiConnection.configureAuto(!systemService.safeMode() && settings.data().wifiAuto);
  if (systemService.safeMode()) notifications.push("Safe Mode", "WiFi auto-connect disabled");
}

#if defined(VQEAF_PERF_DIAG)
static UiPerfCounter uiLoopPerf;
struct UiLoopTimingProbe {
  const uint32_t start;
  UiLoopTimingProbe() : start(micros()) {}
  ~UiLoopTimingProbe() { uiLoopPerf.record(UiPerfCounter::elapsed(micros(),start)); }
};
static void reportUiPerformanceIfDue() {
  static uint32_t lastReportMs=0;
  const uint32_t now=millis();
  if ((uint32_t)(now-lastReportMs)<5000) return;
  lastReportMs=now;
  const auto perf=uiLoopPerf.take();
  Serial.printf("[VQEAF][PERF] samples=%lu avg_loop_us=%lu max_loop_us=%lu "
                "over16=%lu over33=%lu over100=%lu heap8=%lu largest8=%lu psram=%lu\n",
     (unsigned long)perf.samples, (unsigned long)perf.meanUs,
     (unsigned long)perf.maxUs,(unsigned long)perf.over16ms,
     (unsigned long)perf.over33ms,(unsigned long)perf.over100ms,
     (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT),
     (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
     (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
#endif

void loop() {
#if defined(VQEAF_PERF_DIAG)
  // RAII captures ALL return paths, including idle/splash/locked display.
  UiLoopTimingProbe probe;
  reportUiPerformanceIfDue();
#endif
  diagPoll();
  if (!systemService.safeMode()) {
    music.update();
    musicApp.tick(appCtx, screen == ScreenId::Music);
  }
  // Card presence probes are read-only and rate-limited. Do not end/remount
  // SD_MMC while a playing or paused WAV owns an open File handle.
  const StorageService::CardEvent cardEvent = storage.tick(!music.playing());
  if (cardEvent == StorageService::CardEvent::Removed) {
    Serial.printf("[S3DIAG][SD] event=REMOVED errors=%u uptime_ms=%lu\n", storage.ioErrors(), (unsigned long)millis());
    notifications.push("microSD removed", "Storage offline; reinsert to retry");
    // Return from file-dependent views to avoid showing stale directory data.
    if (screen == ScreenId::Launcher) launcher.draw(appCtx);
    if (screen == ScreenId::Explorer) explorer.draw(appCtx);
    if (screen == ScreenId::Files || screen == ScreenId::Gallery ||
        screen == ScreenId::Themes || screen == ScreenId::TextViewer ||
        screen == ScreenId::AppInstaller || screen == ScreenId::Snake) enterScreen(ScreenId::Idle, false);
  } else if (cardEvent == StorageService::CardEvent::Mounted) {
    Serial.printf("[S3DIAG][SD] event=MOUNTED errors=%u uptime_ms=%lu\n", storage.ioErrors(), (unsigned long)millis());
    notifications.push("microSD mounted", "Filesystem ready again");
    appInstaller.refresh();
    const auto recovered=appInstaller.recoveryStats();
    if(recovered.restored||recovered.blocked){
      notifications.push("App recovery",String("Restored ")+int(recovered.restored)+
         ", blocked "+int(recovered.blocked));
    }
    if (screen == ScreenId::Launcher) launcher.draw(appCtx);
    if (screen == ScreenId::Explorer) explorer.draw(appCtx);
  }
  stopwatchApp.tick(appCtx, screen == ScreenId::Stopwatch);
  if (screen == ScreenId::Snake) pixelSnakeApp.tick(appCtx);
  galleryApp.tick(appCtx, screen == ScreenId::Gallery);
  wifiApp.tick(appCtx, screen == ScreenId::WiFi);
  systemService.update(notifications);
  updateWirelessNotifications();
  updateIdleWiFiStatus();
  // Compact WiFi bars on a foreground app (including Launcher): cached
  // chrome refresh touches only the status strip, never the entire screen.
  if (screen != ScreenId::Idle && screen != ScreenId::Lock &&
      screen != ScreenId::Splash && !keyboard.active() &&
      (uint32_t)(millis() - lastAppWifiBadgeRefresh) >= 5000UL) {
    if (screen == ScreenId::Explorer) explorer.refreshStatus(appCtx);
    else if (screen == ScreenId::Launcher) ui.refreshWifiBadge(wifiConnected(), settings.data().hour12);
    else ui.refreshWifiBadge(wifiConnected(), settings.data().hour12);
    lastAppWifiBadgeRefresh = millis();
  }

  if (!ntpConfigured && wifiConnected()) {
    // Vietnam/ICT default for this handheld. Change this offset for other regions.
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    ntpConfigured = true;
  }
  // configTime() is asynchronous. Only report success after the clock really
  // has a valid wall time; never display device uptime as HH:MM.
  if (ntpConfigured && !timeSyncedNotified &&
      (uint32_t)(millis() - lastTimeCheck) >= 5000UL) {
    lastTimeCheck = millis();
    struct tm synced;
    if (getLocalTime(&synced, 2)) {
      timeSyncedNotified = true;
      notifications.push("Clock synchronized", "Network time is available");
      if (screen == ScreenId::Idle) ui.idleClock(settings.data().hour12);
      else if (screen == ScreenId::Explorer) explorer.refreshStatus(appCtx);
      else if (screen == ScreenId::Launcher) ui.refreshWifiBadge(wifiConnected(), settings.data().hour12);
      else if (screen != ScreenId::Splash && screen != ScreenId::Lock && !keyboard.active())
        ui.refreshWifiBadge(wifiConnected(), settings.data().hour12);
    }
  }

  if (screen == ScreenId::Splash) {
    if (millis() - splashAt > 1100) {
      if (systemService.recoverySuggested()) enterScreen(ScreenId::Recovery, false);
      else enterScreen(ScreenId::Idle, false);
    }
    delay(5);
    return;
  }

  updateAutoLock();

  // Keep clock/standby alive without a full transition.
  if ((screen == ScreenId::Idle || screen == ScreenId::Clock || screen == ScreenId::Lock) &&
      millis() - lastClockRefresh >= 10000) {
    if (screen == ScreenId::Idle) ui.idleClock(settings.data().hour12);
    else if (screen == ScreenId::Clock) drawClock(false);
    else ui.lockClock(settings.data().hour12, lockDimmed);
    lastClockRefresh = millis();
  }

  if (screen == ScreenId::Lock && !lockDimmed && millis() - lockAt >= 10000UL) {
    lockDimmed = true;
    setBacklight(8);
    drawLock();
  }

  input.setTextInputActive(keyboard.active());
  KeyEvent e = input.poll();
  if (e.key == Key::None) { delay(4); return; }
  lastActivityAt = millis();

  // Locked state consumes all keypad events before any global shortcut.
  if (screen == ScreenId::Lock) {
    if (lockDimmed) {
      lockDimmed = false;
      restoreBacklight();
      lockAt = millis();
      drawLock();
    }
    if (e.pressed && !e.longPress && (e.key == Key::Start || e.key == Key::Select)) {
      notifications.push("Keypad unlocked", "Device returned to standby");
      enterScreen(ScreenId::Idle, false);
    }
    return;
  }

  // MENU/SELECT are release-vs-hold gestures (see InputManager). Do not
  // intercept a held START while an app/theme is opening: its earlier short
  // click has ALREADY selected that package/theme, so a second transition to
  // Music would interrupt installation or make Theme Manager appear blank.
  const auto shortcut = GlobalShortcutPolicy::resolve(e, keyboard.active());
  if (shortcut == GlobalShortcutPolicy::Action::ToggleT9) {
    Serial.printf("[key] mode: %s\n",input.t9Mode()?"T9":"GAME");
    notifications.push("Input mode",input.t9Mode()?"T9 multi-tap":"Game navigation");
    if (keyboard.active()) keyboard.draw(ui,wifiConnected(),false,storage.mounted(),settings.data().hour12);
    return;
  }
  if (shortcut == GlobalShortcutPolicy::Action::TaskSwitcher) {
    Serial.println("[VQEAF][KEY] MENU hold => Tasks");
    enterScreen(ScreenId::TaskSwitcher);
    return;
  }
  if (e.longPress) return; // an unassigned held key must not re-dispatch

  if (!keyboard.active() && e.pressed && !e.longPress && e.key == Key::Menu) {
    if (screen != ScreenId::Launcher) enterScreen(ScreenId::Launcher);
    else enterScreen(ScreenId::Idle, false);
    return;
  }

  // B is the physical right-softkey/back button; SELECT is a second OK key.
  if (!keyboard.active() && screen != ScreenId::Launcher && screen != ScreenId::Explorer && e.pressed && !e.longPress && e.key == Key::B) e.key = Key::A;
  if (!keyboard.active() && e.pressed && !e.longPress && e.key == Key::Select) e.key = Key::Start;

  if (screen == ScreenId::Idle) {
    if (!e.pressed || e.longPress) return;
    if (e.key == Key::Left) { const int old=idleShortcut; idleShortcut = (idleShortcut + 2) % 3; ui.idleShortcutDelta(old,idleShortcut); }
    else if (e.key == Key::Right) { const int old=idleShortcut; idleShortcut = (idleShortcut + 1) % 3; ui.idleShortcutDelta(old,idleShortcut); }
    else if (e.key == Key::Start) enterScreen(idleShortcutTarget());
    else if (e.key == Key::Option) enterScreen(ScreenId::QuickPanel);
    else if (e.key == Key::Up) enterScreen(ScreenId::Notifications);
    else if (e.key == Key::A) { notifications.push("Keypad locked", "Locked from standby"); enterScreen(ScreenId::Lock, false); }
    return;
  }

  ScreenId next = screen;
  switch (screen) {
    case ScreenId::Launcher:
      next = launcher.handle(appCtx, e); break;
    case ScreenId::Explorer:
      next = explorer.handle(appCtx, e); break;
    case ScreenId::WiFi:
      next = wifiApp.handle(appCtx,e); break;
    case ScreenId::BLE:
      next = bleApp.handle(appCtx,e); break;
    case ScreenId::Music:
      next = musicApp.handle(appCtx,e); break;
    case ScreenId::Calculator:
      next = calculatorApp.handle(appCtx,e); break;
    case ScreenId::Stopwatch:
      next = stopwatchApp.handle(appCtx,e); break;
    case ScreenId::Files:
      next = filesApp.handle(appCtx,e); break;
    case ScreenId::Collection:
      next = collectionApp.handle(appCtx,e); break;
    case ScreenId::Gallery:
      next = galleryApp.handle(appCtx,e); break;
    case ScreenId::TextViewer:
      next = textViewerApp.handle(appCtx,e); break;
    case ScreenId::Browser:
      next = browserApp.handle(appCtx,e); break;
    case ScreenId::Snake:
      next = pixelSnakeApp.handle(appCtx,e); break;
    case ScreenId::Shell:
      next = shellApp.handle(appCtx,e); break;
    case ScreenId::Settings:
      next = settingsApp.handle(appCtx,e); break;
    case ScreenId::Themes:
      next = themesApp.handle(appCtx,e); break;
    case ScreenId::Applications:
      next = applicationsApp.handle(appCtx, e); break;
    case ScreenId::AppInstaller:
      next = appInstallerApp.handle(appCtx, e); break;
    case ScreenId::QuickPanel:
      next = quickPanelApp.handle(appCtx,e); break;
    case ScreenId::TaskSwitcher:
      next = taskSwitcherApp.handle(appCtx,e); break;
    case ScreenId::Notifications:
      next = notificationApp.handle(appCtx,e); break;
    case ScreenId::Notes:
      next = notesApp.handle(appCtx,e); break;
    case ScreenId::Recovery:
      next = recoveryApp.handle(appCtx,e); break;
    case ScreenId::Clock:
    case ScreenId::SystemInfo:
    case ScreenId::About:
      if (e.pressed && !e.longPress && e.key == Key::A) next = detailReturn;
      break;
    default:
      break;
  }
  if (next != screen) {
    bool resumeTarget = false;
    if (screen == ScreenId::TaskSwitcher) resumeTarget = taskSwitcherApp.takeResumeRequest();
    if (screen == ScreenId::Themes && next == ScreenId::Files) resumeTarget = true;
    if (screen == ScreenId::AppInstaller && (next == ScreenId::Browser || next == ScreenId::Files)) resumeTarget = true;
    if (next == ScreenId::Clock || next == ScreenId::SystemInfo || next == ScreenId::About) detailReturn = screen;
    enterScreen(next, true, resumeTarget);
  }
}
