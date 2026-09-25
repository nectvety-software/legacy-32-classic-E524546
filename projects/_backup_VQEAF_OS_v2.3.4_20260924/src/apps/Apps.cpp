#include "Apps.h"
#include "BoardConfig.h"
#include <esp_heap_caps.h>

// GCC 8 / GNU++11 needs definitions when class constexpr integers are odr-used
// through helpers such as min()/array-bound arithmetic.
constexpr int PopupState::VISIBLE;
constexpr int BleApp::MAX_DEVS;
constexpr int FilesApp::MAX_ENTRIES;
constexpr int MusicApp::MAX_TRACKS;
constexpr int GalleryApp::MAX_IMAGES;
constexpr int TextViewerApp::MAX_DOCS;
constexpr int TextViewerApp::PAGE_LINES;
constexpr int TextViewerApp::PAGE_STACK;

static bool gBleReady = false;
static bool statusWifi() { return WiFi.status() == WL_CONNECTED; }

static void drawPopup(AppContext &ctx, PopupState &popup, const char *const items[], int count) {
  if (!popup.open) return;
  ctx.ui.popupMenu(items, count, popup.index, popup.offset, PopupState::VISIBLE);
  ctx.ui.softkeys("Select", "", "Cancel");
}

static bool popupNav(PopupState &popup, const KeyEvent &e, int count) {
  if (!popup.open || !e.pressed || e.longPress) return false;
  if (e.key == Key::Up) popup.up();
  else if (e.key == Key::Down) popup.down(count);
  else if (e.key == Key::A || e.key == Key::Option) popup.close();
  return true;
}

// ---------------- VQEAF portrait launcher ----------------
// This is an original Retro-Go-inspired presentation; it does not include
// Retro-Go's GPL source code, visual assets or theme.json parser.
struct LaunchDestination {
  const char *glyph,*title,*description;
  ScreenId screen;
  const char *folder;
};
static const LaunchDestination HOME[]={
  {"WB","Qeafbrowser","Lightweight web browser",ScreenId::Browser,nullptr},
  {"AP","Installed apps","Signed QEAPP/2 applications",ScreenId::Applications,nullptr},
  {"FI","File manager","Browse files on microSD",ScreenId::Files,nullptr},
  {"MD","Media library","Music, gallery and documents",ScreenId::Collection,nullptr},
  {"TH","Themes","Select a .vqeaf skin",ScreenId::Themes,nullptr},
  {"RC","Recent tasks","Switch to an open task",ScreenId::TaskSwitcher,nullptr},
};
static const LaunchDestination WEB[]={
  {"WB","Open browser","Qeafbrowser Internet access",ScreenId::Browser,nullptr},
  {"WI","WiFi manager","Scan or join a network",ScreenId::WiFi,nullptr},
  {"DL","Downloads","Downloaded media and files",ScreenId::Files,StoragePaths::DOWNLOADS},
  {"BK","Bookmarks","Use Options in browser",ScreenId::Browser,nullptr},
};
static const LaunchDestination APPS[]={
  {"AP","Applications","System utilities + QEAPP/2",ScreenId::Applications,nullptr},
  {"IN","App installer","Signed package inbox",ScreenId::AppInstaller,nullptr},
};
static const LaunchDestination MEDIA[]={
  {"GA","Gallery","Browse pictures",ScreenId::Gallery,nullptr},
  {"MU","Music","Browse and play audio",ScreenId::Music,nullptr},
  {"LI","Library","Explore media collections",ScreenId::Collection,nullptr},
  {"FI","File manager","SD card folders",ScreenId::Files,nullptr},
  {"TX","Text reader","Read TXT and Markdown",ScreenId::TextViewer,nullptr},
};
static const LaunchDestination SYSTEM[]={
  {"WI","WiFi","Wireless connectivity",ScreenId::WiFi,nullptr},
  {"BT","Bluetooth","BLE device scanner",ScreenId::BLE,nullptr},
  {"SH","Shell","System command interface",ScreenId::Shell,nullptr},
  {"RC","Recovery","Boot & Safe Mode controls",ScreenId::Recovery,nullptr},
  {"NT","Notifications","Recent system messages",ScreenId::Notifications,nullptr},
};
static const LaunchDestination CONFIG[]={
  {"TH","Theme manager","Import and apply .vqeaf",ScreenId::Themes,nullptr},
  {"ST","Settings","Screen, sound and network",ScreenId::Settings,nullptr},
  {"CA","Calculator","Handheld calculator",ScreenId::Calculator,nullptr},
  {"SW","Stopwatch","Track elapsed time",ScreenId::Stopwatch,nullptr},
  {"AB","About VQEAF","Firmware and components",ScreenId::About,nullptr},
};
static const char *const TAB_LABELS[]={"Home","Internet","Applications","Media","System","Settings"};
static const char *const LAUNCHER_ACTIONS[]={"Open selected","Next tab","Themes",
                                          "App installer","System info","Standby"};
static const int LAUNCHER_ACTION_COUNT=6;

template<size_t N> static size_t arrayLength(const LaunchDestination (&)[N]) {return N;}
static const LaunchDestination *tabItems(int tab, int &count) {
  switch(tab) {
    case 0:count=arrayLength(HOME);return HOME;
    case 1:count=arrayLength(WEB);return WEB;
    case 2:count=arrayLength(APPS);return APPS;
    case 3:count=arrayLength(MEDIA);return MEDIA;
    case 4:count=arrayLength(SYSTEM);return SYSTEM;
    default:count=arrayLength(CONFIG);return CONFIG;
  }
}

int ExplorerApp::fillRows(AppContext &ctx,LauncherRow out[MAX_ROWS]) const {
  int builtins=0;const LaunchDestination *entries=tabItems(tab,builtins);
  int total=0;
  for(int i=0;i<builtins && total<MAX_ROWS;++i) {
    out[total++]={entries[i].glyph,entries[i].title,entries[i].description,nullptr};
  }
  if(tab==2 && ctx.storage.mounted()) {
    const int installed=ctx.installer.count();
    for(int i=0;i<installed && total<MAX_ROWS;++i) {
      const auto &e=ctx.installer.at(i).info;
      out[total++]={"QA",e.name,e.type[0]=='w'?"Signed Web app":"Signed Text app",e.id};
    }
  }
  return total;
}

const uint16_t *ExplorerApp::selectedIcon(AppContext &ctx,const LauncherRow &row) {
  if(!row.packageId || !ctx.storage.mounted()) {
    cachedIconId[0]=0; cachedIconValid=false; return nullptr;
  }
  if(strcmp(cachedIconId,row.packageId)!=0) {
    snprintf(cachedIconId,sizeof cachedIconId,"%s",row.packageId);
    cachedIconValid=ctx.installer.loadIcon(row.packageId,cachedIcon);
  }
  return cachedIconValid?cachedIcon:nullptr;
}

void ExplorerApp::refreshStatus(AppContext &ctx) {
  LauncherView view(ctx.ui.display());
  view.status(ctx.ui.getLauncherStyle(),ctx.ui.timeText(ctx.settings.data().hour12),
              statusWifi(),ctx.storage.mounted());
}
void ExplorerApp::openHome(AppContext &ctx) {
  tab=0;popup.close();detailOpen=false;draw(ctx);
}
void ExplorerApp::draw(AppContext &ctx) {
  LauncherRow rows[MAX_ROWS]; const int total=fillRows(ctx,rows);
  int &sel=cursors[tab], &offset=offsets[tab];
  if(sel>=total)sel=total?total-1:0;
  if(offset>sel)offset=sel;
  if(sel>=offset+LauncherView::VISIBLE)offset=sel-LauncherView::VISIBLE+1;
  if(total<=LauncherView::VISIBLE)offset=0;
  LauncherView view(ctx.ui.display());
  view.full(ctx.ui.getLauncherStyle(),TAB_LABELS[tab],tab,TAB_COUNT,
            rows,total,offset,sel,ctx.ui.timeText(ctx.settings.data().hour12),
            statusWifi(),ctx.storage.mounted(),
            total?selectedIcon(ctx,rows[sel]):nullptr);
  if(detailOpen && total) {
    const LauncherRow &item=rows[sel];
    const char *values[]={item.title,item.summary,item.packageId?"Signed QEAPP/2":"Built-in utility"};
    view.dialog(ctx.ui.getLauncherStyle(),"Application",values,3,0,0);
  } else if(popup.open) {
    view.dialog(ctx.ui.getLauncherStyle(),"Launcher options",LAUNCHER_ACTIONS,
                LAUNCHER_ACTION_COUNT,popup.offset,popup.index);
  }
}

ScreenId ExplorerApp::selectedTarget(AppContext &ctx) const {
  int builtins=0;const LaunchDestination *items=tabItems(tab,builtins);
  const int sel=cursors[tab];
  if(sel<builtins) {
    if(items[sel].folder)ctx.pendingFolderPath=items[sel].folder;
    return items[sel].screen;
  }
  if(tab==2 && ctx.storage.mounted()) {
    const int i=sel-builtins;
    if(i>=0 && i<ctx.installer.count()) {
      ctx.pendingPackageId=ctx.installer.at(i).info.id;
      return ScreenId::PackageApp; // existing signature-checked launch gate
    }
  }
  return ScreenId::Launcher;
}

ScreenId ExplorerApp::handle(AppContext &ctx,const KeyEvent &e) {
  if(!e.pressed || e.longPress)return ScreenId::Launcher;
  if(detailOpen) {detailOpen=false;draw(ctx);return ScreenId::Launcher;}
  if(popup.open) {
    if(e.key==Key::Start || e.key==Key::Select) {
      const int choice=popup.index;popup.close();
      switch(choice) {
        case 0:return selectedTarget(ctx);
        case 1:tab=(tab+1)%TAB_COUNT;break;
        case 2:return ScreenId::Themes;
        case 3:return ScreenId::AppInstaller;
        case 4:return ScreenId::SystemInfo;
        case 5:return ScreenId::Idle;
      }
      draw(ctx);return ScreenId::Launcher;
    }
    if(e.key==Key::Up)popup.up();
    else if(e.key==Key::Down)popup.down(LAUNCHER_ACTION_COUNT);
    else if(e.key==Key::A || e.key==Key::B || e.key==Key::Option)popup.close();
    // Popup requires redrawing the underlying frame after close.
    if(!popup.open)draw(ctx);
    else {
      LauncherView view(ctx.ui.display());
      view.dialog(ctx.ui.getLauncherStyle(),"Launcher options",LAUNCHER_ACTIONS,
                  LAUNCHER_ACTION_COUNT,popup.offset,popup.index);
    }
    return ScreenId::Launcher;
  }
  if(e.key==Key::Left || e.key==Key::Right) {
    if(e.repeat)return ScreenId::Launcher; // 1 tab per physical press
    tab=(tab+(e.key==Key::Left?TAB_COUNT-1:1))%TAB_COUNT;
    draw(ctx);return ScreenId::Launcher;
  }
  if(e.key==Key::A){if(tab!=0){openHome(ctx);}return ScreenId::Launcher;}
  if(e.key==Key::B){detailOpen=true;draw(ctx);return ScreenId::Launcher;}
  if(e.key==Key::Option){popup.show();draw(ctx);return ScreenId::Launcher;}
  if(e.key==Key::Start || e.key==Key::Select)return selectedTarget(ctx);
  if(e.key==Key::Up || e.key==Key::Down) {
    LauncherRow rows[MAX_ROWS];int n=fillRows(ctx,rows);
    int &sel=cursors[tab], &offset=offsets[tab];
    const int old=sel, oldOffset=offset;
    if(e.key==Key::Up && sel>0)--sel;
    else if(e.key==Key::Down && sel<n-1)++sel;
    if(sel==old)return ScreenId::Launcher;
    if(sel<offset)offset=sel;
    if(sel>=offset+LauncherView::VISIBLE)offset=sel-LauncherView::VISIBLE+1;
    LauncherView view(ctx.ui.display());
    if(oldOffset!=offset) {
      view.content(ctx.ui.getLauncherStyle(),rows,n,offset,sel,
                   n?selectedIcon(ctx,rows[sel]):nullptr);
    } else {
      view.row(ctx.ui.getLauncherStyle(),&rows[old],old-offset,false);
      view.row(ctx.ui.getLauncherStyle(),&rows[sel],sel-offset,true);
      view.scrollbar(ctx.ui.getLauncherStyle(),n,offset);
      view.preview(ctx.ui.getLauncherStyle(),&rows[sel],selectedIcon(ctx,rows[sel]));
    }
  }
  return ScreenId::Launcher;
}

// ---------------- WiFi: Qeafbrowser keypad wizard, non-blocking ----------------
static const char *const wifiOptions[] = {
  "Connect", "Rescan", "Disconnect", "Forget saved", "Network status",
  "Saved networks", "Hidden network", "Network list"
};
static constexpr int WIFI_OPTIONS = sizeof(wifiOptions) / sizeof(wifiOptions[0]);

void WiFiApp::enter(AppContext &ctx) {
  popup.close();
  if (ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Connecting) {
    page = Page::Connecting;
    screenPainted = false;
  } else {
    scan(ctx);
  }
}

void WiFiApp::scan(AppContext &ctx) {
  popup.close();
  input = Input::None;
  page = Page::Networks;
  index = offset = 0;
  screenPainted = false;
  if (!ctx.wifiConnection.scan() &&
      ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Connecting) {
    page = Page::Connecting;
  }
}

void WiFiApp::paintProgress(AppContext &ctx, bool full) {
  auto &ui = ctx.ui;
  WiFiConnectionService::Phase phase = ctx.wifiConnection.phase();
  const bool isScan = phase == WiFiConnectionService::Phase::Scanning;
  const uint32_t second = ctx.wifiConnection.elapsedMs()/1000UL;
  if (full || !screenPainted) {
    ui.clearContent();
    ui.chrome("WiFi", statusWifi(), false, false, ctx.settings.data().hour12);
    if (isScan) ui.message("Search WLAN", "Scanning for access points", "Current WiFi stays active");
    else ui.message("Connecting...", ctx.wifiConnection.requestedSSID(), "Please wait (up to 12s)");
    ui.softkeys("", "", isScan ? "Back" : "Cancel");
    screenPainted = true;
    lastPaintSecond = UINT32_MAX;
  }
  if (lastPaintSecond != second) {
    auto &lcd = ui.display(); auto colors = ui.c();
    lcd.fillRect(12, 160, 213, 40, colors.bg);
    lcd.setTextColor(colors.text, colors.bg);
    lcd.setTextSize(1); lcd.setTextFont(2);
    lcd.setCursor(12, 164);
    lcd.print(isScan ? String("Searching ") + String(second) + "s" :
                        String("Joining ") + String(second) + " / 12s");
    ui.progress(12, 189, 207, isScan ? (int)(second*7 % 100) : (int)min(99UL, second*100UL/12UL));
    lastPaintSecond = second;
  }
}

void WiFiApp::showResult(AppContext &ctx, const String &message) {
  page = Page::Result;
  lastMessage = message;
  screenPainted = false;
  draw(ctx);
}

void WiFiApp::draw(AppContext &ctx) {
  if (ctx.keyboard.active()) {
    ctx.keyboard.draw(ctx.ui, statusWifi(), false, false, ctx.settings.data().hour12);
    return;
  }
  if (ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Scanning && page == Page::Networks) {
    paintProgress(ctx, !screenPainted);
    drawPopup(ctx, popup, wifiOptions, WIFI_OPTIONS);
    return;
  }
  if (page == Page::Connecting) {
    paintProgress(ctx, !screenPainted);
    drawPopup(ctx, popup, wifiOptions, WIFI_OPTIONS);
    return;
  }
  ctx.ui.chrome("WiFi", statusWifi(), false, false, ctx.settings.data().hour12);
  if (page == Page::Details) {
    const bool on = statusWifi();
    ctx.ui.message("Network status", on ? WiFi.SSID() : String("Offline"),
                   on ? String("IP ") + WiFi.localIP().toString() : String("Choose a WLAN network"),
                   on ? String("RSSI ") + WiFi.RSSI() + " dBm" : String(ctx.wifiProfiles.count()) + " saved profile(s)");
    ctx.ui.softkeys("Options", "", "Back");
  } else if (page == Page::Result) {
    ctx.ui.message("WiFi", lastMessage,
                   ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Connected ?
                     ctx.wifiConnection.requestedSSID() : String(ctx.wifiConnection.error()),
                   (ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Connected && statusWifi()) ?
                     String("IP ") + WiFi.localIP().toString() : "Select Retry or Back");
    ctx.ui.softkeys("Options", "Retry", "Back");
  } else {
    const bool savedView = page == Page::Saved;
    const int n = savedView ? ctx.wifiProfiles.count() : ctx.wifiConnection.count();
    const int cursor = savedView ? savedIndex : index;
    const int top = savedView ? savedOffset : offset;
    if (n == 0) {
      ctx.ui.message(savedView ? "Saved networks" : "WiFi", savedView ? "No saved networks" : "No networks found",
                     "Options > Rescan");
    } else {
      for (int row = 0; row < SymbianUI::LIST_VISIBLE; ++row) {
        int i = top + row;
        if (i >= n) { ctx.ui.clearListRow(row); continue; }
        String name, sub;
        if (savedView) {
          const WiFiProfile &p = ctx.wifiProfiles.at(i);
          name = p.ssid;
          sub = p.open ? "Open  Saved" : "Secured  Saved";
        } else {
          const WiFiConnectionService::Network &net = ctx.wifiConnection.network(i);
          name = net.ssid[0] ? String(net.ssid) : String("<hidden>");
          sub = String(net.rssi) + " dBm  " + (net.auth == WIFI_AUTH_OPEN ? "Open" : "Secured");
          if (ctx.wifiProfiles.has(net.ssid)) sub += "  Saved";
        }
        if (statusWifi() && name == WiFi.SSID()) sub = "Connected  " + sub;
        ctx.ui.listItem(row, "Wi", name, sub, i == cursor);
      }
      ctx.ui.scrollbar(n, SymbianUI::LIST_VISIBLE, top);
    }
    ctx.ui.softkeys("Options", "Connect", "Back");
  }
  drawPopup(ctx, popup, wifiOptions, WIFI_OPTIONS);
  screenPainted = true;
}

