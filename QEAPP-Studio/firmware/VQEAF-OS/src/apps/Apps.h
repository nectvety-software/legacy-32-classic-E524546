#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include "../core/SymbianUI.h"
#include "../core/TextKeyboard.h"
#include "../services/StorageService.h"
#include "../services/SettingsStore.h"
#include "../services/MusicService.h"
#include "../services/CalculatorEngine.h"
#include "../services/StopwatchEngine.h"
#include "../services/PlaylistNavigator.h"
#include "../services/NotificationService.h"
#include "../services/SystemService.h"
#include "../services/WiFiProfileStore.h"
#include "../services/WiFiConnectionService.h"
#include "../services/BrowserService.h"
#include "../services/ImageViewerService.h"
#include "../services/ShellService.h"
#include "../services/ThemeFileService.h"
#include "../services/AppInstallerService.h"
#include "../services/QeappDataService.h"
#include "../launcher/LauncherView.h"

struct AppContext {
  SymbianUI &ui;
  StorageService &storage;
  SettingsStore &settings;
  MusicService &music;
  TextKeyboard &keyboard;
  NotificationService &notifications;
  SystemService &system;
  WiFiProfileStore &wifiProfiles;
  WiFiConnectionService &wifiConnection;
  BrowserService &browser;
  ImageViewerService &imageViewer;
  ShellService &shell;
  ThemeFileService &themes;
  AppInstallerService &installer;
  QeappDataService &appData;
  String pendingPackagePath;
  String pendingPackageId;
  String pendingBrowserUrl;
  bool pendingPackageLaunch;
  String pendingOpenPath;
  String pendingFolderPath;
  String pendingThemePath;

  AppContext(SymbianUI &uiRef, StorageService &storageRef, SettingsStore &settingsRef,
             MusicService &musicRef, TextKeyboard &keyboardRef,
             NotificationService &notificationsRef, SystemService &systemRef,
             WiFiProfileStore &wifiProfilesRef, WiFiConnectionService &wifiConnectionRef, BrowserService &browserRef,
             ImageViewerService &imageViewerRef, ShellService &shellRef,
             ThemeFileService &themesRef, AppInstallerService &installerRef, QeappDataService &appDataRef)
      : ui(uiRef), storage(storageRef), settings(settingsRef), music(musicRef),
        keyboard(keyboardRef), notifications(notificationsRef), system(systemRef),
        wifiProfiles(wifiProfilesRef), wifiConnection(wifiConnectionRef), browser(browserRef), imageViewer(imageViewerRef),
        shell(shellRef), themes(themesRef), installer(installerRef), appData(appDataRef), pendingPackagePath(), pendingPackageId(),
        pendingBrowserUrl(), pendingPackageLaunch(false), pendingOpenPath(), pendingFolderPath(), pendingThemePath() {}
};

struct PopupState {
  bool open;
  int index;
  int offset;
  static constexpr int VISIBLE = 5;

  PopupState() : open(false), index(0), offset(0) {}
  void show() { open = true; index = 0; offset = 0; }
  void close() { open = false; index = 0; offset = 0; }
  void up() {
    if (index > 0) {
      --index;
      if (index < offset) --offset;
    }
  }
  void down(int count) {
    if (index < count - 1) {
      ++index;
      if (index >= offset + VISIBLE) ++offset;
    }
  }
};

class LauncherApp {
public:
  void enter() { popup.close(); }
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  int index = 0;
  PopupState popup;
  void moveGrid(Key key);
};


