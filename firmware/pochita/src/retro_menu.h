#ifndef RETRO_MENU_H
#define RETRO_MENU_H

#include "component/Config.h"
#include "component/ButtonManager.h"
#include "component/ui_utils.h"  // Display + SymbianUI + UILayout
#include "nes_emulator.h"
#include <dirent.h>
#include <sys/stat.h>

extern TFT_eSPI tft;
extern SystemMode currentMode;
extern void drawLauncherContent();

// A retro game collection browser: scans the SD card recursively for console
// ROMs (.nes/.snes/.sfc/.gb/.gbc/.gba/.sms/.md/.gen/.gg) and lists every find
// as a playable entry. NES runs through nofrendo; the other systems show the
// retro-go "Emulator Not Found" screen until their cores are installed.
namespace RetroMenu {
enum Kind {
  RK_NES, RK_SNES, RK_GB, RK_GBC, RK_GBA,
  RK_SMS, RK_MD, RK_GG, RK_UNKNOWN
};
struct Game {
  String name;
  String path;  // SD-relative path
  Kind kind;
};
constexpr int MAX_GAMES = 200;
Game games[MAX_GAMES];
int count = 0;
int sel = 0;
int scroll = 0;
static bool sScanned = false;

const char *tagFor(Kind k) {
  switch (k) {
    case RK_NES: return "NES";
    case RK_SNES: return "SNES";
    case RK_GB: return "GB";
    case RK_GBC: return "GBC";
    case RK_GBA: return "GBA";
    case RK_SMS: return "SMS";
    case RK_MD: return "MD";
    case RK_GG: return "GG";
    default: return "?";
  }
}

static Kind kindForName(const String &lower) {
  if (lower.endsWith(".nes")) return RK_NES;
  if (lower.endsWith(".snes") || lower.endsWith(".sfc")) return RK_SNES;
  if (lower.endsWith(".gb")) return RK_GB;
  if (lower.endsWith(".gbc")) return RK_GBC;
  if (lower.endsWith(".gba")) return RK_GBA;
  if (lower.endsWith(".sms")) return RK_SMS;
  if (lower.endsWith(".md") || lower.endsWith(".gen")) return RK_MD;
  if (lower.endsWith(".gg")) return RK_GG;
  return RK_UNKNOWN;
}

static void gameAdd(const String &path, Kind kind) {
  if (count >= MAX_GAMES) return;
  for (int i = 0; i < count; i++)
    if (games[i].path == path) return;
  String base = path;
  int slash = base.lastIndexOf('/');
  if (slash >= 0) base = base.substring(slash + 1);
  int dot = base.lastIndexOf('.');
  if (dot > 0) base = base.substring(0, dot);
  games[count].name = base;
  games[count].path = path;
  games[count].kind = kind;
  count++;
}

// Recursively walk the POSIX VFS mount ("/sd") collecting ROMs. Full VFS
// paths are converted back to SD-relative ones (strip the "/sd" prefix).
static void gameScanVfs(const String &vfsPath, int depth) {
  if (depth > 5) return;
  DIR *dir = opendir(vfsPath.c_str());
  if (!dir) return;
  struct dirent *ent;
  while ((ent = readdir(dir)) != nullptr) {
    String name = ent->d_name;
    if (name.isEmpty() || name == "." || name == "..") continue;
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.isEmpty() || name.startsWith(".")) continue;

    String full = vfsPath + "/" + name;
    bool isDir = (ent->d_type == DT_DIR);
    struct stat st;
    if (stat(full.c_str(), &st) == 0) isDir = S_ISDIR(st.st_mode);
    if (isDir) {
      if (name == "system" || name == "data" || name == "assets") continue;
      gameScanVfs(full, depth + 1);
      continue;
    }

    String lower = name;
    lower.toLowerCase();
    Kind kind = kindForName(lower);
    if (kind != RK_UNKNOWN) gameAdd(full.substring(3), kind);
  }
  closedir(dir);
}

static void retroScanGames() {
  count = 0;
  sScanned = true;
  tft.fillScreen(SymbianUI::BG);
  SymbianUI::drawTitle("Retro");
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Scanning SD for games...", UiLayout::CENTER_X, 140, 2);
  gameScanVfs("/sd", 0);
  if (sel >= count) sel = 0;
  scroll = 0;
}