void WiFiApp::connectSelected(AppContext &ctx, bool savedList) {
  bool open = false;
  if (savedList) {
    if (savedIndex < 0 || savedIndex >= ctx.wifiProfiles.count()) return;
    const WiFiProfile &profile = ctx.wifiProfiles.at(savedIndex);
    selectedSSID = profile.ssid;
    open = profile.open;
    if (ctx.wifiConnection.connect(selectedSSID, profile.password, profile.open, ctx.settings.data().wifiAuto && !ctx.system.safeMode())) {
      page = Page::Connecting; screenPainted = false;
    } else showResult(ctx, "Invalid saved credentials");
  } else {
    if (index < 0 || index >= ctx.wifiConnection.count()) return;
    const WiFiConnectionService::Network &net = ctx.wifiConnection.network(index);
    selectedSSID = net.ssid;
    selectedOpen = net.auth == WIFI_AUTH_OPEN;
    if (selectedSSID.length() == 0) { // hidden SSID must be typed manually
      input = Input::HiddenName;
      ctx.keyboard.open("Hidden network", "", false);
      return;
    }
    int saved = ctx.wifiProfiles.find(selectedSSID);
    if (saved >= 0) {
      const WiFiProfile &profile = ctx.wifiProfiles.at(saved);
      open = profile.open;
      if (ctx.wifiConnection.connect(selectedSSID, profile.password, open, ctx.settings.data().wifiAuto && !ctx.system.safeMode())) {
        page = Page::Connecting; screenPainted = false;
      } else showResult(ctx, "Invalid saved credentials");
    } else if (selectedOpen) {
      if (ctx.wifiConnection.connect(selectedSSID, "", true, ctx.settings.data().wifiAuto && !ctx.system.safeMode())) {
        page = Page::Connecting; screenPainted = false;
      } else showResult(ctx, "Cannot join network");
    } else {
      input = Input::Password;
      ctx.keyboard.open("WiFi password", "", true);
    }
  }
}

void WiFiApp::tick(AppContext &ctx, bool visible) {
  ctx.wifiConnection.poll();
  bool result = false;
  if (ctx.wifiConnection.takeConnectionResult(result)) {
    // The system-wide WiFi status watcher already announces new connections.
    // Only report failures here to avoid duplicate S60 notifications.
    if (!result) ctx.notifications.push("WiFi", ctx.wifiConnection.error());
    if (visible && (page == Page::Connecting || page == Page::Networks)) {
      selectedSSID = ctx.wifiConnection.requestedSSID();
      showResult(ctx, result ? "Connection successful" :
                  (ctx.wifiConnection.requestedSSID().length() ? "Connection failed" : "Scan failed"));
    }
  }
  // Boot auto-selection intentionally does not enqueue one result per failed
  // candidate. If the user opens WiFi during boot, leave its progress view
  // when automatic selection reaches Online or bounded retry, rather than
  // displaying an endless "Joining 12s" page.
  if (visible && page == Page::Connecting) {
    if (ctx.wifiConnection.autoPhase() == WiFiConnectionService::AutoPhase::Online &&
        ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Connected) {
      selectedSSID = WiFi.SSID();
      showResult(ctx, "Auto connection successful");
    } else if (ctx.wifiConnection.autoPhase() == WiFiConnectionService::AutoPhase::Waiting) {
      showResult(ctx, "Saved networks unavailable");
    }
  }
  if (ctx.wifiConnection.takeChanged() && visible) {
    if (page == Page::Networks && ctx.wifiConnection.phase() != WiFiConnectionService::Phase::Scanning) {
      ctx.ui.clearContent();
      screenPainted = false;
      draw(ctx);
    }
  }
  if (visible && !ctx.keyboard.active() && !popup.open &&
      ((page == Page::Networks && ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Scanning) ||
       page == Page::Connecting)) {
    paintProgress(ctx, false); // refresh just one timer line and the narrow bar
  }
}

ScreenId WiFiApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (ctx.keyboard.active()) {
    if (ctx.keyboard.handle(e)) {
      if (!ctx.keyboard.active()) {
        const bool confirmed = ctx.keyboard.accepted();
        String value = confirmed ? ctx.keyboard.value() : String();
        const Input previous = input;
        input = Input::None;
        if (confirmed && previous == Input::HiddenName) {
          value.trim();
          if (!value.length() || value.length() > 32) showResult(ctx, "SSID must be 1..32 bytes");
          else {
            selectedSSID = value;
            int saved = ctx.wifiProfiles.find(value);
            if (saved >= 0) {
              const WiFiProfile &p = ctx.wifiProfiles.at(saved);
              if (ctx.wifiConnection.connect(value, p.password, p.open, ctx.settings.data().wifiAuto && !ctx.system.safeMode())) {
                page = Page::Connecting; screenPainted = false;
              } else showResult(ctx, "Invalid saved profile");
            } else {
              input = Input::Password;
              ctx.keyboard.open("WiFi password", "", true);
            }
          }
        } else if (confirmed && previous == Input::Password) {
          // Manual hidden SSIDs with empty password are valid open networks.
          if (ctx.wifiConnection.connect(selectedSSID, value, value.length() == 0, ctx.settings.data().wifiAuto && !ctx.system.safeMode())) {
            page = Page::Connecting; screenPainted = false;
          } else showResult(ctx, "Invalid password (8..64)");
        }
      }
      draw(ctx);
    }
    return ScreenId::WiFi;
  }
  if (!e.pressed || e.longPress) return ScreenId::WiFi;

  if (popup.open) {
    if (e.key == Key::Start || e.key == Key::Select) {
      const int choice = popup.index;
      popup.close();
      switch (choice) {
        case 0:
          if (page == Page::Saved) connectSelected(ctx, true);
          else if (page == Page::Result) {
            const int saved = ctx.wifiProfiles.find(selectedSSID);
            if (saved >= 0) {
              savedIndex = saved;
              connectSelected(ctx, true);
            } else if (selectedSSID.length()) {
              if (selectedOpen) {
                if (ctx.wifiConnection.connect(selectedSSID, "", true,
                    ctx.settings.data().wifiAuto && !ctx.system.safeMode())) {
                  page = Page::Connecting; screenPainted = false;
                }
              } else {
                input = Input::Password;
                ctx.keyboard.open("WiFi password", "", true);
              }
            }
          } else connectSelected(ctx);
          break;
        case 1: scan(ctx); break;
        case 2: ctx.wifiConnection.disconnect(); page=Page::Networks; screenPainted=false; break;
        case 3: {
          String removeSSID;
          if (page == Page::Saved && ctx.wifiProfiles.count()) removeSSID=ctx.wifiProfiles.at(savedIndex).ssid;
          else if (page == Page::Networks && index < ctx.wifiConnection.count()) removeSSID=ctx.wifiConnection.network(index).ssid;
          if (removeSSID.length() && ctx.wifiProfiles.has(removeSSID)) {
            ctx.wifiProfiles.remove(removeSSID);
            if (statusWifi() && WiFi.SSID() == removeSSID) ctx.wifiConnection.disconnect();
            ctx.notifications.push("WiFi", "Forgot: " + removeSSID);
            savedIndex = min(savedIndex, max(0,ctx.wifiProfiles.count()-1));
          }
          break;
        }
        case 4: page=Page::Details; break;
        case 5: page=Page::Saved; savedIndex=savedOffset=0; break;
        case 6: page=Page::Networks; selectedSSID=""; selectedOpen=false;
                input=Input::HiddenName; ctx.keyboard.open("Hidden network", "", false); break;
        case 7: page=Page::Networks; break;
      }
      screenPainted=false;
      draw(ctx);
      return ScreenId::WiFi;
    }
    popupNav(popup, e, WIFI_OPTIONS);
    draw(ctx);
    return ScreenId::WiFi;
  }

  if (e.key == Key::A) {
    if (page == Page::Connecting) {
      ctx.wifiConnection.disconnect();
      page = Page::Networks; screenPainted = false; draw(ctx);
    } else if (page != Page::Networks) {
      page = Page::Networks; screenPainted=false; ctx.ui.clearContent(); draw(ctx);
    } else if (ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Scanning) {
      ctx.wifiConnection.cancelScan();
      page=Page::Networks;screenPainted=false;draw(ctx);
    } else return ScreenId::Launcher;
    return ScreenId::WiFi;
  }
  if (e.key == Key::Option) {
    popup.show(); drawPopup(ctx, popup, wifiOptions, WIFI_OPTIONS);
    return ScreenId::WiFi;
  }
  if (page == Page::Connecting) return ScreenId::WiFi;
  if (page == Page::Result) {
    if (e.key == Key::Start || e.key == Key::Select) {
      const int saved = ctx.wifiProfiles.find(selectedSSID);
      if (saved >= 0) { savedIndex=saved; connectSelected(ctx,true); }
      else if (selectedSSID.length()) {
        if (selectedOpen) {
          if (ctx.wifiConnection.connect(selectedSSID,"",true,ctx.settings.data().wifiAuto && !ctx.system.safeMode())) { page=Page::Connecting;screenPainted=false; }
        } else { input=Input::Password;ctx.keyboard.open("WiFi password","",true); }
      } else scan(ctx);
      draw(ctx);
    }
    return ScreenId::WiFi;
  }
  if (page == Page::Details) return ScreenId::WiFi;
  if (ctx.wifiConnection.phase() == WiFiConnectionService::Phase::Scanning) return ScreenId::WiFi;

  const bool savedView = page == Page::Saved;
  const int n = savedView ? ctx.wifiProfiles.count() : ctx.wifiConnection.count();
  int &cursor = savedView ? savedIndex : index;
  int &top = savedView ? savedOffset : offset;
  if ((e.key == Key::Up && cursor > 0) || (e.key == Key::Down && cursor < n-1)) {
    const int previous = cursor, oldTop = top;
    cursor += e.key == Key::Up ? -1 : 1;
    if (cursor < top) top--;
    if (cursor >= top+SymbianUI::LIST_VISIBLE) top++;
    if (oldTop != top) draw(ctx);
    else {
      auto rowPaint = [&](int i, bool focused) {
        String name, sub;
        if (savedView) {
          const WiFiProfile &p = ctx.wifiProfiles.at(i);
          name=p.ssid;sub=p.open?"Open  Saved":"Secured  Saved";
        } else {
          const auto &net = ctx.wifiConnection.network(i);
          name=net.ssid[0]?String(net.ssid):String("<hidden>");
          sub=String(net.rssi)+" dBm  "+(net.auth==WIFI_AUTH_OPEN?"Open":"Secured");
          if (ctx.wifiProfiles.has(net.ssid)) sub+="  Saved";
        }
        if (statusWifi() && name == WiFi.SSID()) sub="Connected  "+sub;
        ctx.ui.listItem(i-top,"Wi",name,sub,focused);
      };
      rowPaint(previous,false);rowPaint(cursor,true);
      ctx.ui.scrollbar(n,SymbianUI::LIST_VISIBLE,top);
    }
  } else if (e.key == Key::Start || e.key == Key::Select) {
    connectSelected(ctx,savedView);draw(ctx);
  }
  return ScreenId::WiFi;
}

// ---------------- BLE ----------------
static const char *const bleOptions[] = {"Details", "Rescan", "Clear results", "Bluetooth info"};
static constexpr int BLE_OPTIONS = sizeof(bleOptions) / sizeof(bleOptions[0]);

void BleApp::enter(AppContext &ctx) {
  popup.close();
  details = false;
  scan(ctx);
}

void BleApp::scan(AppContext &ctx) {
  ctx.ui.clearContent();
  if (!initialized) {
    NimBLEDevice::init("VQEAF-OS");
    initialized = true;
    gBleReady = true;
  }
  ctx.ui.chrome("Bluetooth", statusWifi(), true, ctx.storage.mounted(), ctx.settings.data().hour12);
  ctx.ui.message("Bluetooth", "Scanning BLE devices...", "Radio memory is released after scan");
  ctx.ui.softkeys("", "", "Back");

  NimBLEScan *scanner = NimBLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(60);
  scanner->setWindow(30);
  NimBLEScanResults results = scanner->getResults(2500, false);
  count=0;
  index = offset = 0;
  for (int i=0, limit=(int)results.getCount(); i<limit; ++i) {
    const NimBLEAdvertisedDevice *d=results.getDevice(i);
    if(!d)continue;
    const int16_t power=(int16_t)d->getRSSI();
    int slot=-1;
    if(count<MAX_DEVS) slot=count++;
    else {
      int weakest=0;
      for(int n=1;n<count;++n)if(devs[n].rssi<devs[weakest].rssi)weakest=n;
      if(power>devs[weakest].rssi)slot=weakest;
    }
    if(slot<0)continue;
    String name=d->haveName()?String(d->getName().c_str()):String("Unnamed device");
    String addr=String(d->getAddress().toString().c_str());
    snprintf(devs[slot].name,sizeof(devs[slot].name),"%s",name.c_str());
    snprintf(devs[slot].addr,sizeof(devs[slot].addr),"%s",addr.c_str());
    devs[slot].rssi=power;
  }
  scanner->clearResults();
  // Like Qeafbrowser WiFi results, strongest advertisements appear first.
  for (int i=0;i<count;++i) for(int j=i+1;j<count;++j)
    if(devs[j].rssi>devs[i].rssi) { Dev tmp=devs[i]; devs[i]=devs[j]; devs[j]=tmp; }

  // The OS currently scans only; it does not maintain BLE connections. Releasing
  // NimBLE here returns its host/controller allocations to the heap while the
  // copied scan results stay available in our fixed-size array.
  NimBLEDevice::deinit(true);
  initialized = false;
  gBleReady = false;
}

void BleApp::draw(AppContext &ctx) {
  ctx.ui.chrome("Bluetooth", statusWifi(), false, ctx.storage.mounted(), ctx.settings.data().hour12);
  if (details && count) {
    ctx.ui.clearContent();
    ctx.ui.message("BLE device", devs[index].name, devs[index].addr,
                   String(devs[index].rssi) + " dBm / scan-only");
    ctx.ui.softkeys("", "", "Back");
    return;
  }
  if (count == 0) {
    ctx.ui.message("Bluetooth", "No BLE devices found", "Options > Rescan");
  } else {
    for (int row = 0; row < SymbianUI::LIST_VISIBLE; ++row) {
      int i = offset + row;
      if (i >= count) { ctx.ui.clearListRow(row); continue; }
      ctx.ui.listItem(row, "BLE", devs[i].name, String(devs[i].addr) + "  " + String(devs[i].rssi) + "dBm", i == index);
    }
    ctx.ui.scrollbar(count, SymbianUI::LIST_VISIBLE, offset);
  }
  ctx.ui.softkeys("Options", "Details", "Back");
  drawPopup(ctx, popup, bleOptions, BLE_OPTIONS);
}

ScreenId BleApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::BLE;
  if (details) {
    if(e.key==Key::A || e.key==Key::Option) {details=false; draw(ctx);}
    return ScreenId::BLE;
  }

  if (popup.open) {
    if (e.key == Key::Start) {
      int choice = popup.index;
      popup.close();
      if (choice == 0 && count) {
        details=true; draw(ctx); return ScreenId::BLE;
      }
      if (choice == 1) scan(ctx);
      else if (choice == 2) { count = index = offset = 0; }
      else if (choice == 3) {
        ctx.ui.message("Bluetooth", "BLE scanner idle", "Radio starts only during scan", "Memory is released between scans");
        ctx.ui.softkeys("", "", "Back");
        return ScreenId::BLE;
      }
      draw(ctx);
      return ScreenId::BLE;
    }
    popupNav(popup,e,BLE_OPTIONS);
    if(popup.open)drawPopup(ctx,popup,bleOptions,BLE_OPTIONS);
    else draw(ctx);
    return ScreenId::BLE;
  }

  if (e.key == Key::A) return ScreenId::Launcher;
  if (e.key == Key::Option) { popup.show(); draw(ctx); }
  else if ((e.key == Key::Up && index > 0) || (e.key == Key::Down && index < count - 1)) {
    const int oldIndex = index, oldOffset = offset;
    index += (e.key == Key::Up) ? -1 : 1;
    if (index < offset) offset--;
    if (index >= offset + SymbianUI::LIST_VISIBLE) offset++;
    if (oldOffset != offset) {
      draw(ctx);
    } else {
      auto paintRow = [&](int item, bool selected) {
        const int row = item - offset;
        if (row < 0 || row >= SymbianUI::LIST_VISIBLE) return;
        ctx.ui.listItem(row, "BLE", devs[item].name,
                        String(devs[item].addr) + "  " + String(devs[item].rssi) + "dBm", selected);
      };
      paintRow(oldIndex, false);
      paintRow(index, true);
      ctx.ui.scrollbar(count, SymbianUI::LIST_VISIBLE, offset);
    }
  }
  else if (e.key == Key::Start && count) {
    details=true; draw(ctx);
  }
  return ScreenId::BLE;
}

static ScreenId fileOpenTarget(AppContext &ctx, const FsEntry &entry) {
  if (entry.isDir) return ScreenId::Files;
  String lower=entry.name; lower.toLowerCase();
  if (lower.endsWith(".vqeaf")) { ctx.pendingThemePath = entry.path; return ScreenId::Themes; }
  if (lower.endsWith(".qeapp")) { ctx.pendingPackagePath = entry.path; return ScreenId::AppInstaller; }
  if(lower.endsWith(".bmp")||lower.endsWith(".jpg")||lower.endsWith(".jpeg")||lower.endsWith(".png")) {
    ctx.pendingOpenPath=entry.path; return ScreenId::Gallery;
  }
  if(lower.endsWith(".txt")||lower.endsWith(".md")||lower.endsWith(".log")||lower.endsWith(".json")||lower.endsWith(".ini")||lower.endsWith(".csv")) {
    ctx.pendingOpenPath=entry.path; return ScreenId::TextViewer;
  }
  return ScreenId::Files;
}