class ExplorerApp {
public:
  void enter() { popup.close(); detailOpen=false; }
  void draw(AppContext &ctx);
  void refreshStatus(AppContext &ctx);
  void openHome(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  static const int TAB_COUNT=6;
  static const int MAX_ROWS=18; // 2 system rows + MAX_INSTALLED signed QEAPP/2
  int tab=0;
  int cursors[TAB_COUNT]={0};
  int offsets[TAB_COUNT]={0};
  PopupState popup;
  bool detailOpen=false;
  // One signed icon only (32x32 RGB565 = 2048 bytes). Cache is never
  // refreshed on every list redraw or navigation frame.
  uint16_t cachedIcon[1024]={0};
  char cachedIconId[25]={0};
  bool cachedIconValid=false;
  const uint16_t *selectedIcon(AppContext &ctx,const LauncherRow &row);
  int fillRows(AppContext &ctx, LauncherRow out[MAX_ROWS]) const;
  ScreenId selectedTarget(AppContext &ctx) const;
};

class WiFiApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
  void tick(AppContext &ctx, bool visible); // polled by main loop; connection survives app switch
  bool connected() const { return WiFi.status() == WL_CONNECTED; }
private:
  enum class Page : uint8_t { Networks, Saved, Details, Connecting, Result };
  enum class Input : uint8_t { None, Password, HiddenName };
  Page page = Page::Networks;
  Input input = Input::None;
  int index = 0, offset = 0;
  int savedIndex = 0, savedOffset = 0;
  bool screenPainted = false;
  uint32_t lastPaintSecond = 0;
  String selectedSSID;
  String pendingPassword;
  bool selectedOpen = false;
  String lastMessage;
  PopupState popup;
  void scan(AppContext &ctx);
  void connectSelected(AppContext &ctx, bool savedList = false);
  void showResult(AppContext &ctx, const String &message);
  void paintProgress(AppContext &ctx, bool full);
};

class TaskSwitcherApp {
public:
  void enter(AppContext &ctx, ScreenId from);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
  bool takeResumeRequest() { bool v = resumeRequested; resumeRequested = false; return v; }
private:
  int index = 0;
  int offset = 0;
  ScreenId returnTo = ScreenId::Idle;
  PopupState popup;
  bool resumeRequested = false;
};

class BleApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
  bool ready() const { return initialized; }
private:
  static constexpr int MAX_DEVS = 16;
  struct Dev {
    char name[32];
    char addr[18];
    int16_t rssi;
    Dev() : name{0}, addr{0}, rssi(0) {}
  } devs[MAX_DEVS];
  int count = 0, index = 0, offset = 0;
  bool initialized = false;
  bool details = false;
  PopupState popup;
  void scan(AppContext &ctx);
};

class FilesApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  String path = "/";
  static constexpr int MAX_ENTRIES = 40;
  FsEntry entries[MAX_ENTRIES];
  int count = 0, index = 0, offset = 0;
  bool confirmDelete = false;
  int confirmChoice = 1;
  PopupState popup;
  void reload(AppContext &ctx);
  void parent(AppContext &ctx);
  void showProperties(AppContext &ctx);
};

class CollectionApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  int index = 0;
  int images = 0, music = 0, docs = 0, total = 0;
  PopupState popup;
};

class MusicApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  static constexpr int MAX_TRACKS = 32;
  FsEntry tracks[MAX_TRACKS];
  int count = 0, index = 0, offset = 0;
  PopupState popup;
  PlaylistNavigator playlist;
  int playingIndex = -1;
  bool nowPlaying = false;
  uint32_t lastPaintSecond = 0xFFFFFFFFUL;
  int nextIndex(bool forward);
  void playIndex(AppContext &ctx, int item);
  void playerFooter(AppContext &ctx);
  void paintVolume(AppContext &ctx);
public:
  void tick(AppContext &ctx, bool visible);
};

class GalleryApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  static constexpr int MAX_IMAGES = 24;
  FsEntry images[MAX_IMAGES];
  int count = 0, index = 0, offset = 0;
  bool preview = false;
  bool slideshow = false;
  uint32_t lastSlideAt = 0;
public:
  void tick(AppContext &ctx,bool visible);
private:
  ScreenId returnTo = ScreenId::Launcher;
  PopupState popup;
  void drawPreview(AppContext &ctx);
};

class TextViewerApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  static constexpr int MAX_DOCS = 24;
  static constexpr int PAGE_LINES = 15;
  static constexpr int PAGE_STACK = 16;
  FsEntry docs[MAX_DOCS];
  char pageLines[PAGE_LINES][40] = {{0}};
  uint32_t offsets[PAGE_STACK] = {0};
  uint32_t nextOffset = 0;
  int count = 0, index = 0, offset = 0, lineCount = 0, page = 0;
  bool viewing = false;
  ScreenId returnTo = ScreenId::Applications;
  PopupState popup;
  bool loadPage(AppContext &ctx, uint32_t fileOffset);
  void drawViewer(AppContext &ctx);
};

class BrowserApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  int offset = 0;
  int selectedLink = -1;
  bool urlEntry = false;
  bool launchedFromPackage = false;
  PopupState popup;
  void loadHome(AppContext &ctx);
  void redrawBody(AppContext &ctx);
  void moveLink(AppContext &ctx, int direction);
};

class ShellApp {
public:
  void enter(AppContext &ctx);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  PopupState popup;
  int historyIndex = -1;
  void openCommand(AppContext &ctx, const String &initial = "");
};

class RecoveryApp {
public:
  void enter() { index = 0; popup.close(); }
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  int index = 0;
  PopupState popup;
};

class SettingsApp {
public:
  void enter() { index = 0; offset = 0; popup.close(); }
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  int index = 0;
  int offset = 0;
  PopupState popup;
};

class ThemesApp {
public:
  void enter(AppContext &ctx, ScreenId from);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  static constexpr int BUILTIN_COUNT = 4;
  int index = 0;
  int offset = 0;
  ScreenId returnTo = ScreenId::Launcher;
  PopupState popup;
  bool showDetails = false;
  String feedback;
  int total(const AppContext &ctx) const { return BUILTIN_COUNT + ctx.themes.count(); }
  bool apply(AppContext &ctx);
};

class AppInstallerApp {
public:
  void enter(AppContext &ctx, ScreenId from);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  static constexpr int MAX_PACKAGES = 12;
  FsEntry inbox[MAX_PACKAGES];
  // Compact verified-manifest cache for the 12-item Inbox. This stores only
  // name/version/type, not full icon/payload data or a package framebuffer.
  struct InboxLabel { char name[41], version[20], type[8]; bool manifestOk; } inboxLabel[MAX_PACKAGES];
  int count = 0, index = 0, offset = 0;
  bool installedTab = false, details = false, confirm = false, verified = false;
  bool willUpdate = false, installAllowed = false, confirmData = false;
  String previousVersion;
  bool operationResultOpen = false;
  String operationResultTitle;
  int confirmChoice = 1;
  PopupState popup;
  ScreenId returnTo = ScreenId::Applications;
  String selectedPath;
  Qeapp::Meta selectedMeta;
  String feedback;
  // A single icon preview; cache between partial UI redraws rather than
  // repeatedly hashing the package whenever a softkey is pressed.
  bool previewIconReady = false;
  uint16_t previewPixels[1024];
  void reload(AppContext &ctx);
  void openDetails(AppContext &ctx);
  void paintRow(AppContext &ctx, int item, bool selected);
};

class ApplicationsApp {
public:
  void enter(AppContext &ctx) { ctx.installer.refresh(); index = 0; offset = 0; popup.close(); }
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  int index = 0;
  int offset = 0;
  PopupState popup;
  ScreenId openSelected(AppContext &ctx);
};

class QuickPanelApp {
public:
  void enter() { index = 0; }
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  int index = 0;
  void adjust(AppContext &ctx, int direction);
};

class NotificationCenterApp {
public:
  void enter(AppContext &ctx, ScreenId returnScreen = ScreenId::Applications);
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  int index = 0;
  int offset = 0;
  PopupState popup;
  ScreenId returnTo = ScreenId::Applications;
};

class NotesApp {
public:
  void enter() { popup.close(); }
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx, const KeyEvent &e);
private:
  PopupState popup;
  bool confirmClear = false;
  int confirmChoice = 1;
};

// Utilities: entirely local and Safe-Mode compatible. Both preserve state
// across the task switcher and avoid per-frame full-screen redraws.
class CalculatorApp {
public:
  void enter() { col=0; row=0; popup.close(); }
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx,const KeyEvent &e);
private:
  CalculatorEngine engine;
  uint8_t col=0,row=0;
  PopupState popup;
  static char button(uint8_t row,uint8_t col);
  void paintButton(AppContext &ctx,uint8_t r,uint8_t c,bool selected);
  void paintDisplay(AppContext &ctx);
};

class StopwatchApp {
public:
  void enter() { selected=0; }
  void draw(AppContext &ctx);
  ScreenId handle(AppContext &ctx,const KeyEvent &e);
  void tick(AppContext &ctx,bool visible);
private:
  StopwatchEngine engine;
  uint32_t lastPaint=0;
  uint8_t selected=0;
  void paintTime(AppContext &ctx);
  void paintAction(AppContext &ctx,uint8_t idx,bool focused);
};