static void drawGameRow(int idx, bool isSel) {
  const int rowH = SymbianUI::LIST_ROW_H;
  const int top = UiLayout::CONTENT_Y;
  const int visible = (UiLayout::FOOTER_Y - top) / rowH;
  int row = idx - scroll;
  if (row < 0 || row >= visible || idx >= count) return;
  SymbianUI::drawListRow(top + row * rowH, rowH, games[idx].name, isSel,
                         tagFor(games[idx].kind));
}

static void drawGameRows() {
  const int rowH = SymbianUI::LIST_ROW_H;
  const int top = UiLayout::CONTENT_Y;
  const int visible = (UiLayout::FOOTER_Y - top) / rowH;
  for (int i = 0; i < visible && (scroll + i) < count; i++)
    drawGameRow(scroll + i, (scroll + i) == sel);
}

inline void drawRetroMenu() {
  if (!sScanned) retroScanGames();
  SymbianUI::drawChrome("Retro", "Play", "Exit");
  if (sel < scroll) scroll = sel;
  const int visible =
      (UiLayout::FOOTER_Y - UiLayout::CONTENT_Y) / SymbianUI::LIST_ROW_H;
  if (sel >= scroll + visible) scroll = sel - visible + 1;
  drawGameRows();
  if (count == 0) {
    tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No ROMs found on SD", UiLayout::CENTER_X, 150, 2);
    tft.drawString("Add .nes / .gb / .gba ... files", UiLayout::CENTER_X, 174, 1);
  }
}

static void showCoreMissing(const char *tag, const String &name) {
  tft.fillScreen(COLOR_BLACK);
  tft.fillRect(0, 0, 240, 20, 0x8410);
  tft.drawLine(0, 19, 240, 19, 0x4208);
  tft.setTextColor(0x0000);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("RETRO", 5, 10, 1);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN);
  tft.drawString(tag, UiLayout::CENTER_X, 50, 2);

  tft.setTextColor(TFT_WHITE);
  tft.drawString(UiLayout::ellipsize(name, 28), UiLayout::CENTER_X, 75, 1);

  tft.setTextColor(TFT_RED);
  tft.drawString("Emulator Not Found", UiLayout::CENTER_X, 115, 2);

  tft.setTextColor(TFT_SILVER);
  tft.drawString("Install Retro-Go to play", UiLayout::CENTER_X, 145, 1);
  tft.drawString("this game", UiLayout::CENTER_X, 160, 1);

  tft.setTextColor(0x2145);
  tft.drawString("Press Back to return", UiLayout::CENTER_X, 200, 1);

  unsigned long start = millis();
  while (millis() - start < 3000) {
    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_A)) break;
    delay(10);
  }
}

static void retroLaunch(int idx) {
  const Game &g = games[idx];
  const char *tag = tagFor(g.kind);
  tft.fillScreen(SymbianUI::BG);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String("[ ") + tag + " ]", UiLayout::CENTER_X, 40, 2);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(UiLayout::ellipsize(g.name, 22), UiLayout::CENTER_X, 66, 2);
  tft.setTextColor(SymbianUI::DIM);
  tft.drawString("Starting...", UiLayout::CENTER_X, 96, 1);
  delay(300);

  if (g.kind == RK_NES) {
    runNesEmulator(g.path);
  } else {
    showCoreMissing(tag, g.name);
  }
}

inline void loopRetroMenu() {
  if (count == 0) {
    if (buttonManager.isJustPressed(KEY_A)) {
      currentMode = MODE_LAUNCHER;
      drawLauncherContent();
    }
    return;
  }

  int oldSel = sel;
  int oldScroll = scroll;
  if (buttonManager.isJustPressed(KEY_UP))
    sel = (sel - 1 + count) % count;
  if (buttonManager.isJustPressed(KEY_DOWN))
    sel = (sel + 1) % count;
  if (sel != oldSel) {
    const int visible =
        (UiLayout::FOOTER_Y - UiLayout::CONTENT_Y) / SymbianUI::LIST_ROW_H;
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + visible) scroll = sel - visible + 1;
    if (scroll != oldScroll) {
      drawGameRows();
    } else {
      drawGameRow(oldSel, false);
      drawGameRow(sel, true);
    }
  }
  if (buttonManager.isJustPressed(KEY_START)) {
    retroLaunch(sel);
    drawRetroMenu();
    return;
  }
  if (buttonManager.isJustPressed(KEY_A)) {
    currentMode = MODE_LAUNCHER;
    drawLauncherContent();
  }
}
}  // namespace RetroMenu

using namespace RetroMenu;

#endif