// ---------------- Files ----------------
static const char *const fileOptions[] = {"Open", "Properties", "Delete", "Refresh", "Root folder", "Downloads", "Themes", "App inbox"};
static constexpr int FILE_OPTIONS = sizeof(fileOptions) / sizeof(fileOptions[0]);

void FilesApp::reload(AppContext &ctx) {
  count = ctx.storage.list(path, entries, MAX_ENTRIES);
  index = offset = 0;
  confirmDelete = false;
}

void FilesApp::enter(AppContext &ctx) {
  path = "/";
  if (ctx.pendingFolderPath.length() && ctx.storage.exists(ctx.pendingFolderPath)) {
    File f = ctx.storage.fs().open(ctx.pendingFolderPath);
    if (f && f.isDirectory()) path = ctx.pendingFolderPath;
    if (f) f.close();
  }
  ctx.pendingFolderPath = "";
  popup.close();
  reload(ctx);
}

void FilesApp::parent(AppContext &ctx) {
  if (path == "/") return;
  int slash = path.lastIndexOf('/');
  path = slash <= 0 ? "/" : path.substring(0, slash);
  reload(ctx);
}

void FilesApp::showProperties(AppContext &ctx) {
  if (!count) return;
  String type = entries[index].isDir ? "Folder" : "File";
  String size = entries[index].isDir ? "" : String((unsigned long)entries[index].size) + " bytes";
  ctx.ui.message("Properties", entries[index].name, type + "  " + size, entries[index].path);
  ctx.ui.softkeys("", "", "Back");
}

void FilesApp::draw(AppContext &ctx) {
  String title = path == "/" ? "File manager" : "Files " + path;
  ctx.ui.chrome(title, statusWifi(), gBleReady, ctx.storage.mounted(), ctx.settings.data().hour12);

  if (!ctx.storage.mounted()) {
    ctx.ui.message("File manager", "microSD is not mounted");
    ctx.ui.softkeys("", "", "Back");
    return;
  }
  if (confirmDelete && count) {
    ctx.ui.dialog("Delete file?", entries[index].name, "This cannot be undone.",
                  "Delete", "Cancel", confirmChoice);
    ctx.ui.softkeys("", "Select", "Cancel");
    return;
  }

  if (!count) {
    ctx.ui.message("File manager", "Folder is empty");
  } else {
    for (int row = 0; row < SymbianUI::LIST_VISIBLE; ++row) {
      int i = offset + row;
      if (i >= count) { ctx.ui.clearListRow(row); continue; }
      String sub = entries[i].isDir ? "Folder" : String((unsigned long)(entries[i].size / 1024ULL)) + " KB";
      ctx.ui.listItem(row, entries[i].isDir ? "Dir" : "File", entries[i].name, sub, i == index);
    }
    ctx.ui.scrollbar(count, SymbianUI::LIST_VISIBLE, offset);
  }
  ctx.ui.softkeys("Options", "Open", "Back");
  drawPopup(ctx, popup, fileOptions, FILE_OPTIONS);
}

ScreenId FilesApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::Files;

  if (confirmDelete) {
    if (e.key == Key::Left || e.key == Key::Right) {
      confirmChoice = confirmChoice ? 0 : 1;
      draw(ctx);
    } else if (e.key == Key::Start || e.key == Key::Select) {
      if (confirmChoice == 0 && count && !entries[index].isDir) {
        ctx.storage.remove(entries[index].path);
        reload(ctx);
      } else {
        confirmDelete = false;
      }
      draw(ctx);
    } else if (e.key == Key::A || e.key == Key::B) {
      confirmDelete = false;
      draw(ctx);
    }
    return ScreenId::Files;
  }

  if (popup.open) {
    if (e.key == Key::Start) {
      int choice = popup.index;
      popup.close();
      if (choice == 0 && count) {
        if (entries[index].isDir) { path = entries[index].path; reload(ctx); draw(ctx); return ScreenId::Files; }
        ScreenId target=fileOpenTarget(ctx,entries[index]);
        if(target!=ScreenId::Files)return target;
        showProperties(ctx); return ScreenId::Files;
      }
      if (choice == 1 && count) { showProperties(ctx); return ScreenId::Files; }
      if (choice == 2 && count && !entries[index].isDir) { confirmDelete = true; confirmChoice = 1; draw(ctx); return ScreenId::Files; }
      if (choice == 3) { reload(ctx); draw(ctx); return ScreenId::Files; }
      if (choice == 4) { path = "/"; reload(ctx); draw(ctx); return ScreenId::Files; }
      if (choice == 5) { path = StoragePaths::DOWNLOADS; reload(ctx); draw(ctx); return ScreenId::Files; }
      if (choice == 6) { path = StoragePaths::THEMES; reload(ctx); draw(ctx); return ScreenId::Files; }
      if (choice == 7) { path = StoragePaths::APPS_INBOX; reload(ctx); draw(ctx); return ScreenId::Files; }
    }
    popupNav(popup, e, FILE_OPTIONS);
    draw(ctx);
    return ScreenId::Files;
  }

  if (e.key == Key::A) {
    if (path == "/") return ScreenId::Launcher;
    parent(ctx);
    draw(ctx);
  } else if ((e.key == Key::Up && index > 0) || (e.key == Key::Down && index < count - 1)) {
    const int oldIndex = index, oldOffset = offset;
    index += (e.key == Key::Up) ? -1 : 1;
    if (index < offset) offset--;
    if (index >= offset + SymbianUI::LIST_VISIBLE) offset++;
    if (oldOffset != offset) {
      draw(ctx);
    } else {
      auto paintRow = [&](int item, bool selected) {
        const int row = item - offset;
        if (row < 0 || row >= SymbianUI::LIST_VISIBLE) return;
        String sub = entries[item].isDir ? "Folder" : String((unsigned long)(entries[item].size / 1024ULL)) + " KB";
        ctx.ui.listItem(row, entries[item].isDir ? "Dir" : "File", entries[item].name, sub, selected);
      };
      paintRow(oldIndex, false);
      paintRow(index, true);
      ctx.ui.scrollbar(count, SymbianUI::LIST_VISIBLE, offset);
    }
  } else if (e.key == Key::Start && count) {
    if (entries[index].isDir) { path = entries[index].path; reload(ctx); draw(ctx); }
    else { ScreenId target=fileOpenTarget(ctx,entries[index]); if(target!=ScreenId::Files)return target; showProperties(ctx); }
  } else if (e.key == Key::Option) {
    popup.show();
    draw(ctx);
  }
  return ScreenId::Files;
}

// ---------------- Collection ----------------
static const char *const collectionOptions[] = {"Open category", "Refresh", "Open music", "Storage info"};
static constexpr int COLLECTION_OPTIONS = sizeof(collectionOptions) / sizeof(collectionOptions[0]);

void CollectionApp::enter(AppContext &ctx) {
  index = 0;
  popup.close();
  const char *imgExt[] = {".bmp", ".jpg", ".jpeg", ".png"};
  const char *musExt[] = {".wav", ".mp3", ".aac", ".flac"};
  const char *docExt[] = {".txt", ".md", ".log", ".json", ".ini", ".csv"};
  images = ctx.storage.countMedia("/", imgExt, 4, 2);
  music  = ctx.storage.countMedia("/", musExt, 4, 2);
  docs   = ctx.storage.countMedia("/", docExt, 6, 2);
  total = images + music + docs;
}

void CollectionApp::draw(AppContext &ctx) {
  ctx.ui.chrome("Library", statusWifi(), false, false, ctx.settings.data().hour12);
  const char *name[] = {"Images", "Music", "Documents", "Indexed total"};
  const char *ic[] = {"Pic", "Mus", "Doc", "All"};
  int val[] = {images, music, docs, total};
  for (int i = 0; i < 4; ++i) ctx.ui.listItem(i, ic[i], name[i], String(val[i]) + " files", i == index);
  ctx.ui.softkeys("Options", "View", "Back");
  drawPopup(ctx, popup, collectionOptions, COLLECTION_OPTIONS);
}

ScreenId CollectionApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::Collection;

  if (popup.open) {
    if (e.key == Key::Start) {
      int choice = popup.index;
      popup.close();
      if (choice == 0) {
        if (index == 0) return ScreenId::Gallery;
        if (index == 1) return ScreenId::Music;
        if (index == 2) return ScreenId::TextViewer;
        return ScreenId::Collection;
      }
      if (choice == 1) { enter(ctx); draw(ctx); return ScreenId::Collection; }
      if (choice == 2) return ScreenId::Music;
      if (choice == 3) {
        ctx.ui.message("Storage", ctx.storage.mounted() ? "microSD mounted" : "microSD unavailable",
                       ctx.storage.mounted() ? String((unsigned long)ctx.storage.cardSizeMB()) + " MB card" : "",
                       String(total) + " indexed media files");
        ctx.ui.softkeys("", "", "Back");
        return ScreenId::Collection;
      }
    }
    popupNav(popup, e, COLLECTION_OPTIONS);
    draw(ctx);
    return ScreenId::Collection;
  }

  if (e.key == Key::A) return ScreenId::Launcher;
  if ((e.key == Key::Up && index > 0) || (e.key == Key::Down && index < 3)) {
    const int oldIndex = index;
    index += (e.key == Key::Up) ? -1 : 1;
    const char *name[] = {"Images", "Music", "Documents", "Indexed total"};
    const char *ic[] = {"Pic", "Mus", "Doc", "All"};
    int val[] = {images, music, docs, total};
    ctx.ui.listItem(oldIndex, ic[oldIndex], name[oldIndex], String(val[oldIndex]) + " files", false);
    ctx.ui.listItem(index, ic[index], name[index], String(val[index]) + " files", true);
  }
  else if (e.key == Key::Option) { popup.show(); draw(ctx); }
  else if (e.key == Key::Start && index == 0) return ScreenId::Gallery;
  else if (e.key == Key::Start && index == 1) return ScreenId::Music;
  else if (e.key == Key::Start && index == 2) return ScreenId::TextViewer;
  return ScreenId::Collection;
}

// ---------------- Music ----------------
// List + Now Playing remain one app/task. No waveform or album framebuffer.
static const char *const musicOptions[] = {
  "Now playing", "Play / Pause", "Next track", "Previous track", "Shuffle toggle",
  "Repeat mode", "Stop", "Rescan library", "Track details"
};
static constexpr int MUSIC_OPTIONS = sizeof(musicOptions)/sizeof(musicOptions[0]);

void MusicApp::enter(AppContext &ctx) {
  popup.close(); nowPlaying=false;
  const char *ext[]={".wav"};
  count=ctx.storage.scanMedia(StoragePaths::MUSIC,ext,1,tracks,MAX_TRACKS,2);
  if(!count) count=ctx.storage.scanMedia("/",ext,1,tracks,MAX_TRACKS,3);
  index=offset=0;
  playingIndex=-1;
  if(ctx.music.playing()) {
    for(int i=0;i<count;++i) if(tracks[i].path==ctx.music.currentPath()) {
      playingIndex=index=i; offset=(i/5)*5; break;
    }
  }
  playlist.resetCycle(playingIndex);
}
int MusicApp::nextIndex(bool forward) {
  return playlist.next(count,playingIndex>=0?playingIndex:index,
                       forward,false,millis());
}
void MusicApp::playIndex(AppContext &ctx,int item) {
  if(item<0 || item>=count) return;
  if(ctx.music.play(tracks[item].path)) {
    playingIndex=index=item;
    playlist.markPlayed(item);
    if(index<offset)offset=index;
    if(index>=offset+5)offset=index-4;
  }else{
    playingIndex=-1;
  }
  lastPaintSecond=0xFFFFFFFFUL;
}
void MusicApp::playerFooter(AppContext &ctx) {
  auto &t=ctx.ui.display(); const auto c=ctx.ui.c();
  t.fillRect(4,241,Board::SCREEN_W-9,47,c.panel);
  t.setTextFont(1);t.setTextSize(1);t.setTextColor(c.text,c.panel);
  const char *rep=playlist.repeatName();
  t.setCursor(10,250);
  t.print(ctx.music.status()+" / V"+String(ctx.music.getVolume())+"%");
  t.setCursor(10,268);
  t.print(String("Shuffle ")+(playlist.shuffle()?"ON":"OFF")+"  Repeat "+rep);
}
void MusicApp::paintVolume(AppContext &ctx) {
  auto &t=ctx.ui.display(); const auto c=ctx.ui.c();
  t.fillRect(20,259,199,33,c.bg);
  t.setTextFont(1);t.setTextColor(c.text,c.bg);t.setCursor(26,261);
  t.print(String("Volume ")+String(ctx.music.getVolume())+"%");
  // Clear the interior to avoid leaving old fill behind when decreasing.
  t.fillRect(25,278,188,8,c.bg);
  ctx.ui.progress(24,277,190,ctx.music.getVolume());
}
void MusicApp::draw(AppContext &ctx) {
  ctx.ui.chrome(nowPlaying?"Now playing":"Music",statusWifi(),false,ctx.storage.mounted(),ctx.settings.data().hour12);
  if(nowPlaying){
    auto &t=ctx.ui.display(); const auto c=ctx.ui.c();
    ctx.ui.clearContent();
    t.fillRect(15,47,210,124,c.panel);t.drawRect(15,47,210,124,c.dim);
    // procedural pixel note, no persistent icon bitmap
    t.fillRect(102,57,5,47,c.accent);t.fillRect(105,57,34,5,c.accent);
    t.fillCircle(93,105,11,c.accent);t.fillCircle(126,98,11,c.accent);
    t.setTextFont(2);t.setTextColor(c.text,c.panel);
    String label=playingIndex>=0?tracks[playingIndex].name:String("No track selected");
    if(label.length()>22)label=label.substring(0,21)+"~";
    t.setCursor(24,180);t.print(label);
    t.setTextFont(1);t.setCursor(25,209);
    t.print("Left/Right prev/next; Up/Down volume");
    lastPaintSecond=0xFFFFFFFFUL;
    tick(ctx,true);
    paintVolume(ctx);
    ctx.ui.softkeys("Options",ctx.music.playing() ? (ctx.music.paused()?"Resume":"Pause") : "Play","List");
    drawPopup(ctx,popup,musicOptions,MUSIC_OPTIONS);
    return;
  }
  for(int row=0;row<5;++row){
    int i=offset+row;
    if(i>=count){ctx.ui.clearListRow(row);continue;}
    String label=tracks[i].name;
    if(i==playingIndex)label=String("> ")+label;
    ctx.ui.listItem(row,"Mus",label,tracks[i].path,i==index);
  }
  ctx.ui.scrollbar(count,5,offset,35,237);
  playerFooter(ctx);
  if(!count){
    auto &t=ctx.ui.display(); auto c=ctx.ui.c(); t.setCursor(10,278);
    t.setTextFont(1);t.setTextColor(c.dim,c.panel);t.print("Copy PCM16 WAV to /Media/Music");
  }
  ctx.ui.softkeys("Options",ctx.music.playing() && ctx.music.paused()?"Resume":"Play","Back");
  drawPopup(ctx,popup,musicOptions,MUSIC_OPTIONS);
}
void MusicApp::tick(AppContext &ctx,bool visible){
  if(ctx.music.consumeFinished()){
    int next=playlist.next(count,playingIndex,true,true,millis());
    if(next>=0)playIndex(ctx,next);
    else playingIndex=-1;
    if(visible){draw(ctx);return;}
  }
  if(!visible||!nowPlaying)return;
  const uint32_t sec=ctx.music.playedSeconds();
  if(lastPaintSecond==sec)return;
  lastPaintSecond=sec;
  auto &t=ctx.ui.display(); auto c=ctx.ui.c();
  t.fillRect(25,223,190,16,c.bg);
  t.setTextColor(c.text,c.bg);t.setTextFont(1);t.setCursor(26,224);
  const uint32_t duration=ctx.music.durationSeconds();
  char timebuf[40];snprintf(timebuf,sizeof timebuf,"%02lu:%02lu / %02lu:%02lu",
    (unsigned long)(sec/60),(unsigned long)(sec%60),
    (unsigned long)(duration/60),(unsigned long)(duration%60));
  t.print(timebuf);
  ctx.ui.progress(24,243,190,duration?int((uint64_t(sec)*100U)/duration):0);
}
ScreenId MusicApp::handle(AppContext &ctx,const KeyEvent &e){
  if(!e.pressed||e.longPress)return ScreenId::Music;
  if(popup.open){
    if(e.key==Key::Start){
      const int choice=popup.index; popup.close();
      switch(choice){
        case 0: nowPlaying=true; break;
        case 1: if(ctx.music.playing()&&playingIndex==index)ctx.music.togglePause();else playIndex(ctx,index);break;
        case 2: {int n=nextIndex(true);if(n>=0)playIndex(ctx,n);break;}
        case 3: {int n=nextIndex(false);if(n>=0)playIndex(ctx,n);break;}
        case 4: playlist.toggleShuffle(playingIndex);break;
        case 5: playlist.cycleRepeat();break;
        case 6: ctx.music.stop();playingIndex=-1;break;
        case 7: enter(ctx);break;
        case 8: if(count){ctx.ui.message("Track details",tracks[index].name,tracks[index].path,
          String((unsigned long)tracks[index].size)+" bytes");ctx.ui.softkeys("","","Back");return ScreenId::Music;}break;
      }
      draw(ctx);return ScreenId::Music;
    }
    popupNav(popup,e,MUSIC_OPTIONS);
    if(popup.open)drawPopup(ctx,popup,musicOptions,MUSIC_OPTIONS);
    else draw(ctx);
    return ScreenId::Music;
  }
  if(e.key==Key::Option){popup.show();drawPopup(ctx,popup,musicOptions,MUSIC_OPTIONS);return ScreenId::Music;}
  if(nowPlaying){
    if(e.key==Key::A){nowPlaying=false;draw(ctx);}
    else if(e.key==Key::Start){
      if(ctx.music.playing())ctx.music.togglePause();else if(count)playIndex(ctx,playingIndex>=0?playingIndex:index);
      draw(ctx);
    }else if(e.key==Key::Left||e.key==Key::Right){int n=nextIndex(e.key==Key::Right);if(n>=0)playIndex(ctx,n);draw(ctx);}
    else if(e.key==Key::Up||e.key==Key::Down){
      int v=ctx.music.getVolume()+(e.key==Key::Up?5:-5);
      ctx.music.setVolume(constrain(v,0,100));
      ctx.settings.data().volume=ctx.music.getVolume();ctx.settings.save();
      paintVolume(ctx);
    }
    return ScreenId::Music;
  }
  if(e.key==Key::A)return ScreenId::Launcher;
  if((e.key==Key::Up&&index>0)||(e.key==Key::Down&&index<count-1)){
    int old=index,prev=offset;index+=e.key==Key::Up?-1:1;
    if(index<offset)--offset;
    if(index>=offset+5)++offset;
    if(prev!=offset)draw(ctx);
    else{
      auto row=[&](int item,bool focused){int r=item-offset;if(r<0||r>=5)return;
        String label=tracks[item].name;if(item==playingIndex)label=String("> ")+label;
        ctx.ui.listItem(r,"Mus",label,tracks[item].path,focused);
      };
      row(old,false);row(index,true);ctx.ui.scrollbar(count,5,offset,35,237);
    }
  }else if(e.key==Key::Start&&count){
    if(playingIndex==index&&ctx.music.playing())ctx.music.togglePause();else playIndex(ctx,index);
    nowPlaying=true;draw(ctx);
  }
  return ScreenId::Music;
}

