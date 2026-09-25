// VQEAF OS 2.2: physical portrait Home + 3x4 Menu route, based on the user's
// own v2.0.1 grid implementation. The optional tabbed Explorer is separate.
#include "Apps.h"
#include "BoardConfig.h"
#include "../core/UiIconCatalog.h"

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
// ---------------- 3x4 MAIN MENU ----------------
// v1.1 S60 launcher + AMOLED Red theme: true portrait 240x320, 3 columns x 4 rows.
// The twelve destinations mirror the dense Nokia/S60 menu style while all
// icons remain procedural so the skin consumes no bitmap RAM.
static const char *const (&launcherIcon)[12] = UiIconCatalog::GRID_IDS;
static const char *launcherTitle[] = {"WiFi","Bluetooth","Music","File mgr","Gallery","Internet","Shell","Recovery","Settings","Themes","Apps","Library"};
static const char *launcherSub[]   = {
  "Scan and connect", "BLE device scanner", "WAV library / player",
  "Browse microSD", "JPEG/PNG/BMP viewer", "Qeafbrowser keypad web",
  "System shell", "Crash recovery / Safe Mode", "Appearance and system",
  "Scan and apply SD themes", "Tools and utilities", "Images, music, documents"
};
static const ScreenId launcherDst[] = {
  ScreenId::WiFi, ScreenId::BLE, ScreenId::Music,
  ScreenId::Files, ScreenId::Gallery, ScreenId::Browser,
  ScreenId::Shell, ScreenId::Recovery, ScreenId::Settings,
  ScreenId::Themes, ScreenId::Applications, ScreenId::Collection
};
static constexpr int LAUNCH_COUNT = 12;
static const char *const launcherOptions[] = {
  "Open selected", "WiFi", "Gallery", "Qeafbrowser", "Shell",
  "File manager", "Settings", "Themes", "Applications", "Recovery", "Retro Explorer"
};
static constexpr int LAUNCH_OPTIONS = sizeof(launcherOptions) / sizeof(launcherOptions[0]);

void LauncherApp::moveGrid(Key key) {
  constexpr int cols = SymbianUI::GRID_COLS;
  const int row = index / cols;
  const int col = index % cols;
  const int rowStart = row * cols;
  const int rowEnd = min(rowStart + cols - 1, LAUNCH_COUNT - 1);

  if (key == Key::Left) {
    index = (index > rowStart) ? index - 1 : rowEnd;
  } else if (key == Key::Right) {
    index = (index < rowEnd) ? index + 1 : rowStart;
  } else if (key == Key::Up) {
    if (index - cols >= 0) index -= cols;
    else {
      int candidate = col;
      while (candidate + cols < LAUNCH_COUNT) candidate += cols;
      index = candidate;
    }
  } else if (key == Key::Down) {
    if (index + cols < LAUNCH_COUNT) index += cols;
    else index = col < LAUNCH_COUNT ? col : 0;
  }
}

void LauncherApp::draw(AppContext &ctx) {
  ctx.ui.chrome("Menu", statusWifi(), false, false, ctx.settings.data().hour12);
  ctx.ui.menuBackground();
  for (int i = 0; i < LAUNCH_COUNT; ++i)
    ctx.ui.gridItem(i, launcherIcon[i], launcherTitle[i], i == index);
  ctx.ui.softkeys("Options", "Open", "Exit");
  drawPopup(ctx, popup, launcherOptions, LAUNCH_OPTIONS);
}

ScreenId LauncherApp::handle(AppContext &ctx, const KeyEvent &e) {
  if (!e.pressed || e.longPress) return ScreenId::Launcher;

  if (popup.open) {
    if (e.key == Key::Start || e.key == Key::Select) {
      int choice = popup.index;
      popup.close();
      if (choice == 0) return launcherDst[index];
      if (choice == 1) return ScreenId::WiFi;
      if (choice == 2) return ScreenId::Gallery;
      if (choice == 3) return ScreenId::Browser;
      if (choice == 4) return ScreenId::Shell;
      if (choice == 5) return ScreenId::Files;
      if (choice == 6) return ScreenId::Settings;
      if (choice == 7) return ScreenId::Themes;
      if (choice == 8) return ScreenId::Applications;
      if (choice == 9) return ScreenId::Recovery;
      if (choice == 10) return ScreenId::Explorer;
      draw(ctx);
      return ScreenId::Launcher;
    }
    popupNav(popup, e, LAUNCH_OPTIONS);
    if (popup.open) drawPopup(ctx, popup, launcherOptions, LAUNCH_OPTIONS);
    else draw(ctx);
    return ScreenId::Launcher;
  }

  if (e.key == Key::A || e.key == Key::B) return ScreenId::Idle;
  if (e.key == Key::Up || e.key == Key::Down || e.key == Key::Left || e.key == Key::Right) {
    int old = index;
    moveGrid(e.key);
    if (old != index) {
      ctx.ui.gridItem(old, launcherIcon[old], launcherTitle[old], false);
      ctx.ui.gridItem(index, launcherIcon[index], launcherTitle[index], true);
    }
  } else if (e.key == Key::Start || e.key == Key::Select) {
    return launcherDst[index];
  } else if (e.key == Key::Option) {
    popup.show();
    drawPopup(ctx, popup, launcherOptions, LAUNCH_OPTIONS);
  }
  return ScreenId::Launcher;
}