// ---------------- Gallery ----------------
static const char *const galleryOptions[] = {"View", "Next image", "Previous image", "Slideshow", "Stop slideshow", "Rescan", "File info"};
static constexpr int GALLERY_OPTIONS = sizeof(galleryOptions) / sizeof(galleryOptions[0]);

void GalleryApp::enter(AppContext &ctx) {
  returnTo = ctx.pendingOpenPath.length() ? ScreenId::Files : ScreenId::Launcher;
  popup.close(); preview = false; slideshow=false; index = offset = 0;
  const char *ext[] = {".bmp", ".jpg", ".jpeg", ".png"};
  count = ctx.storage.scanMedia(StoragePaths::PICTURES, ext, 4, images, MAX_IMAGES, 2);
  if (!count) count = ctx.storage.scanMedia("/", ext, 4, images, MAX_IMAGES, 3);
  if(ctx.pendingOpenPath.length()){
    bool found=false;
    for(int i=0;i<count;++i)if(images[i].path==ctx.pendingOpenPath){
      index=i;offset=(i/SymbianUI::LIST_VISIBLE)*SymbianUI::LIST_VISIBLE;preview=true;found=true;break;
    }
    // File Manager may open an image stored outside Media/Pictures.
    if(!found&&ctx.storage.mounted()){
      File f=ctx.storage.fs().open(ctx.pendingOpenPath,FILE_READ);
      if(f&&!f.isDirectory()){
        int pos=ctx.pendingOpenPath.lastIndexOf('/');
        images[0]=FsEntry(ctx.pendingOpenPath.substring(pos+1),ctx.pendingOpenPath,false,f.size());
        count=1;index=offset=0;preview=true;
      }
      if(f)f.close();
    }
    ctx.pendingOpenPath="";
  }
}

void GalleryApp::drawPreview(AppContext &ctx) {
  ctx.ui.chrome("Gallery", statusWifi(), false, false, ctx.settings.data().hour12);
  ctx.ui.clearContent();
  if (!count) {
    ctx.ui.message("Gallery", "No images found");
    ctx.ui.softkeys("", "", "Back");
    return;
  }
  TFT_eSPI &d = ctx.ui.display(); ThemeColors c = ctx.ui.c();
  d.fillRect(8, 39, 224, 220, c.panel);
  d.drawRect(8, 39, 224, 220, c.dim);
  String err;
  bool ok = ctx.imageViewer.drawFile(d, ctx.storage.fs(), images[index].path, 11, 42, 218, 194, err);
  d.fillRect(10, 239, 220, 18, c.panel);
  d.setTextFont(1); d.setTextColor(ok ? c.text : c.dim, c.panel);
  String label = ok ? images[index].name : (String("Cannot preview: ") + err);
  if (label.length() > 36) label = label.substring(0,35) + "~";
  d.setCursor(14, 245); d.print(label);
  if(slideshow){
    d.setCursor(14,266);d.setTextColor(c.dim,c.bg);
    d.print(String("Slideshow ")+String(index+1)+"/"+String(count)+"  4s");
  }
  ctx.ui.softkeys("List", slideshow?"Pause":"Next", "Back");
}
void GalleryApp::tick(AppContext &ctx,bool visible){
  if(!visible||!slideshow||!preview||count<2)return;
  const uint32_t now=millis();
  if((uint32_t)(now-lastSlideAt)>=4000UL){
    lastSlideAt=now;index=(index+1)%count;drawPreview(ctx);
  }
}

void GalleryApp::draw(AppContext &ctx) {
  if (preview) { drawPreview(ctx); return; }
  ctx.ui.chrome("Gallery", statusWifi(), false, false, ctx.settings.data().hour12);
  if (!ctx.storage.mounted()) {
    ctx.ui.message("Gallery", "microSD is not mounted");
  } else if (!count) {
    ctx.ui.message("Gallery", "No BMP/JPEG/PNG images found");
  } else {
    for (int row=0; row<SymbianUI::LIST_VISIBLE; ++row) {
      int i=offset+row;
      if (i>=count) { ctx.ui.clearListRow(row); continue; }
      String sub=String((unsigned long)(images[i].size/1024ULL))+" KB";
      ctx.ui.listItem(row,"Pic",images[i].name,sub,i==index);
    }
    ctx.ui.scrollbar(count,SymbianUI::LIST_VISIBLE,offset);
  }
  ctx.ui.softkeys("Options","View","Back");
  drawPopup(ctx,popup,galleryOptions,GALLERY_OPTIONS);
}

ScreenId GalleryApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::Gallery;
  if (preview) {
    if (e.key==Key::A || e.key==Key::B) { preview=false; slideshow=false; draw(ctx); return ScreenId::Gallery; }
    if (e.key==Key::Option) { slideshow=!slideshow; lastSlideAt=millis(); drawPreview(ctx); return ScreenId::Gallery; }
    if ((e.key==Key::Left || e.key==Key::Up) && count) {
      index=(index+count-1)%count;lastSlideAt=millis();drawPreview(ctx);
    }
    else if ((e.key==Key::Right || e.key==Key::Down || e.key==Key::Start) && count) {
      index=(index+1)%count;lastSlideAt=millis();drawPreview(ctx);
    }
    return ScreenId::Gallery;
  }
  if (popup.open) {
    if (e.key==Key::Start || e.key==Key::Select) {
      int choice=popup.index; popup.close();
      if (choice==0 && count) { preview=true; drawPreview(ctx); return ScreenId::Gallery; }
      if (choice==1 && count) { index=(index+1)%count; preview=true; drawPreview(ctx); return ScreenId::Gallery; }
      if (choice==2 && count) { index=(index+count-1)%count; preview=true; drawPreview(ctx); return ScreenId::Gallery; }
      if (choice==3 && count) { slideshow=true;preview=true;lastSlideAt=millis();drawPreview(ctx);return ScreenId::Gallery; }
      if (choice==4) {slideshow=false;draw(ctx);return ScreenId::Gallery;}
      if (choice==5) { enter(ctx); draw(ctx); return ScreenId::Gallery; }
      if (choice==6 && count) {
        ctx.ui.message("Image info",images[index].name,images[index].path,String((unsigned long)images[index].size)+" bytes");
        ctx.ui.softkeys("","","Back"); return ScreenId::Gallery;
      }
    }
    popupNav(popup,e,GALLERY_OPTIONS);
    if(popup.open)drawPopup(ctx,popup,galleryOptions,GALLERY_OPTIONS);
    else draw(ctx);
    return ScreenId::Gallery;
  }
  if (e.key==Key::A || e.key==Key::B) return returnTo;
  if (e.key==Key::Option) { popup.show(); drawPopup(ctx,popup,galleryOptions,GALLERY_OPTIONS); return ScreenId::Gallery; }
  if ((e.key==Key::Up && index>0) || (e.key==Key::Down && index<count-1)) {
    int old=index, oldOff=offset; index += e.key==Key::Up ? -1 : 1;
    if (index<offset) --offset; if (index>=offset+SymbianUI::LIST_VISIBLE) ++offset;
    if (oldOff!=offset) draw(ctx);
    else {
      auto rowPaint=[&](int item,bool sel){ int row=item-offset; if(row<0||row>=SymbianUI::LIST_VISIBLE)return; ctx.ui.listItem(row,"Pic",images[item].name,String((unsigned long)(images[item].size/1024ULL))+" KB",sel); };
      rowPaint(old,false); rowPaint(index,true); ctx.ui.scrollbar(count,SymbianUI::LIST_VISIBLE,offset);
    }
  } else if ((e.key==Key::Start || e.key==Key::Select) && count) { preview=true; drawPreview(ctx); }
  return ScreenId::Gallery;
}

// ---------------- Text viewer ----------------
static const char *const textOptions[] = {"Open", "Next page", "Previous page", "Rescan", "File info"};
static constexpr int TEXT_OPTIONS = sizeof(textOptions)/sizeof(textOptions[0]);

void TextViewerApp::enter(AppContext &ctx) {
  returnTo = ctx.pendingPackageLaunch ? ScreenId::Applications : (ctx.pendingOpenPath.length() ? ScreenId::Files : ScreenId::Applications);
  bool fromPackage = ctx.pendingPackageLaunch;
  ctx.pendingPackageLaunch = false;
  popup.close(); viewing=false; index=offset=page=0; nextOffset=0; lineCount=0;
  memset(offsets,0,sizeof(offsets));
  const char *ext[] = {".txt", ".md", ".log", ".json", ".ini", ".csv"};
  count=ctx.storage.scanMedia(StoragePaths::DOCUMENTS,ext,6,docs,MAX_DOCS,2);
  if(!count)count=ctx.storage.scanMedia("/",ext,6,docs,MAX_DOCS,3);
  if (fromPackage) {
    if (!ctx.pendingOpenPath.length() || !ctx.storage.exists(ctx.pendingOpenPath)) {
      ctx.pendingOpenPath="";
      return;
    }
    File f=ctx.storage.fs().open(ctx.pendingOpenPath,FILE_READ);
    if (f && !f.isDirectory()) {
      docs[0]=FsEntry("Package document",ctx.pendingOpenPath,false,f.size());count=1;index=offset=0;
      page=0;offsets[0]=0;loadPage(ctx,0);viewing=true;
    }
    if (f) f.close();
    ctx.pendingOpenPath="";
    return;
  }
  if(ctx.pendingOpenPath.length()){for(int i=0;i<count;++i)if(docs[i].path==ctx.pendingOpenPath){index=i;offset=(i/SymbianUI::LIST_VISIBLE)*SymbianUI::LIST_VISIBLE;page=0;offsets[0]=0;loadPage(ctx,0);viewing=true;break;}ctx.pendingOpenPath="";}
}

bool TextViewerApp::loadPage(AppContext &ctx, uint32_t fileOffset) {
  lineCount=0; nextOffset=fileOffset;
  for (int i=0;i<PAGE_LINES;++i) pageLines[i][0]=0;
  if (!count || !ctx.storage.mounted()) return false;
  File f=ctx.storage.fs().open(docs[index].path,FILE_READ);
  if (!f) return false;
  if (!f.seek(fileOffset)) { f.close(); return false; }
  char line[40]; int col=0;
  while (f.available() && lineCount<PAGE_LINES) {
    int r=f.read(); if (r<0) break; char ch=(char)r;
    if (ch=='\r') continue;
    if (ch=='\t') ch=' ';
    if ((unsigned char)ch < 32 && ch!='\n') continue;
    if (ch=='\n' || col>=38) {
      line[col]=0; snprintf(pageLines[lineCount++],40,"%s",line); col=0;
      if (ch!='\n') {
        if (lineCount >= PAGE_LINES) {
          // The wrap-triggering character belongs to the next page. Rewind one
          // byte so page boundaries never drop text.
          uint32_t pos=f.position(); if(pos>0) f.seek(pos-1);
          break;
        }
        line[col++]=ch;
      }
      continue;
    }
    line[col++]=ch;
  }
  if (col && lineCount<PAGE_LINES) { line[col]=0; snprintf(pageLines[lineCount++],40,"%s",line); }
  nextOffset=f.position(); f.close(); return true;
}

void TextViewerApp::drawViewer(AppContext &ctx) {
  ctx.ui.chrome(docs[index].name,statusWifi(),false,false,ctx.settings.data().hour12);
  ctx.ui.clearContent();
  TFT_eSPI &d=ctx.ui.display(); ThemeColors c=ctx.ui.c();
  d.setTextFont(1); d.setTextSize(1); d.setTextColor(c.text,c.bg);
  int y=39;
  for (int i=0;i<lineCount;++i) { d.setCursor(7,y); d.print(pageLines[i]); y+=16; }
  d.fillRect(0,279,Board::SCREEN_W,16,c.panel);
  d.setTextColor(c.dim,c.panel); d.setCursor(7,284); d.print(String("Page ")+String(page+1)+"  "+String((unsigned long)nextOffset)+" bytes");
  ctx.ui.softkeys("List","","Back");
}

void TextViewerApp::draw(AppContext &ctx) {
  if (viewing) { drawViewer(ctx); return; }
  ctx.ui.chrome("Text viewer",statusWifi(),false,false,ctx.settings.data().hour12);
  if (!count) ctx.ui.message("Text viewer","No TXT/MD/LOG/JSON files found");
  else {
    for(int row=0;row<SymbianUI::LIST_VISIBLE;++row){int i=offset+row; if(i>=count){ctx.ui.clearListRow(row);continue;} ctx.ui.listItem(row,"Doc",docs[i].name,String((unsigned long)(docs[i].size/1024ULL))+" KB",i==index);} 
    ctx.ui.scrollbar(count,SymbianUI::LIST_VISIBLE,offset);
  }
  ctx.ui.softkeys("Options","Open","Back"); drawPopup(ctx,popup,textOptions,TEXT_OPTIONS);
}

ScreenId TextViewerApp::handle(AppContext &ctx,const KeyEvent &e) {
  if(!e.pressed||e.longPress)return ScreenId::TextViewer;
  if(viewing){
    if(e.key==Key::A||e.key==Key::B||e.key==Key::Option){viewing=false;draw(ctx);return ScreenId::TextViewer;}
    if((e.key==Key::Down||e.key==Key::Right||e.key==Key::Start) && nextOffset<docs[index].size && page<PAGE_STACK-1){offsets[++page]=nextOffset;loadPage(ctx,nextOffset);drawViewer(ctx);}
    else if((e.key==Key::Up||e.key==Key::Left)&&page>0){--page;loadPage(ctx,offsets[page]);drawViewer(ctx);} return ScreenId::TextViewer;
  }
  if(popup.open){
    if(e.key==Key::Start||e.key==Key::Select){int choice=popup.index;popup.close();
      if(choice==0&&count){page=0;offsets[0]=0;loadPage(ctx,0);viewing=true;drawViewer(ctx);return ScreenId::TextViewer;}
      if(choice==1&&count){page=0;offsets[0]=0;loadPage(ctx,0);viewing=true;drawViewer(ctx);return ScreenId::TextViewer;}
      if(choice==2&&count){page=0;offsets[0]=0;loadPage(ctx,0);viewing=true;drawViewer(ctx);return ScreenId::TextViewer;}
      if(choice==3){enter(ctx);draw(ctx);return ScreenId::TextViewer;}
      if(choice==4&&count){ctx.ui.message("Document info",docs[index].name,docs[index].path,String((unsigned long)docs[index].size)+" bytes");ctx.ui.softkeys("","","Back");return ScreenId::TextViewer;}
    }
    popupNav(popup,e,TEXT_OPTIONS);draw(ctx);return ScreenId::TextViewer;
  }
  if(e.key==Key::A||e.key==Key::B)return returnTo;
  if(e.key==Key::Option){popup.show();drawPopup(ctx,popup,textOptions,TEXT_OPTIONS);return ScreenId::TextViewer;}
  if((e.key==Key::Up&&index>0)||(e.key==Key::Down&&index<count-1)){int old=index,oldOff=offset;index+=e.key==Key::Up?-1:1;if(index<offset)--offset;if(index>=offset+SymbianUI::LIST_VISIBLE)++offset;if(oldOff!=offset)draw(ctx);else{auto rp=[&](int item,bool sel){int row=item-offset;if(row<0||row>=SymbianUI::LIST_VISIBLE)return;ctx.ui.listItem(row,"Doc",docs[item].name,String((unsigned long)(docs[item].size/1024ULL))+" KB",sel);};rp(old,false);rp(index,true);ctx.ui.scrollbar(count,SymbianUI::LIST_VISIBLE,offset);}}
  else if((e.key==Key::Start||e.key==Key::Select)&&count){page=0;offsets[0]=0;loadPage(ctx,0);viewing=true;drawViewer(ctx);} return ScreenId::TextViewer;
}

// ---------------- Qeafbrowser ----------------
static const char *const browserOptions[] = {"Enter address", "Home", "Reload", "Back", "Download link", "Downloads", "Page info"};
static constexpr int BROWSER_OPTIONS=sizeof(browserOptions)/sizeof(browserOptions[0]);
static constexpr int BROWSER_VISIBLE=13;

void BrowserApp::loadHome(AppContext &ctx) {
  if (ctx.system.safeMode()) return;
  ctx.ui.chrome("Qeafbrowser",statusWifi(),false,false,ctx.settings.data().hour12);
  ctx.ui.message("Qeafbrowser","Loading...","https://qeafivels.com/"); ctx.ui.softkeys("","","Cancel");
  ctx.browser.load("https://qeafivels.com/");
  offset=0; selectedLink=ctx.browser.linkCount()?0:-1;
}

void BrowserApp::enter(AppContext &ctx) {
  popup.close(); urlEntry=false; offset=0; selectedLink=-1;
  launchedFromPackage = ctx.pendingPackageLaunch;
  ctx.pendingPackageLaunch = false;
  if(ctx.system.safeMode() || !ctx.browser.available()) return;
  if (ctx.pendingBrowserUrl.length()) {
    String url=ctx.pendingBrowserUrl;ctx.pendingBrowserUrl="";
    ctx.ui.message("Opening application", "Qeafbrowser", url);
    ctx.browser.load(url);
    if(ctx.browser.linkCount())selectedLink=0;
    return;
  }
  if(ctx.browser.lineCount()==0 && statusWifi()) loadHome(ctx);
  if(ctx.browser.linkCount()) selectedLink=0;
}

void BrowserApp::redrawBody(AppContext &ctx) {
  ctx.ui.clearContent();
  TFT_eSPI &d=ctx.ui.display(); ThemeColors c=ctx.ui.c();
  d.fillRect(5,36,230,24,c.panel); d.drawRect(5,36,230,24,c.dim);
  d.setTextFont(1); d.setTextColor(c.dim,c.panel); String u=ctx.browser.url(); if(u.length()>38)u=u.substring(0,37)+"~"; d.setCursor(9,44); d.print(u); if(ctx.browser.pageFromCache()){d.setTextColor(c.accent,c.panel);d.setCursor(201,44);d.print("C");}
  int y=66;
  for(int row=0;row<BROWSER_VISIBLE;++row){int i=offset+row;if(i>=ctx.browser.lineCount())break;const BrowserLine &ln=ctx.browser.lineAt(i);bool sel=ln.link>=0&&ln.link==selectedLink;uint16_t bg=sel?c.selected:c.bg;d.fillRect(4,y-2,232,15,bg);d.setTextColor(ln.link>=0?0x05FF:c.text,bg);d.setCursor(7,y);d.print(ln.text);if(sel)d.drawRect(4,y-2,232,15,c.border);y+=16;}
  ctx.ui.scrollbar(ctx.browser.lineCount(),BROWSER_VISIBLE,offset,64,278);
  ctx.ui.softkeys("Options",selectedLink>=0?"Open":"","Back");
}

void BrowserApp::draw(AppContext &ctx) {
  if(ctx.keyboard.active()){ctx.keyboard.draw(ctx.ui,statusWifi(),false,false,ctx.settings.data().hour12);return;}
  ctx.ui.chrome("Qeafbrowser",statusWifi(),false,false,ctx.settings.data().hour12);
  if(ctx.system.safeMode()){ctx.ui.message("Safe mode","Qeafbrowser is disabled","Leave Safe Mode in Recovery");ctx.ui.softkeys("","","Back");return;}
  if(!ctx.browser.available()){ctx.ui.message("Qeafbrowser","Browser memory unavailable","Restart or use Recovery");ctx.ui.softkeys("","","Back");return;}
  if(!statusWifi() && ctx.browser.lineCount()==0){ctx.ui.message("Qeafbrowser","WiFi is not connected","Options > Home can open cache");ctx.ui.softkeys("Options","","Back");drawPopup(ctx,popup,browserOptions,BROWSER_OPTIONS);return;}
  if(ctx.browser.lineCount()==0){String err=ctx.browser.error();ctx.ui.message("Qeafbrowser",err.length()?err:"No page loaded","Options > Home or Enter address");ctx.ui.softkeys("Options","","Back");drawPopup(ctx,popup,browserOptions,BROWSER_OPTIONS);return;}
  redrawBody(ctx); drawPopup(ctx,popup,browserOptions,BROWSER_OPTIONS);
}

void BrowserApp::moveLink(AppContext &ctx,int direction){int n=ctx.browser.linkCount();if(!n){selectedLink=-1;return;}if(selectedLink<0)selectedLink=direction>0?0:n-1;else selectedLink=(selectedLink+n+direction)%n;int first=-1;for(int i=0;i<ctx.browser.lineCount();++i){if(ctx.browser.lineAt(i).link==selectedLink){first=i;break;}}if(first>=0){if(first<offset)offset=first;else if(first>=offset+BROWSER_VISIBLE)offset=first-BROWSER_VISIBLE+1;}redrawBody(ctx);}

ScreenId BrowserApp::handle(AppContext &ctx,const KeyEvent &e){
  if(ctx.keyboard.active()){if(ctx.keyboard.handle(e)){if(ctx.keyboard.accepted()){String u=ctx.keyboard.value();ctx.ui.message("Qeafbrowser","Loading...",u);ctx.browser.load(u);offset=0;selectedLink=ctx.browser.linkCount()?0:-1;}draw(ctx);}return ScreenId::Browser;}
  if(!e.pressed||e.longPress)return ScreenId::Browser;
  if(ctx.system.safeMode()){if(e.key==Key::A||e.key==Key::B)return launchedFromPackage?ScreenId::Applications:ScreenId::Launcher;return ScreenId::Browser;}
  if(popup.open){if(e.key==Key::Start||e.key==Key::Select){int choice=popup.index;popup.close();if(choice==0){ctx.keyboard.open("Web address",ctx.browser.url(),false);draw(ctx);return ScreenId::Browser;}if(choice==1){loadHome(ctx);draw(ctx);return ScreenId::Browser;}if(choice==2){ctx.ui.message("Qeafbrowser","Reloading...",ctx.browser.url());ctx.browser.reload();offset=0;selectedLink=ctx.browser.linkCount()?0:-1;draw(ctx);return ScreenId::Browser;}if(choice==3){ctx.browser.goBack();offset=0;selectedLink=ctx.browser.linkCount()?0:-1;draw(ctx);return ScreenId::Browser;}if(choice==4){
        if(selectedLink<0){ctx.ui.message("Download","Select a page link first");ctx.ui.softkeys("","","Back");return ScreenId::Browser;}
        String saved,err;ctx.ui.message("Qeafbrowser","Downloading...",ctx.browser.linkAt(selectedLink).label);
        if(ctx.browser.download(ctx.browser.linkAt(selectedLink).url,saved,err)){
          ctx.notifications.push("Download complete",saved);
          String lower=saved;lower.toLowerCase();
          if(lower.endsWith(".qeapp")){
            ctx.pendingPackagePath=saved;
            return ScreenId::AppInstaller;
          }
          ctx.ui.message("Download complete",saved,"Saved to microSD");
        }
        else ctx.ui.message("Download failed",err,ctx.browser.linkAt(selectedLink).url);
        ctx.ui.softkeys("","","Back");return ScreenId::Browser;
      }if(choice==5){ctx.pendingFolderPath=StoragePaths::DOWNLOADS;return ScreenId::Files;}if(choice==6){ctx.ui.message("Page info",ctx.browser.title(),ctx.browser.url(),String(ctx.browser.pageFromCache()?"CACHE  ":"HTTP ")+String(ctx.browser.status())+"  "+String(ctx.browser.lineCount())+" lines");ctx.ui.softkeys("","","Back");return ScreenId::Browser;}}popupNav(popup,e,BROWSER_OPTIONS);draw(ctx);return ScreenId::Browser;}
  if(e.key==Key::A||e.key==Key::B)return launchedFromPackage?ScreenId::Applications:ScreenId::Launcher;
  if(e.key==Key::Option){popup.show();drawPopup(ctx,popup,browserOptions,BROWSER_OPTIONS);return ScreenId::Browser;}
  if(e.key==Key::Left){moveLink(ctx,-1);return ScreenId::Browser;}if(e.key==Key::Right){moveLink(ctx,1);return ScreenId::Browser;}
  if(e.key==Key::Up&&offset>0){--offset;redrawBody(ctx);}else if(e.key==Key::Down&&offset+BROWSER_VISIBLE<ctx.browser.lineCount()){++offset;redrawBody(ctx);}else if((e.key==Key::Start||e.key==Key::Select)&&selectedLink>=0){ctx.ui.message("Qeafbrowser","Opening link...",ctx.browser.linkAt(selectedLink).label);ctx.browser.openLink(selectedLink);offset=0;selectedLink=ctx.browser.linkCount()?0:-1;draw(ctx);}return ScreenId::Browser;
}

// ---------------- S3 diagnostic shell ----------------
static const char *const shellOptions[] = {"Command", "Network monitor", "System monitor", "File commands", "Help", "Clear", "Recovery"};
static constexpr int SHELL_OPTIONS = sizeof(shellOptions) / sizeof(shellOptions[0]);
static constexpr int SHELL_VISIBLE = 18;

void ShellApp::enter(AppContext &ctx) {
  (void)ctx;
  popup.close();
  historyIndex = -1;
}

void ShellApp::openCommand(AppContext &ctx, const String &initial) {
  ctx.keyboard.open("Shell command", initial, false);
}

void ShellApp::draw(AppContext &ctx) {
  if (ctx.keyboard.active()) {
    ctx.keyboard.draw(ctx.ui, statusWifi(), false, false, ctx.settings.data().hour12);
    return;
  }

  ctx.ui.chrome("Shell", statusWifi(), false, false, ctx.settings.data().hour12);
  TFT_eSPI &d = ctx.ui.display();
  ThemeColors c = ctx.ui.c();
  d.fillRect(0, SymbianUI::CONTENT_TOP, Board::SCREEN_W,
             SymbianUI::SOFTKEY_TOP - SymbianUI::CONTENT_TOP, TFT_BLACK);

  const int total = ctx.shell.lineCount();
  const int first = max(0, total - SHELL_VISIBLE);
  int y = 36;
  d.setTextFont(1); d.setTextSize(1);
  for (int i = first; i < total; ++i) {
    const char *line = ctx.shell.lineAt(i);
    uint16_t ink = (line[0] == '$') ? 0x07E0 : 0xC618;
    d.setTextColor(ink, TFT_BLACK);
    d.setCursor(4, y);
    d.print(line);
    y += 12;
  }

  d.fillRect(0, 258, Board::SCREEN_W, 36, TFT_BLACK);
  d.drawFastHLine(0, 258, Board::SCREEN_W, 0x4208);
  d.setTextColor(0x07E0, TFT_BLACK);
  String prompt = String("s3:") + ctx.shell.cwd() + "$";
  if (prompt.length() > 34) prompt = "s3:~$";
  d.setCursor(4, 266); d.print(prompt);
  d.setTextColor(0x8410, TFT_BLACK);
  d.setCursor(4, 279); d.print("START=command  UP=history");

  ctx.ui.softkeys("Options", "Command", "Back");
  drawPopup(ctx, popup, shellOptions, SHELL_OPTIONS);
  (void)c;
}

ScreenId ShellApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (ctx.keyboard.active()) {
    if (ctx.keyboard.handle(e)) {
      if (!ctx.keyboard.active() && ctx.keyboard.accepted()) {
        String command = ctx.keyboard.value();
        historyIndex = -1;
        ctx.shell.execute(command, ctx.notifications);
        if (ctx.shell.takeRebootRequest()) {
          draw(ctx);
          delay(250);
          ESP.restart();
        }
      }
      draw(ctx);
    }
    return ScreenId::Shell;
  }
  if (!e.pressed || e.longPress) return ScreenId::Shell;

  if (popup.open) {
    if (e.key == Key::Start || e.key == Key::Select) {
      int choice = popup.index;
      popup.close();
      if (choice == 0) openCommand(ctx);
      else if (choice == 1) ctx.shell.execute("netmon", ctx.notifications);
      else if (choice == 2) ctx.shell.execute("top", ctx.notifications);
      else if (choice == 3) {
        ctx.shell.execute("echo FILES: cp mv mkdir rm rmdir touch", ctx.notifications);
        ctx.shell.execute("echo write append hexdump cat stat ls", ctx.notifications);
      }
      else if (choice == 4) ctx.shell.execute("help", ctx.notifications);
      else if (choice == 5) ctx.shell.clear();
      else if (choice == 6) return ScreenId::Recovery;
      draw(ctx);
      return ScreenId::Shell;
    }
    popupNav(popup, e, SHELL_OPTIONS);
    draw(ctx);
    return ScreenId::Shell;
  }

  if (e.key == Key::A || e.key == Key::B) return ScreenId::Applications;
  if (e.key == Key::Start || e.key == Key::Select) {
    openCommand(ctx);
    draw(ctx);
  } else if (e.key == Key::Option) {
    popup.show();
    drawPopup(ctx, popup, shellOptions, SHELL_OPTIONS);
  } else if (e.key == Key::Up && ctx.shell.historyCount() > 0) {
    historyIndex = min(historyIndex + 1, ctx.shell.historyCount() - 1);
    openCommand(ctx, ctx.shell.historyAt(historyIndex));
    draw(ctx);
  } else if (e.key == Key::Down && historyIndex >= 0) {
    historyIndex--;
    openCommand(ctx, historyIndex >= 0 ? ctx.shell.historyAt(historyIndex) : String());
    draw(ctx);
  }
  return ScreenId::Shell;
}

// ---------------- Recovery / Safe Mode ----------------
static const char *const recoveryOptions[]={"Boot status","Start normal mode","Enable Safe Mode","Clear recovery flags","Restart device"};
static constexpr int RECOVERY_COUNT=sizeof(recoveryOptions)/sizeof(recoveryOptions[0]);

void RecoveryApp::draw(AppContext &ctx){
  ctx.ui.chrome(ctx.system.safeMode()?"Safe mode":"Recovery",statusWifi(),false,false,ctx.settings.data().hour12);
  String subs[RECOVERY_COUNT];
  subs[0]=String(ctx.system.lastResetReasonText())+" / crashes "+String(ctx.system.consecutiveCrashes());
  subs[1]="Disable Safe Mode and restart";
  subs[2]=ctx.system.safeMode()?"Safe Mode is active":"Use minimal drivers on next boot";
  subs[3]="Clear crash-loop markers";
  subs[4]="Software restart";
  for(int i=0;i<RECOVERY_COUNT;++i)ctx.ui.listItem(i,i==2?"Lock":"Rec",recoveryOptions[i],subs[i],i==index);
  ctx.ui.softkeys("","Select","Back");
}

ScreenId RecoveryApp::handle(AppContext &ctx,const KeyEvent &e){if(!e.pressed||e.longPress)return ScreenId::Recovery;if(e.key==Key::A||e.key==Key::B)return ScreenId::Applications;if(e.key==Key::Up&&index>0){int old=index;--index;draw(ctx);}else if(e.key==Key::Down&&index<RECOVERY_COUNT-1){++index;draw(ctx);}else if(e.key==Key::Start||e.key==Key::Select){if(index==0){ctx.ui.message("Recovery status",ctx.system.lastResetReasonText(),String("Crash streak: ")+ctx.system.consecutiveCrashes(),ctx.system.safeMode()?"Safe Mode active":"Normal mode");ctx.ui.softkeys("","","Back");}else if(index==1){ctx.system.setSafeMode(false);ctx.ui.message("Recovery","Restarting in normal mode...");delay(250);ESP.restart();}else if(index==2){ctx.system.setSafeMode(true);ctx.ui.message("Recovery","Safe Mode enabled","Restarting with minimal services...");delay(250);ESP.restart();}else if(index==3){ctx.system.clearRecoveryState();ctx.notifications.push("Recovery","Crash flags cleared");draw(ctx);}else if(index==4){ctx.ui.message("Recovery","Restarting device...");delay(250);ESP.restart();}}return ScreenId::Recovery;}

// ---------------- Settings ----------------
static const char *const settingsOptions[] = {"Change", "Reset appearance", "System info", "About", "Close options"};
static constexpr int SETTINGS_OPTIONS = sizeof(settingsOptions) / sizeof(settingsOptions[0]);
static constexpr int SETTINGS_COUNT = 7;

static String lockTimeoutText(uint16_t sec) {
  if (sec == 0) return "Off";
  if (sec < 60) return String(sec) + " sec";
  return String(sec / 60) + " min";
}

static uint16_t nextLockTimeout(uint16_t current, int direction) {
  const uint16_t values[] = {0, 30, 60, 120, 300};
  int pos = 0;
  for (int i = 0; i < 5; ++i) if (values[i] == current) pos = i;
  pos += direction;
  if (pos < 0) pos = 4;
  if (pos > 4) pos = 0;
  return values[pos];
}

static void changeSetting(AppContext &ctx, int index, Key key) {
  SystemSettings &s = ctx.settings.data();
  if (index == 0) {
    // Theme cycle: Legacy Lime, AMOLED Red, Black and VQEAF Night.
    if (key == Key::Left) {
      s.theme = s.theme == ThemeId::External ? ThemeId::S60Green :
                (s.theme == ThemeId::S60Green ? ThemeId::Classic :
                (s.theme == ThemeId::Classic ? ThemeId::Black :
                (s.theme == ThemeId::Black ? ThemeId::AmoledRed : ThemeId::S60Green)));
    } else {
      s.theme = s.theme == ThemeId::External ? ThemeId::S60Green :
                (s.theme == ThemeId::S60Green ? ThemeId::AmoledRed :
                (s.theme == ThemeId::AmoledRed ? ThemeId::Black :
                (s.theme == ThemeId::Black ? ThemeId::Classic : ThemeId::S60Green)));
    }
    ctx.settings.selectBuiltInTheme(s.theme);
    ctx.ui.setTheme(s.theme);
    ctx.ui.clear();
  } else if (index == 1) {
    int d = (key == Key::Left) ? -10 : 10;
    s.brightness = constrain((int)s.brightness + d, 20, 100);
    analogWrite(Board::TFT_LEDK_PIN, map(s.brightness, 0, 100, 0, 255));
  } else if (index == 2) {
    int d = (key == Key::Left) ? -5 : 5;
    s.volume = constrain((int)s.volume + d, 0, 100);
    ctx.music.setVolume(s.volume);
  } else if (index == 3) {
    s.hour12 = !s.hour12;
  } else if (index == 4) {
    s.wifiAuto = !s.wifiAuto;
    ctx.wifiConnection.configureAuto(s.wifiAuto && !ctx.system.safeMode());
  } else if (index == 5) {
    s.lockTimeoutSec = nextLockTimeout(s.lockTimeoutSec, key == Key::Left ? -1 : 1);
  }
  ctx.settings.save();
}

void SettingsApp::draw(AppContext &ctx) {
  ctx.ui.chrome("Settings", statusWifi(), gBleReady, ctx.storage.mounted(), ctx.settings.data().hour12);
  SystemSettings &s = ctx.settings.data();
  const char *labels[SETTINGS_COUNT] = {
    "Theme", "Backlight", "Audio volume", "Clock format",
    "Auto WiFi strongest", "Auto keypad lock", "About"
  };
  const char *ics[SETTINGS_COUNT] = {"Th", "Br", "Mus", "Clk", "Wi", "Lock", "i"};

  for (int row = 0; row < SymbianUI::LIST_VISIBLE; ++row) {
    int i = offset + row;
    if (i >= SETTINGS_COUNT) { ctx.ui.clearListRow(row); continue; }
    String value;
    if (i == 0) value = s.theme == ThemeId::External ? "Custom: SD theme" : themeName(s.theme);
    else if (i == 1) value = String(s.brightness) + "%";
    else if (i == 2) value = String(s.volume) + "%";
    else if (i == 3) value = s.hour12 ? "12-hour" : "24-hour";
    else if (i == 4) value = s.wifiAuto ? "On" : "Off";
    else if (i == 5) value = lockTimeoutText(s.lockTimeoutSec);
    else value = "VQEAF OS v2.1.0";
    ctx.ui.listItem(row, ics[i], labels[i], value, i == index);
  }
  ctx.ui.scrollbar(SETTINGS_COUNT, SymbianUI::LIST_VISIBLE, offset);
  ctx.ui.softkeys("Options", (index == 6 || index == 0) ? "Open" : "Change", "Back");
  drawPopup(ctx, popup, settingsOptions, SETTINGS_OPTIONS);
}

ScreenId SettingsApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::Settings;

  if (popup.open) {
    if (e.key == Key::Start || e.key == Key::Select) {
      int choice = popup.index;
      popup.close();
      if (choice == 0) {
        if (index == 6) return ScreenId::About;
        if (index == 0) return ScreenId::Themes;
        changeSetting(ctx, index, Key::Right);
      } else if (choice == 1) {
        SystemSettings &s = ctx.settings.data();
        s.theme = ThemeId::S60Green;
        s.brightness = 90;
        ctx.settings.selectBuiltInTheme(s.theme);
        ctx.ui.setTheme(s.theme);
        ctx.ui.clear();
        analogWrite(Board::TFT_LEDK_PIN, map(s.brightness, 0, 100, 0, 255));
        ctx.settings.save();
      } else if (choice == 2) return ScreenId::SystemInfo;
      else if (choice == 3) return ScreenId::About;
      draw(ctx);
      return ScreenId::Settings;
    }
    popupNav(popup, e, SETTINGS_OPTIONS);
    draw(ctx);
    return ScreenId::Settings;
  }

  if (e.key == Key::A || e.key == Key::B) return ScreenId::Launcher;
  if ((e.key == Key::Up && index > 0) || (e.key == Key::Down && index < SETTINGS_COUNT - 1)) {
    const int oldIndex = index, oldOffset = offset;
    index += (e.key == Key::Up) ? -1 : 1;
    if (index < offset) --offset;
    if (index >= offset + SymbianUI::LIST_VISIBLE) ++offset;
    if (oldOffset != offset) {
      draw(ctx);
    } else {
      const char *labels[SETTINGS_COUNT] = {"Theme", "Backlight", "Audio volume", "Clock format", "Auto WiFi strongest", "Auto keypad lock", "About"};
      const char *ics[SETTINGS_COUNT] = {"Th", "Br", "Mus", "Clk", "Wi", "Lock", "i"};
      auto paintRow = [&](int item, bool selected) {
        SystemSettings &s = ctx.settings.data();
        String value;
        if (item == 0) value = s.theme == ThemeId::External ? "Custom: SD theme" : themeName(s.theme);
        else if (item == 1) value = String(s.brightness) + "%";
        else if (item == 2) value = String(s.volume) + "%";
        else if (item == 3) value = s.hour12 ? "12-hour" : "24-hour";
        else if (item == 4) value = s.wifiAuto ? "On" : "Off";
        else if (item == 5) value = lockTimeoutText(s.lockTimeoutSec);
        else value = "VQEAF OS v2.1.0";
        ctx.ui.listItem(item - offset, ics[item], labels[item], value, selected);
      };
      paintRow(oldIndex, false);
      paintRow(index, true);
      ctx.ui.scrollbar(SETTINGS_COUNT, SymbianUI::LIST_VISIBLE, offset);
      ctx.ui.softkeys("Options", (index == 6 || index == 0) ? "Open" : "Change", "Back");
    }
  } else if (e.key == Key::Option) {
    popup.show();
    draw(ctx);
  } else if (e.key == Key::Start || e.key == Key::Select || e.key == Key::Left || e.key == Key::Right) {
    if (index == 6) return ScreenId::About;
    if (index == 0 && (e.key == Key::Start || e.key == Key::Select)) return ScreenId::Themes;
    changeSetting(ctx, index, e.key);
    draw(ctx);
  }
  return ScreenId::Settings;
}

// ---------------- Theme manager (read-only VQEAF import) ----------------
static const ThemeId builtinThemeIds[4] = {
  ThemeId::Classic, ThemeId::AmoledRed, ThemeId::Black, ThemeId::S60Green
};
static const char *const themeActions[] = {"Apply", "Rescan microSD", "Theme details"};
static constexpr int THEME_ACTIONS = sizeof(themeActions)/sizeof(themeActions[0]);

void ThemesApp::enter(AppContext &ctx, ScreenId from) {
  if (from == ScreenId::Launcher || from == ScreenId::Applications ||
      from == ScreenId::Settings || from == ScreenId::Files) returnTo = from;
  popup.close(); showDetails = false; feedback = "";
  ctx.themes.scan(ctx.storage);
  index = offset = 0;
  if (ctx.pendingThemePath.length()) {
    int found = ctx.themes.find(ctx.pendingThemePath);
    if (found >= 0) index = BUILTIN_COUNT + found;
    else feedback = "Theme not found. Rescan SD";
    ctx.pendingThemePath = "";
  } else if (ctx.settings.data().theme == ThemeId::External) {
    int found = ctx.themes.find(ctx.settings.selectedThemePath());
    if (found >= 0) index = BUILTIN_COUNT + found;
    else feedback = "Saved theme missing; Night fallback";
  } else {
    // Focus the currently applied built-in theme when opening Themes.
    for (int i = 0; i < BUILTIN_COUNT; ++i)
      if (builtinThemeIds[i] == ctx.settings.data().theme) { index = i; break; }
  }
  if (index >= SymbianUI::LIST_VISIBLE) offset = index - SymbianUI::LIST_VISIBLE + 1;
}

bool ThemesApp::apply(AppContext &ctx) {
  if (index < BUILTIN_COUNT) {
    ThemeId id = builtinThemeIds[index];
    ctx.settings.selectBuiltInTheme(id);
    ctx.ui.setTheme(id);
    feedback = String(themeName(id)) + " applied";
    ctx.notifications.push("Theme applied", themeName(id));
    ctx.ui.clear();
    return true;
  }
  const int slot = index - BUILTIN_COUNT;
  if (slot < 0 || slot >= ctx.themes.count()) return false;
  const ThemeFileService::Entry &item = ctx.themes.at(slot);
  ThemeColors palette; LauncherStyle launcherSkin;
  String name = item.label, error;
  if (!ctx.themes.load(ctx.storage, item.path, palette, name, error, &launcherSkin)) {
    feedback = error;
    ctx.notifications.push("Invalid theme", error);
    return false; // preserve current theme and its NVS entry
  }
  // Commit only after full validation, then force a coherent redraw.
  ctx.settings.selectThemeFile(item.path);
  ctx.ui.setExternalTheme(palette, &launcherSkin);
  ctx.notifications.push("Theme applied", name);
  feedback = name + " applied";
  ctx.ui.clear();
  return true;
}

void ThemesApp::draw(AppContext &ctx) {
  ctx.ui.chrome("Themes", statusWifi(), false, false, ctx.settings.data().hour12);
  for (int row = 0; row < SymbianUI::LIST_VISIBLE; ++row) {
    int i = offset + row;
    if (i >= total(ctx)) { ctx.ui.clearListRow(row); continue; }
    String title, sub;
    if (i < BUILTIN_COUNT) {
      title = themeName(builtinThemeIds[i]);
      sub = ctx.settings.data().theme == builtinThemeIds[i] ? "Applied | Built-in" : "Built-in";
    } else {
      const ThemeFileService::Entry &file = ctx.themes.at(i - BUILTIN_COUNT);
      title = file.label;
      sub = ctx.settings.data().theme == ThemeId::External && ctx.settings.selectedThemePath() == file.path
          ? "Applied | microSD" : "microSD / .vqeaf";
    }
    ctx.ui.listItem(row, "Th", title, sub, i == index);
  }
  ctx.ui.scrollbar(total(ctx), SymbianUI::LIST_VISIBLE, offset);
  if (feedback.length()) {
    // Draw inside content area below 6 rows, not over the 240x320 softkey bar.
    TFT_eSPI &d = ctx.ui.display(); ThemeColors c = ctx.ui.c();
    d.fillRect(3, 281, 230, 15, c.bg);
    d.setTextFont(1); d.setTextSize(1); d.setTextColor(c.dim, c.bg);
    String clipped = feedback.substring(0, 36);
    d.setCursor(4, 284); d.print(clipped);
  } else if (!ctx.themes.hasCard()) {
    TFT_eSPI &d = ctx.ui.display(); ThemeColors c = ctx.ui.c();
    d.fillRect(3, 281, 230, 15, c.bg);
    d.setTextFont(1); d.setTextSize(1); d.setTextColor(c.dim,c.bg);
    d.setCursor(4, 284); d.print("No SD: built-in themes available");
  }
  ctx.ui.softkeys("Options", "Apply", "Back");
  drawPopup(ctx, popup, themeActions, THEME_ACTIONS);
}

ScreenId ThemesApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::Themes;
  if (showDetails) { showDetails = false; draw(ctx); return ScreenId::Themes; }
  if (popup.open) {
    if (e.key == Key::Start || e.key == Key::Select) {
      int action = popup.index;
      popup.close();
      if (action == 0) apply(ctx);
      else if (action == 1) {
        // Preserve the selected file across a refresh where possible.
        String selected = index >= BUILTIN_COUNT
            ? String(ctx.themes.at(index - BUILTIN_COUNT).path) : String();
        ctx.themes.scan(ctx.storage);
        int refreshed = selected.length() ? ctx.themes.find(selected) : -1;
        index = refreshed >= 0 ? BUILTIN_COUNT + refreshed :
                (index < BUILTIN_COUNT ? index : 0);
        offset = index >= SymbianUI::LIST_VISIBLE
            ? index - SymbianUI::LIST_VISIBLE + 1 : 0;
        feedback = ctx.themes.hasCard() ? String(ctx.themes.count()) + " SD themes found"
            : "microSD not mounted";
      } else if (action == 2) {
        String title = index < BUILTIN_COUNT ? themeName(builtinThemeIds[index]) : ctx.themes.at(index - BUILTIN_COUNT).label;
        String desc = index < BUILTIN_COUNT ? "Embedded RGB565 palette" : ctx.themes.at(index - BUILTIN_COUNT).path;
        ctx.ui.message("Theme information", title, desc, "START to return");
        ctx.ui.softkeys("", "", "Back");
        showDetails = true;
        return ScreenId::Themes;
      }
      draw(ctx); // Full list redraw only when the popup closes.
    } else {
      const bool wasOpen = popup.open;
      popupNav(popup, e, THEME_ACTIONS);
      if (wasOpen && popup.open) drawPopup(ctx, popup, themeActions, THEME_ACTIONS);
      else draw(ctx);
    }
    return ScreenId::Themes;
  }
  if (e.key == Key::A || e.key == Key::B) return returnTo;
  if ((e.key == Key::Up && index > 0) || (e.key == Key::Down && index < total(ctx)-1)) {
    int old = index, oldOffset = offset;
    index += e.key == Key::Up ? -1 : 1;
    if (index < offset) --offset;
    if (index >= offset + SymbianUI::LIST_VISIBLE) ++offset;
    if (oldOffset != offset) draw(ctx);
    else {
      // Partial redraw for selection changes: no full-screen flash on keypad.
      auto rp = [&](int i, bool selected) {
        String title, sub;
        if (i < BUILTIN_COUNT) {
          title = themeName(builtinThemeIds[i]);
          sub = ctx.settings.data().theme == builtinThemeIds[i] ? "Applied | Built-in" : "Built-in";
        } else {
          const ThemeFileService::Entry &it = ctx.themes.at(i - BUILTIN_COUNT);
          title = it.label;
          sub = ctx.settings.data().theme == ThemeId::External && ctx.settings.selectedThemePath() == it.path ? "Applied | microSD" : "microSD / .vqeaf";
        }
        ctx.ui.listItem(i-offset, "Th", title, sub, selected);
      };
      rp(old, false); rp(index, true);
      ctx.ui.scrollbar(total(ctx), SymbianUI::LIST_VISIBLE, offset);
    }
  } else if (e.key == Key::Start || e.key == Key::Select) {
    apply(ctx); draw(ctx);
  } else if (e.key == Key::Option) {
    popup.show(); drawPopup(ctx, popup, themeActions, THEME_ACTIONS);
  }
  return ScreenId::Themes;
}

// ---------------- QEAPP package manager ----------------
static const char *const INSTALLER_OPTS[] = {
  "Details", "Install / Uninstall", "Inbox / Installed", "Rescan", "Applications"
};
static constexpr int INSTALLER_OPT_COUNT = 5;

void AppInstallerApp::reload(AppContext &ctx) {
  count=0;index=0;offset=0;details=false;confirm=false;feedback="";selectedPath="";previewIconReady=false;
  if (!ctx.storage.mounted()) return;
  ctx.installer.refresh();
  if (installedTab) { count=ctx.installer.count();return; }
  // Stream directory entries directly into our 12-entry pool: no 40-item
  // temporary array on the Arduino loop task stack.
  File folder=ctx.storage.fs().open(StoragePaths::APPS_INBOX,FILE_READ);
  if(!folder||!folder.isDirectory())return;
  File item=folder.openNextFile();
  while(item&&count<MAX_PACKAGES){
    String full=item.name();int pos=full.lastIndexOf('/');
    String name=pos<0?full:full.substring(pos+1);
    String lower=name;lower.toLowerCase();
    if(!item.isDirectory()&&lower.endsWith(".qeapp")) {
      const String filePath=String(StoragePaths::APPS_INBOX)+"/"+name;
      inbox[count]=FsEntry(name,filePath,false,item.size());
      Qeapp::Meta meta;String readError;
      InboxLabel &label=inboxLabel[count];
      label.manifestOk=ctx.installer.inspect(filePath,meta,readError);
      label.name[0]=label.version[0]=label.type[0]=0;
      if(label.manifestOk){
        snprintf(label.name,sizeof label.name,"%s",meta.name);
        snprintf(label.version,sizeof label.version,"%s",meta.version);
        snprintf(label.type,sizeof label.type,"%s",meta.type);
      }
      ++count;
    }
    item.close();item=folder.openNextFile();
  }
  if(item)item.close();
  folder.close();
}

void AppInstallerApp::enter(AppContext &ctx,ScreenId from){
  returnTo=(from==ScreenId::Files||from==ScreenId::Browser)?from:ScreenId::Applications;
  installedTab=false;popup.close();reload(ctx);
  if(ctx.pendingPackagePath.length()){
    selectedPath=ctx.pendingPackagePath;ctx.pendingPackagePath="";
    // A package opened from File Manager can be outside the inbox.
    openDetails(ctx);
  }
}

void AppInstallerApp::paintRow(AppContext &ctx,int item,bool selected){
 int row=item-offset;if(row<0||row>=SymbianUI::LIST_VISIBLE)return;
 if(installedTab){
   if(item>=ctx.installer.count()){ctx.ui.clearListRow(row);return;}
   const auto &app=ctx.installer.at(item);
   ctx.ui.listItem(row,"App",app.info.name,String("v")+app.info.version+" / "+app.info.type,selected);
   uint16_t pixels[1024];
   if(ctx.installer.loadIcon(app.info.id,pixels))ctx.ui.display().pushImage(9,SymbianUI::CONTENT_TOP+1+row*SymbianUI::LIST_ROW_H+10,32,32,pixels);
 }else{
   if(item>=count){ctx.ui.clearListRow(row);return;}
   if(inboxLabel[item].manifestOk)
     ctx.ui.listItem(row,"App",inboxLabel[item].name,
       String("v")+inboxLabel[item].version+" / "+inboxLabel[item].type,selected);
   else ctx.ui.listItem(row,"File",inbox[item].name,"Unsigned / invalid - Details",selected);
 }
}

void AppInstallerApp::openDetails(AppContext &ctx){
 details=true;confirm=false;popup.close();verified=false;feedback="";previewIconReady=false;
 if(installedTab){
   if(index>=0&&index<ctx.installer.count()){
     const auto &a=ctx.installer.at(index);selectedMeta=a.info;selectedPath=ctx.installer.installedPath(a.info.id);
     verified=ctx.installer.get(a.info.id,selectedMeta);
     if(!verified)feedback="Installed app signature invalid";
   }else feedback="No installed app selected";
 }else{
   if(!selectedPath.length()&&index>=0&&index<count)selectedPath=inbox[index].path;
   if(selectedPath.length())verified=ctx.installer.inspect(selectedPath,selectedMeta,feedback);
   else feedback="No .qeapp selected";
 }
 if (verified) previewIconReady = installedTab ?
   ctx.installer.loadIcon(selectedMeta.id,previewPixels) :
   ctx.installer.previewIcon(selectedPath,previewPixels);
}

void AppInstallerApp::draw(AppContext &ctx){
 const char *tab=installedTab?"Installed apps":"App inbox";
 ctx.ui.chrome(tab,statusWifi(),false,false,ctx.settings.data().hour12);
 if(!ctx.storage.mounted()){
   ctx.ui.message("App installer","microSD is not available");ctx.ui.softkeys("","","Back");return;
 }
 if(confirm){
   ctx.ui.dialog(installedTab?"Uninstall application?":"Install application?",
     selectedMeta.name,installedTab?"Remove from microSD?":String("Version ")+selectedMeta.version,
     installedTab?"Remove":"Install","Cancel",confirmChoice);
   ctx.ui.softkeys("","Select","Cancel");return;
 }
 if(details){
   if(!verified){ctx.ui.message("Package rejected",feedback,"No files were installed");ctx.ui.softkeys("","","Back");return;}
   ctx.ui.message(installedTab?"Installed application":"Signature verified",
       selectedMeta.name,String("Version ")+selectedMeta.version+"  /  "+selectedMeta.type,
       !strcmp(selectedMeta.type,"web")?"Permission: network":"Permission: read bundled text");
   if(previewIconReady)ctx.ui.display().pushImage(103,170,32,32,previewPixels);
   else ctx.ui.drawIcon(101,167,"App",ctx.ui.c().bg);
   ctx.ui.softkeys("Options",installedTab?"Open":"Install","Back");
   drawPopup(ctx,popup,INSTALLER_OPTS,INSTALLER_OPT_COUNT);return;
 }
 if(!count){ctx.ui.message(tab,installedTab?"No installed applications":"No .qeapp files in inbox",installedTab?"Switch to Inbox with Options":"Download via Qeafbrowser");}
 else for(int row=0;row<SymbianUI::LIST_VISIBLE;row++)paintRow(ctx,offset+row,offset+row==index);
 ctx.ui.scrollbar(count,SymbianUI::LIST_VISIBLE,offset);
 if(feedback.length()){
   TFT_eSPI &d=ctx.ui.display();ThemeColors colors=ctx.ui.c();
   d.fillRect(3,283,230,14,colors.bg);d.setTextFont(1);d.setTextColor(colors.dim,colors.bg);
   d.setCursor(5,286);d.print(feedback.substring(0,32));
 }
 ctx.ui.softkeys("Options","Details","Back");
 drawPopup(ctx,popup,INSTALLER_OPTS,INSTALLER_OPT_COUNT);
}

ScreenId AppInstallerApp::handle(AppContext &ctx,const KeyEvent &e){
 if(!e.pressed||e.longPress)return ScreenId::AppInstaller;
 if(confirm){
   if(e.key==Key::Left||e.key==Key::Right){confirmChoice=1-confirmChoice;draw(ctx);return ScreenId::AppInstaller;}
   if(e.key==Key::A||e.key==Key::B){confirm=false;draw(ctx);return ScreenId::AppInstaller;}
   if(e.key==Key::Start||e.key==Key::Select){
     if(confirmChoice==0){
       String error;
       if(installedTab){
         bool ok=ctx.installer.uninstall(selectedMeta.id,error);
         feedback=ok?"Uninstalled successfully":error;
         if(ok)ctx.notifications.push("App uninstalled",selectedMeta.name);
       }else{
         Qeapp::Meta installed;
         bool ok=ctx.installer.install(selectedPath,installed,error);
         feedback=ok?"Installed successfully":error;
         if(ok)ctx.notifications.push("App installed",installed.name);
       }
       confirm=false;details=false;reload(ctx);
       // reload() resets feedback, so preserve operation result in the list footer.
       feedback=error.length()?error:(installedTab?"Uninstalled successfully":"Installed successfully");
     }else confirm=false;
     draw(ctx);
   }
   return ScreenId::AppInstaller;
 }
 if(popup.open){
   if(e.key==Key::Start||e.key==Key::Select){
     int choice=popup.index;popup.close();
     if(choice==0){selectedPath="";openDetails(ctx);}
     if(choice==1){if(!details){selectedPath="";openDetails(ctx);}if(verified){confirm=true;confirmChoice=1;}}
     if(choice==2){installedTab=!installedTab;selectedPath="";reload(ctx);}
     if(choice==3){selectedPath="";reload(ctx);}
     if(choice==4)return ScreenId::Applications;
     draw(ctx);return ScreenId::AppInstaller;
   }
   popupNav(popup,e,INSTALLER_OPT_COUNT);draw(ctx);return ScreenId::AppInstaller;
 }
 if(details){
   if(e.key==Key::A||e.key==Key::B){details=false;selectedPath="";draw(ctx);return ScreenId::AppInstaller;}
   if(e.key==Key::Option){popup.show();drawPopup(ctx,popup,INSTALLER_OPTS,INSTALLER_OPT_COUNT);return ScreenId::AppInstaller;}
   if(e.key==Key::Start||e.key==Key::Select){
     if(verified&&installedTab){ctx.pendingPackageId=selectedMeta.id;return ScreenId::PackageApp;}
     if(verified){confirm=true;confirmChoice=1;draw(ctx);}
     return ScreenId::AppInstaller;
   }
   return ScreenId::AppInstaller;
 }
 if(e.key==Key::A||e.key==Key::B)return returnTo;
 if(e.key==Key::Option){popup.show();drawPopup(ctx,popup,INSTALLER_OPTS,INSTALLER_OPT_COUNT);return ScreenId::AppInstaller;}
 if((e.key==Key::Up&&index>0)||(e.key==Key::Down&&index<count-1)){
   int old=index,oldOffset=offset;index+=e.key==Key::Up?-1:1;
   if(index<offset)--offset;if(index>=offset+SymbianUI::LIST_VISIBLE)++offset;
   if(offset!=oldOffset)draw(ctx);
   else{paintRow(ctx,old,false);paintRow(ctx,index,true);ctx.ui.scrollbar(count,SymbianUI::LIST_VISIBLE,offset);}
 }else if((e.key==Key::Start||e.key==Key::Select)&&count){selectedPath="";openDetails(ctx);draw(ctx);}
 return ScreenId::AppInstaller;
}

// ---------------- Applications ----------------
static const char *const appOptions[] = {
  "Open selected", "App installer", "Shell", "Tasks", "Text viewer", "Recovery",
  "Notifications", "Clock", "System info", "About"
};
static constexpr int APP_OPTIONS = sizeof(appOptions) / sizeof(appOptions[0]);
static constexpr int APP_COUNT = 14;
static ScreenId applicationDestination(int n) {
  static const ScreenId dst[APP_COUNT] = {
    ScreenId::Shell,ScreenId::TaskSwitcher,ScreenId::Notes,ScreenId::Notifications,
    ScreenId::TextViewer,ScreenId::Recovery,ScreenId::Clock,ScreenId::SystemInfo,
    ScreenId::About,ScreenId::Themes,ScreenId::AppInstaller,ScreenId::Files,
    ScreenId::Calculator,ScreenId::Stopwatch
  };
  return dst[constrain(n,0,APP_COUNT-1)];
}
static const char *const applicationTitle[APP_COUNT] = {
 "Shell","Open apps","Notes","Notifications","Text viewer","Recovery",
 "Clock","System info","About","Themes","App installer","App inbox","Calculator","Stopwatch"
};
static const char *const applicationSub[APP_COUNT] = {
 "System terminal","Recent apps and resume","Quick note","System events",
 "TXT/MD viewer","Safe Mode","Time","Memory and reset",
 "OS information","microSD themes","Install/uninstall QEAPP","Downloaded packages",
 "Portrait keypad arithmetic","Timer and 4 lap records"
};
static const char *const applicationIcon[APP_COUNT]={"Term","App","Note","Bell","Doc","Rec","Clk","Sys","i","Th","App","Dir","Calc","Clk"};

ScreenId ApplicationsApp::openSelected(AppContext &ctx){
 if(index<APP_COUNT){if(index==11)ctx.pendingFolderPath=StoragePaths::APPS_INBOX;return applicationDestination(index);}
 int n=index-APP_COUNT;
 if(n>=0&&n<ctx.installer.count()){
   ctx.pendingPackageId=ctx.installer.at(n).info.id;
   return ScreenId::PackageApp;
 }
 return ScreenId::Applications;
}

void ApplicationsApp::draw(AppContext &ctx){
 ctx.ui.chrome("Applications",statusWifi(),false,false,ctx.settings.data().hour12);
 int total=APP_COUNT+ctx.installer.count();
 for(int row=0;row<SymbianUI::LIST_VISIBLE;row++){
   int item=offset+row;
   if(item>=total){ctx.ui.clearListRow(row);continue;}
   if(item<APP_COUNT)ctx.ui.listItem(row,applicationIcon[item],applicationTitle[item],applicationSub[item],item==index);
   else{
     const auto &entry=ctx.installer.at(item-APP_COUNT);
     ctx.ui.listItem(row,"App",entry.info.name,String("v")+entry.info.version+" / "+entry.info.type,item==index);
     uint16_t pixels[1024];if(ctx.installer.loadIcon(entry.info.id,pixels))
       ctx.ui.display().pushImage(9,SymbianUI::CONTENT_TOP+1+row*SymbianUI::LIST_ROW_H+10,32,32,pixels);
   }
 }
 ctx.ui.scrollbar(total,SymbianUI::LIST_VISIBLE,offset);
 ctx.ui.softkeys("Options","Open","Back");drawPopup(ctx,popup,appOptions,APP_OPTIONS);
}

ScreenId ApplicationsApp::handle(AppContext &ctx,const KeyEvent &e){
 if(!e.pressed||e.longPress)return ScreenId::Applications;
 if(popup.open){
   if(e.key==Key::Start||e.key==Key::Select){int c=popup.index;popup.close();
     if(c==0)return openSelected(ctx);
     static const ScreenId quick[]={ScreenId::AppInstaller,ScreenId::Shell,ScreenId::TaskSwitcher,
       ScreenId::TextViewer,ScreenId::Recovery,ScreenId::Notifications,ScreenId::Clock,
       ScreenId::SystemInfo,ScreenId::About};
     if(c>0&&c<=9)return quick[c-1];
   }
   popupNav(popup,e,APP_OPTIONS);if(popup.open)drawPopup(ctx,popup,appOptions,APP_OPTIONS);else draw(ctx);
   return ScreenId::Applications;
 }
 if(e.key==Key::A||e.key==Key::B)return ScreenId::Launcher;
 int total=APP_COUNT+ctx.installer.count();
 if((e.key==Key::Up&&index>0)||(e.key==Key::Down&&index<total-1)){
   int old=index,prev=offset;index+=e.key==Key::Up?-1:1;
   if(index<offset)--offset;if(index>=offset+SymbianUI::LIST_VISIBLE)++offset;
   // Installed icons are loaded on demand only for the two changed rows.
   if(prev!=offset)draw(ctx);
   else {
     auto rp=[&](int item,bool sel){int row=item-offset;if(row<0||row>=SymbianUI::LIST_VISIBLE)return;
       if(item<APP_COUNT)ctx.ui.listItem(row,applicationIcon[item],applicationTitle[item],applicationSub[item],sel);
       else{const auto &entry=ctx.installer.at(item-APP_COUNT);
         ctx.ui.listItem(row,"App",entry.info.name,String("v")+entry.info.version+" / "+entry.info.type,sel);
         uint16_t pixels[1024];if(ctx.installer.loadIcon(entry.info.id,pixels))
           ctx.ui.display().pushImage(9,SymbianUI::CONTENT_TOP+1+row*SymbianUI::LIST_ROW_H+10,32,32,pixels);
       }};
     rp(old,false);rp(index,true);ctx.ui.scrollbar(total,SymbianUI::LIST_VISIBLE,offset);
   }
 }else if(e.key==Key::Start||e.key==Key::Select)return openSelected(ctx);
 else if(e.key==Key::Option){popup.show();drawPopup(ctx,popup,appOptions,APP_OPTIONS);}
 return ScreenId::Applications;
}

// ---------------- Quick panel ----------------
static bool wifiRadioEnabled() {
  return WiFi.getMode() != WIFI_OFF;
}

void QuickPanelApp::adjust(AppContext &ctx, int direction) {
  SystemSettings &s = ctx.settings.data();
  if (index == 0) {
    if (wifiRadioEnabled()) {
      ctx.wifiConnection.disconnect(); // manual radio-off cancels pending scan/join
      ctx.wifiConnection.configureAuto(false);
      WiFi.mode(WIFI_OFF);
      ctx.notifications.push("WiFi", "Radio switched off");
    } else {
      WiFi.mode(WIFI_STA);
      ctx.wifiConnection.configureAuto(s.wifiAuto && !ctx.system.safeMode());
      ctx.notifications.push("WiFi", "Radio switched on");
    }
  } else if (index == 1) {
    s.brightness = constrain((int)s.brightness + direction * 10, 20, 100);
    analogWrite(Board::TFT_LEDK_PIN, map(s.brightness, 0, 100, 0, 255));
    ctx.settings.save();
  } else if (index == 2) {
    s.volume = constrain((int)s.volume + direction * 5, 0, 100);
    ctx.music.setVolume(s.volume);
    ctx.settings.save();
  }
}

void QuickPanelApp::draw(AppContext &ctx) {
  ctx.ui.chrome("Quick panel", statusWifi(), gBleReady, ctx.storage.mounted(), ctx.settings.data().hour12);
  SystemSettings &s = ctx.settings.data();
  const char *labels[] = {"WiFi radio", "Backlight", "Audio volume", "Notifications", "Lock device"};
  const char *icons[] = {"Wi", "Br", "Mus", "Bell", "Lock"};
  for (int i = 0; i < 5; ++i) {
    String value;
    if (i == 0) value = wifiRadioEnabled() ? (statusWifi() ? "Connected" : "On") : "Off";
    else if (i == 1) value = String(s.brightness) + "%";
    else if (i == 2) value = String(s.volume) + "%";
    else if (i == 3) value = String(ctx.notifications.unreadCount()) + " unread";
    else value = "Lock keypad now";
    ctx.ui.listItem(i, icons[i], labels[i], value, i == index);
  }
  ctx.ui.softkeys("", index >= 3 ? "Open" : "Change", "Back");
}

ScreenId QuickPanelApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::QuickPanel;
  if (e.key == Key::A || e.key == Key::B || e.key == Key::Option) return ScreenId::Idle;
  if ((e.key == Key::Up && index > 0) || (e.key == Key::Down && index < 4)) {
    const int oldIndex = index;
    index += (e.key == Key::Up) ? -1 : 1;
    SystemSettings &s = ctx.settings.data();
    const char *labels[] = {"WiFi radio", "Backlight", "Audio volume", "Notifications", "Lock device"};
    const char *icons[] = {"Wi", "Br", "Mus", "Bell", "Lock"};
    auto paintRow = [&](int item, bool selected) {
      String value;
      if (item == 0) value = wifiRadioEnabled() ? (statusWifi() ? "Connected" : "On") : "Off";
      else if (item == 1) value = String(s.brightness) + "%";
      else if (item == 2) value = String(s.volume) + "%";
      else if (item == 3) value = String(ctx.notifications.unreadCount()) + " unread";
      else value = "Lock keypad now";
      ctx.ui.listItem(item, icons[item], labels[item], value, selected);
    };
    paintRow(oldIndex, false);
    paintRow(index, true);
    ctx.ui.softkeys("", index >= 3 ? "Open" : "Change", "Back");
  }
  else if (e.key == Key::Left) { adjust(ctx, -1); draw(ctx); }
  else if (e.key == Key::Right) { adjust(ctx, 1); draw(ctx); }
  else if (e.key == Key::Start || e.key == Key::Select) {
    if (index <= 2) { adjust(ctx, 1); draw(ctx); }
    else if (index == 3) return ScreenId::Notifications;
    else return ScreenId::Lock;
  }
  return ScreenId::QuickPanel;
}

// ---------------- Notification center ----------------
static const char *const notificationOptions[] = {"Open", "Mark all read", "Delete selected", "Clear all"};
static constexpr int NOTIFICATION_OPTIONS = sizeof(notificationOptions) / sizeof(notificationOptions[0]);

void NotificationCenterApp::enter(AppContext &ctx, ScreenId returnScreen) {
  popup.close();
  returnTo = returnScreen == ScreenId::Lock ? ScreenId::Idle : returnScreen;
  index = 0;
  offset = 0;
  if (ctx.notifications.count() > 0) ctx.notifications.markRead(0);
}

void NotificationCenterApp::draw(AppContext &ctx) {
  ctx.ui.chrome("Notifications", statusWifi(), gBleReady, ctx.storage.mounted(), ctx.settings.data().hour12);
  int count = ctx.notifications.count();
  if (!count) {
    ctx.ui.message("Notifications", "No notifications", "System events will appear here");
  } else {
    for (int row = 0; row < SymbianUI::LIST_VISIBLE; ++row) {
      int i = offset + row;
      if (i >= count) { ctx.ui.clearListRow(row); continue; }
      const SystemNotification &n = ctx.notifications.at(i);
      String sub = n.read ? n.body : String("NEW  ") + n.body;
      ctx.ui.listItem(row, "Bell", n.title, sub, i == index);
    }
    ctx.ui.scrollbar(count, SymbianUI::LIST_VISIBLE, offset);
  }
  ctx.ui.softkeys("Options", count ? "Open" : "", "Back");
  drawPopup(ctx, popup, notificationOptions, NOTIFICATION_OPTIONS);
}

ScreenId NotificationCenterApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::Notifications;
  int count = ctx.notifications.count();

  if (popup.open) {
    if (e.key == Key::Start || e.key == Key::Select) {
      int choice = popup.index;
      popup.close();
      if (choice == 0 && count) {
        ctx.notifications.markRead(index);
        const SystemNotification &n = ctx.notifications.at(index);
        ctx.ui.message(n.title, n.body, String("Event #") + String(index + 1));
        ctx.ui.softkeys("", "", "Back");
        return ScreenId::Notifications;
      }
      if (choice == 1) ctx.notifications.markAllRead();
      else if (choice == 2 && count) {
        ctx.notifications.remove(index);
        if (index >= ctx.notifications.count()) index = max(0, ctx.notifications.count() - 1);
        offset = min(offset, max(0, ctx.notifications.count() - SymbianUI::LIST_VISIBLE));
      } else if (choice == 3) {
        ctx.notifications.clear();
        index = offset = 0;
      }
      draw(ctx);
      return ScreenId::Notifications;
    }
    popupNav(popup, e, NOTIFICATION_OPTIONS);
    draw(ctx);
    return ScreenId::Notifications;
  }

  if (e.key == Key::A || e.key == Key::B) return returnTo;
  if (e.key == Key::Option) { popup.show(); draw(ctx); }
  else if ((e.key == Key::Up && index > 0) || (e.key == Key::Down && index < count - 1)) {
    const int oldIndex = index, oldOffset = offset;
    index += (e.key == Key::Up) ? -1 : 1;
    if (index < offset) --offset;
    if (index >= offset + SymbianUI::LIST_VISIBLE) ++offset;
    if (oldOffset != offset) {
      draw(ctx);
    } else {
      auto paintRow = [&](int item, bool selected) {
        const int row = item - offset;
        if (row < 0 || row >= SymbianUI::LIST_VISIBLE) return;
        const SystemNotification &n = ctx.notifications.at(item);
        String sub = n.read ? n.body : String("NEW  ") + n.body;
        ctx.ui.listItem(row, "Bell", n.title, sub, selected);
      };
      paintRow(oldIndex, false);
      paintRow(index, true);
      ctx.ui.scrollbar(count, SymbianUI::LIST_VISIBLE, offset);
    }
  } else if ((e.key == Key::Start || e.key == Key::Select) && count) {
    ctx.notifications.markRead(index);
    const SystemNotification &n = ctx.notifications.at(index);
    ctx.ui.message(n.title, n.body, "Marked as read");
    ctx.ui.softkeys("", "", "Back");
  }
  return ScreenId::Notifications;
}

// ---------------- Notes ----------------
static const char *const notesOptions[] = {"Edit note", "Clear note", "Notifications", "About"};
static constexpr int NOTES_OPTIONS = sizeof(notesOptions) / sizeof(notesOptions[0]);

static void drawNoteBody(AppContext &ctx, const String &note) {
  TFT_eSPI &d = ctx.ui.display();
  ThemeColors c = ctx.ui.c();
  d.fillRect(8, 43, Board::SCREEN_W - 16, 226, c.panel);
  d.drawRect(8, 43, Board::SCREEN_W - 16, 226, c.dim);
  d.setTextFont(1);
  d.setTextColor(c.text, c.panel);
  if (!note.length()) {
    d.setCursor(18, 65); d.print("No note yet.");
    d.setCursor(18, 83); d.print("Press START to create one.");
    return;
  }
  const int width = 31;
  int y = 58;
  for (int pos = 0; pos < (int)note.length() && y < 250; pos += width) {
    String line = note.substring(pos, min(pos + width, (int)note.length()));
    d.setCursor(15, y); d.print(line);
    y += 18;
  }
}

void NotesApp::draw(AppContext &ctx) {
  if (ctx.keyboard.active()) {
    ctx.keyboard.draw(ctx.ui, statusWifi(), gBleReady, ctx.storage.mounted(), ctx.settings.data().hour12);
    return;
  }
  ctx.ui.chrome("Notes", statusWifi(), gBleReady, ctx.storage.mounted(), ctx.settings.data().hour12);
  if (confirmClear) {
    ctx.ui.dialog("Clear note?", "Delete the saved quick note?", "This cannot be undone.", "Clear", "Cancel", confirmChoice);
    ctx.ui.softkeys("", "Select", "Cancel");
    return;
  }
  drawNoteBody(ctx, ctx.settings.note());
  ctx.ui.softkeys("Options", "Edit", "Back");
  drawPopup(ctx, popup, notesOptions, NOTES_OPTIONS);
}

ScreenId NotesApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (ctx.keyboard.active()) {
    if (ctx.keyboard.handle(e)) {
      if (!ctx.keyboard.active() && ctx.keyboard.accepted()) {
        ctx.settings.saveNote(ctx.keyboard.value());
        ctx.notifications.push("Notes", "Quick note saved");
      }
      draw(ctx);
    }
    return ScreenId::Notes;
  }
  if (!e.pressed || e.longPress) return ScreenId::Notes;

  if (confirmClear) {
    if (e.key == Key::Left || e.key == Key::Right) {
      confirmChoice = confirmChoice ? 0 : 1;
      draw(ctx);
    } else if (e.key == Key::Start || e.key == Key::Select) {
      if (confirmChoice == 0) {
        ctx.settings.saveNote("");
        ctx.notifications.push("Notes", "Quick note cleared");
      }
      confirmClear = false;
      draw(ctx);
    } else if (e.key == Key::A || e.key == Key::B) {
      confirmClear = false;
      draw(ctx);
    }
    return ScreenId::Notes;
  }

  if (popup.open) {
    if (e.key == Key::Start || e.key == Key::Select) {
      int choice = popup.index;
      popup.close();
      if (choice == 0) ctx.keyboard.open("Edit note", ctx.settings.note(), false);
      else if (choice == 1) { confirmClear = true; confirmChoice = 1; }
      else if (choice == 2) return ScreenId::Notifications;
      else if (choice == 3) return ScreenId::About;
      draw(ctx);
      return ScreenId::Notes;
    }
    popupNav(popup, e, NOTES_OPTIONS);
    draw(ctx);
    return ScreenId::Notes;
  }

  if (e.key == Key::A || e.key == Key::B) return ScreenId::Applications;
  if (e.key == Key::Start || e.key == Key::Select) {
    ctx.keyboard.open("Edit note", ctx.settings.note(), false);
    draw(ctx);
  } else if (e.key == Key::Option) {
    popup.show();
    draw(ctx);
  }
  return ScreenId::Notes;
}

// ---------------- Task switcher / Recent apps ----------------
static const char *const taskOptions[] = {"Open", "Remove recent", "Clear recent", "System info"};
static constexpr int TASK_OPTIONS = sizeof(taskOptions) / sizeof(taskOptions[0]);

void TaskSwitcherApp::enter(AppContext &ctx, ScreenId from) {
  popup.close();
  resumeRequested = false;
  returnTo = (from == ScreenId::TaskSwitcher || from == ScreenId::Splash || from == ScreenId::Lock)
               ? ScreenId::Idle : from;
  index = 0;
  offset = 0;
  if (ctx.system.recentCount() == 0 && SystemService::isTaskScreen(returnTo)) {
    ctx.system.recordScreen(returnTo);
  }
}

void TaskSwitcherApp::draw(AppContext &ctx) {
  ctx.ui.chrome("Open applications", statusWifi(), gBleReady, ctx.storage.mounted(), ctx.settings.data().hour12);
  const int count = ctx.system.recentCount();
  if (!count) {
    ctx.ui.message("Open applications", "No recent applications", "Open an app from Menu first");
  } else {
    for (int row = 0; row < SymbianUI::LIST_VISIBLE; ++row) {
      const int i = offset + row;
      if (i >= count) { ctx.ui.clearListRow(row); continue; }
      ScreenId recent = ctx.system.recentAt(i);
      String sub = (i == 0) ? "Most recently used" : "Suspended UI state";
      ctx.ui.listItem(row, SystemService::screenIcon(recent), SystemService::screenName(recent), sub, i == index);
    }
    ctx.ui.scrollbar(count, SymbianUI::LIST_VISIBLE, offset);
  }
  ctx.ui.softkeys("Options", count ? "Switch" : "", "Back");
  drawPopup(ctx, popup, taskOptions, TASK_OPTIONS);
}

ScreenId TaskSwitcherApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::TaskSwitcher;
  int count = ctx.system.recentCount();

  if (popup.open) {
    if (e.key == Key::Start || e.key == Key::Select) {
      int choice = popup.index;
      popup.close();
      if (choice == 0 && count) {
        resumeRequested = true;
        return ctx.system.recentAt(index);
      }
      if (choice == 1 && count) {
        ctx.system.removeRecent(index);
        count = ctx.system.recentCount();
        if (index >= count) index = max(0, count - 1);
        offset = min(offset, max(0, count - SymbianUI::LIST_VISIBLE));
      } else if (choice == 2) {
        ctx.system.clearRecent();
        index = offset = 0;
      } else if (choice == 3) {
        return ScreenId::SystemInfo;
      }
      draw(ctx);
      return ScreenId::TaskSwitcher;
    }
    popupNav(popup, e, TASK_OPTIONS);
    draw(ctx);
    return ScreenId::TaskSwitcher;
  }

  if (e.key == Key::A || e.key == Key::B) {
    resumeRequested = true;
    return returnTo;
  }
  if (e.key == Key::Option) { popup.show(); draw(ctx); }
  else if ((e.key == Key::Up && index > 0) || (e.key == Key::Down && index < count - 1)) {
    const int oldIndex = index, oldOffset = offset;
    index += (e.key == Key::Up) ? -1 : 1;
    if (index < offset) --offset;
    if (index >= offset + SymbianUI::LIST_VISIBLE) ++offset;
    if (oldOffset != offset) {
      draw(ctx);
    } else {
      auto paintRow = [&](int item, bool selected) {
        const int row = item - offset;
        if (row < 0 || row >= SymbianUI::LIST_VISIBLE) return;
        ScreenId recent = ctx.system.recentAt(item);
        String sub = (item == 0) ? "Most recently used" : "Suspended UI state";
        ctx.ui.listItem(row, SystemService::screenIcon(recent), SystemService::screenName(recent), sub, selected);
      };
      paintRow(oldIndex, false);
      paintRow(index, true);
      ctx.ui.scrollbar(count, SymbianUI::LIST_VISIBLE, offset);
    }
  } else if ((e.key == Key::Start || e.key == Key::Select) && count) {
    resumeRequested = true;
    return ctx.system.recentAt(index);
  }
  return ScreenId::TaskSwitcher;
}

// ---------------- Calculator (4x4 S60 keypad) ----------------
static const char *const calculatorOptions[]={"Decimal point","Backspace","Clear all"};
static constexpr int CALCULATOR_OPTIONS=3;
static const char calculatorKeys[4][4]={{'7','8','9','/'},{'4','5','6','*'},
                                        {'1','2','3','-'},{'C','0','=','+'}};
char CalculatorApp::button(uint8_t r,uint8_t c){return calculatorKeys[r%4][c%4];}
void CalculatorApp::paintDisplay(AppContext &ctx){
  auto &t=ctx.ui.display();auto c=ctx.ui.c();
  t.fillRect(8,42,224,48,c.panel);t.drawRect(8,42,224,48,c.dim);
  String line=engine.display();if(line.length()>19)line=line.substring(0,18)+"~";
  ctx.ui.textBold(15,56,line,2,engine.error()?c.accent:c.text,c.panel);
}
void CalculatorApp::paintButton(AppContext &ctx,uint8_t r,uint8_t colIndex,bool selected){
  auto &t=ctx.ui.display();auto c=ctx.ui.c();
  const int x=10+colIndex*56,y=99+r*43;
  const uint16_t fill=selected?c.selected:c.panel;
  t.fillRect(x,y,52,39,fill);t.drawRect(x,y,52,39,selected?c.accent:c.dim);
  const char ch=button(r,colIndex);
  const String label=String(ch);
  const int tw=ctx.ui.textWidth(label,2);
  ctx.ui.textBold(x+(52-tw)/2,y+10,label,2,selected?c.text:c.text,fill);
}
void CalculatorApp::draw(AppContext &ctx){
  ctx.ui.chrome("Calculator",statusWifi(),false,false,ctx.settings.data().hour12);
  paintDisplay(ctx);
  for(uint8_t r=0;r<4;++r)for(uint8_t c=0;c<4;++c)paintButton(ctx,r,c,r==row&&c==col);
  auto &t=ctx.ui.display();auto c=ctx.ui.c();
  t.fillRect(8,275,224,14,c.bg);t.setCursor(11,278);t.setTextFont(1);t.setTextColor(c.dim,c.bg);
  t.print("Options: .  /  backspace  /  clear");
  ctx.ui.softkeys("Options","Enter","Back");
  drawPopup(ctx,popup,calculatorOptions,CALCULATOR_OPTIONS);
}
ScreenId CalculatorApp::handle(AppContext &ctx,const KeyEvent &e){
  if(!e.pressed||e.longPress)return ScreenId::Calculator;
  if(popup.open){
    if(e.key==Key::Start){
      const int choice=popup.index;popup.close();
      engine.press(choice==0?'.':(choice==1?'<':'C'));
      draw(ctx);return ScreenId::Calculator;
    }
    popupNav(popup,e,CALCULATOR_OPTIONS);
    if(popup.open)drawPopup(ctx,popup,calculatorOptions,CALCULATOR_OPTIONS);
    else draw(ctx);
    return ScreenId::Calculator;
  }
  if(e.key==Key::A)return ScreenId::Applications;
  if(e.key==Key::Option){
    popup.show();drawPopup(ctx,popup,calculatorOptions,CALCULATOR_OPTIONS);
    return ScreenId::Calculator;
  }
  if(e.key==Key::Up||e.key==Key::Down||e.key==Key::Left||e.key==Key::Right){
    uint8_t oldR=row,oldC=col;
    if(e.key==Key::Up)row=(row+3)%4;
    else if(e.key==Key::Down)row=(row+1)%4;
    else if(e.key==Key::Left)col=(col+3)%4;
    else col=(col+1)%4;
    paintButton(ctx,oldR,oldC,false);paintButton(ctx,row,col,true);
  }else if(e.key==Key::Start){engine.press(button(row,col));paintDisplay(ctx);}
  return ScreenId::Calculator;
}

// ---------------- Stopwatch (wrap-safe millis, at most 4 laps) ----------------
void StopwatchApp::paintTime(AppContext &ctx){
  auto &t=ctx.ui.display();auto c=ctx.ui.c();
  t.fillRect(12,55,216,74,c.panel);
  const uint32_t cs=engine.elapsed(millis())/10;
  const uint32_t sec=cs/100;
  char buff[32];snprintf(buff,sizeof buff,"%02lu:%02lu.%02lu",
    (unsigned long)((sec/60)%100),(unsigned long)(sec%60),(unsigned long)(cs%100));
  ctx.ui.textBold(24,76,String(buff),2,c.text,c.panel);
}
void StopwatchApp::paintAction(AppContext &ctx,uint8_t idx,bool focused){
  auto &t=ctx.ui.display();auto c=ctx.ui.c();
  const char *label=idx==0?(engine.running()?"Pause":"Start"):(idx==1?"Lap":"Reset");
  const uint16_t fill=focused?c.selected:c.panel;
  const int x=8+idx*76;
  t.fillRect(x,140,70,33,fill);t.drawRect(x,140,70,33,focused?c.accent:c.dim);
  ctx.ui.textBold(x+5,151,label,1,c.text,fill);
}
void StopwatchApp::draw(AppContext &ctx){
  ctx.ui.chrome("Stopwatch",statusWifi(),false,false,ctx.settings.data().hour12);
  auto &t=ctx.ui.display();auto c=ctx.ui.c();
  t.fillRect(7,37,226,251,c.bg);
  paintTime(ctx);
  for(uint8_t n=0;n<3;++n)paintAction(ctx,n,selected==n);
  for(uint8_t i=0;i<engine.lapCount();++i){
    const uint32_t cs=engine.lapAt(i)/10,sec=cs/100;
    char buff[40];snprintf(buff,sizeof buff,"Lap %u  %02lu:%02lu.%02lu",unsigned(i+1),
      (unsigned long)((sec/60)%100),(unsigned long)(sec%60),(unsigned long)(cs%100));
    t.setTextColor(c.text,c.bg);t.setTextFont(1);t.setCursor(15,187+21*i);t.print(buff);
  }
  ctx.ui.softkeys("", "Select", "Back");
  lastPaint=millis();
}
void StopwatchApp::tick(AppContext &ctx,bool visible){
  if(!visible||!engine.running())return;
  const uint32_t now=millis();
  if((uint32_t)(now-lastPaint)>=100){lastPaint=now;paintTime(ctx);}
}
ScreenId StopwatchApp::handle(AppContext &ctx,const KeyEvent &e){
  if(!e.pressed||e.longPress)return ScreenId::Stopwatch;
  if(e.key==Key::A)return ScreenId::Applications;
  if(e.key==Key::Up||e.key==Key::Left){
    const uint8_t previous=selected;selected=(selected+2)%3;
    paintAction(ctx,previous,false);paintAction(ctx,selected,true);
  }
  else if(e.key==Key::Down||e.key==Key::Right){
    const uint8_t previous=selected;selected=(selected+1)%3;
    paintAction(ctx,previous,false);paintAction(ctx,selected,true);
  }
  else if(e.key==Key::Start){
    const uint32_t now=millis();
    if(selected==0)engine.toggle(now);
    else if(selected==1)engine.lap(now);
    else if(selected==2)engine.reset();
    draw(ctx);
  }
  return ScreenId::Stopwatch;
}
