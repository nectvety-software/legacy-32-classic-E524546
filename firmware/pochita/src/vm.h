#ifndef VM_H
#define VM_H

#include "component/Config.h"
#include "component/ButtonManager.h"
#include "component/ui_utils.h"
#include "component/FileManager.h"
#include "app_registry.h"
#include "lua_interpreter.h"
#include "nes_emulator.h"
#include "ai_chat.h"
#include <dirent.h>
#include <sys/stat.h>

extern TFT_eSPI tft;
extern SystemMode currentMode;
extern void drawLauncherContent();

// A Nokia-S40-style "Virtual Machine" app drawer. It lists every app the user
// has installed through Files > "Add to launcher" (console ROMs, Lua scripts,
// PLE/VN AI models) and starts each one through its matching runtime,
// returning to this menu when the app exits. No SD rescan is needed: the
// installed list is the launcher registry itself.
namespace Vm {
enum Kind {
  VMK_NES, VMK_SNES, VMK_GB, VMK_GBC, VMK_GBA,
  VMK_SMS, VMK_MD, VMK_GG, VMK_LUA, VMK_AI, VMK_UNKNOWN
};
struct App {
  String name;
  String path;  // SD-relative path
  Kind kind;
};
constexpr int MAX_APPS = 128;
App apps[MAX_APPS];
int count = 0;
int sel = 0;
int scroll = 0;

struct TagInfo {
  const char *tag;
  bool runnable;
};
const TagInfo kTags[] = {
    {"NES", true}, {"SNES", false}, {"GB", false},  {"GBC", false},
    {"GBA", false}, {"SMS", false}, {"MD", false},  {"GG", false},
    {"LUA", true}, {"AI", true},    {"?", false},
};
}  // namespace Vm

static Vm::Kind vmKindForName(const String &lower) {
  if (lower.endsWith(".nes")) return Vm::VMK_NES;
  if (lower.endsWith(".snes") || lower.endsWith(".sfc")) return Vm::VMK_SNES;
  if (lower.endsWith(".gb")) return Vm::VMK_GB;
  if (lower.endsWith(".gbc")) return Vm::VMK_GBC;
  if (lower.endsWith(".gba")) return Vm::VMK_GBA;
  if (lower.endsWith(".sms")) return Vm::VMK_SMS;
  if (lower.endsWith(".md") || lower.endsWith(".gen")) return Vm::VMK_MD;
  if (lower.endsWith(".gg")) return Vm::VMK_GG;
  if (lower.endsWith(".lua")) return Vm::VMK_LUA;
  return Vm::VMK_UNKNOWN;
}

static void vmAddApp(const String &path, Vm::Kind kind, const String &label) {
  if (Vm::count >= Vm::MAX_APPS) return;
  for (int i = 0; i < Vm::count; i++)
    if (Vm::apps[i].path == path) return;
  String base = label;
  if (base.length() == 0) {
    base = path;
    int slash = base.lastIndexOf('/');
    if (slash >= 0) base = base.substring(slash + 1);
    int dot = base.lastIndexOf('.');
    if (dot > 0) base = base.substring(0, dot);
  }
  Vm::apps[Vm::count].name = base;
  Vm::apps[Vm::count].path = path;
  Vm::apps[Vm::count].kind = kind;
  Vm::count++;
}

static Vm::Kind vmKindForPath(const String &path) {
  String lower = path;
  lower.toLowerCase();
  Vm::Kind kind = vmKindForName(lower);
  if (kind != Vm::VMK_UNKNOWN) return kind;
  if (lower.endsWith(".bin")) {
    File mf = SD.open(path, FILE_READ);
    uint8_t h[4] = {0, 0, 0, 0};
    if (mf && mf.read(h, 4) == 4) {
      uint32_t magic;
      memcpy(&magic, h, 4);
      if (magic == AI_MAGIC_PLE || magic == AI_MAGIC_VN)
        kind = Vm::VMK_AI;
      else if (h[0] == 'N' && h[1] == 'E' && h[2] == 'S' && h[3] == 0x1A)
        kind = Vm::VMK_NES;
    }
    if (mf) mf.close();
  }
  return kind;
}

// Builds the drawer from the launcher's installed-app registry (Files >
// "Add to launcher"). The SD card is never rescanned here.
static void vmScanApps() {
  Vm::count = 0;
  for (size_t i = 0; i < appList.size(); i++) {
    if (appList[i].type != APP_INSTALLED) continue;
    Vm::Kind kind = vmKindForPath(appList[i].filePath);
    vmAddApp(appList[i].filePath, kind, appList[i].name);
    if (Vm::count >= Vm::MAX_APPS) break;
  }
  if (Vm::sel >= Vm::count) Vm::sel = 0;
  Vm::scroll = 0;
}

static void vmDrawRow(int idx, bool sel) {
  const int rowH = SymbianUI::LIST_ROW_H;
  const int top = UiLayout::CONTENT_Y;
  const int visible = (UiLayout::FOOTER_Y - top) / rowH;
  int row = idx - Vm::scroll;
  if (row < 0 || row >= visible || idx >= Vm::count) return;
  const char *tag = Vm::kTags[Vm::apps[idx].kind].tag;
  SymbianUI::drawListRow(top + row * rowH, rowH, Vm::apps[idx].name, sel, tag);
}

static void vmDrawRows() {
  const int rowH = SymbianUI::LIST_ROW_H;
  const int top = UiLayout::CONTENT_Y;
  const int visible = (UiLayout::FOOTER_Y - top) / rowH;
  for (int i = 0; i < visible && (Vm::scroll + i) < Vm::count; i++)
    vmDrawRow(Vm::scroll + i, (Vm::scroll + i) == Vm::sel);
}

static void drawVmMenu() {
  vmScanApps();  // rebuild from the installed-app registry every time
  SymbianUI::drawChrome("Virtual Machine", "Run", "Exit");
  if (Vm::sel < Vm::scroll) Vm::scroll = Vm::sel;
  const int visible = (UiLayout::FOOTER_Y - UiLayout::CONTENT_Y) / SymbianUI::LIST_ROW_H;
  if (Vm::sel >= Vm::scroll + visible) Vm::scroll = Vm::sel - visible + 1;
  vmDrawRows();
  if (Vm::count == 0) {
    tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No installed apps", UiLayout::CENTER_X, 150, 2);
    tft.drawString("Files > Add to launcher", UiLayout::CENTER_X, 174, 1);
  }
}

// Replicates the FileManager Lua launcher: a graphics script keeps its final
// frame with a footer hint, an output script shows the captured [OUTPUT].
static void vmRunLua(const String &path, const String &name) {
  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("[ LUA ]", UiLayout::CENTER_X, 40, 2);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(UiLayout::ellipsize(name, 24), UiLayout::CENTER_X, 65, 2);
  tft.setTextColor(TFT_SILVER);
  tft.drawString("Running script...", UiLayout::CENTER_X, 100, 1);
  delay(300);

  tft.fillScreen(COLOR_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  String result = lua.runScript(path);

  if (result.length() == 0) {
    tft.fillRect(0, 296, UiLayout::WIDTH, 24, TFT_BLACK);
    tft.setTextColor(THEME_COLOR);
    tft.drawString("Press Back to exit", UiLayout::CENTER_X, 308, 1);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A)) break;
      delay(10);
    }
  } else {
    tft.fillScreen(COLOR_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("[ OUTPUT ]", UiLayout::CENTER_X, 20, 2);
    int y = 50;
    tft.setTextColor(TFT_GREEN);
    tft.setTextDatum(ML_DATUM);
    int start = 0;
    for (int i = 0; i <= result.length(); i++) {
      if (i == result.length() || result[i] == '\n') {
        String line = result.substring(start, i);
        if (line.length() > 0 && y < 230) {
          tft.drawString(line, 10, y, 2);
          y += 20;
        }
        start = i + 1;
      }
    }
    tft.setTextColor(THEME_COLOR);
    tft.drawString("Press Back to exit", UiLayout::CENTER_X, 210, 1);
    while (true) {
      buttonManager.update();
      if (buttonManager.isJustPressed(KEY_A)) break;
      delay(10);
    }
  }
}

// Console ROMs without a compiled core show the retro-go "not found" screen
// and return to the VM menu.
static void vmShowCoreMissing(Vm::Kind kind, const String &name) {
  tft.fillScreen(COLOR_BLACK);
  tft.fillRect(0, 0, 240, 20, 0x8410);
  tft.drawLine(0, 19, 240, 19, 0x4208);
  tft.setTextColor(0x0000);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("VM", 5, 10, 1);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN);
  tft.drawString(Vm::kTags[kind].tag, UiLayout::CENTER_X, 50, 2);

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

static void vmLaunch(int idx) {
  const Vm::App &app = Vm::apps[idx];
  const char *tag = Vm::kTags[app.kind].tag;
  tft.fillScreen(SymbianUI::BG);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String("[ ") + tag + " ]", UiLayout::CENTER_X, 40, 2);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString(UiLayout::ellipsize(app.name, 22), UiLayout::CENTER_X, 66, 2);
  tft.setTextColor(SymbianUI::DIM);
  tft.drawString("Starting...", UiLayout::CENTER_X, 96, 1);
  delay(300);

  switch (app.kind) {
    case Vm::VMK_NES: runNesEmulator(app.path); break;
    case Vm::VMK_LUA: vmRunLua(app.path, app.name); break;
    case Vm::VMK_AI: runAiChat(app.path); break;
    default: vmShowCoreMissing(app.kind, app.name); break;
  }

  // Return to the VM menu after the app exits (unless a /run launch happened).
  if (!aiTookLaunch()) drawVmMenu();
}

static void loopVm() {
  if (Vm::count == 0) {
    if (buttonManager.isJustPressed(KEY_A)) {
      currentMode = MODE_LAUNCHER;
      drawLauncherContent();
    }
    return;
  }

  int oldSel = Vm::sel;
  int oldScroll = Vm::scroll;
  if (buttonManager.isJustPressed(KEY_UP))
    Vm::sel = (Vm::sel - 1 + Vm::count) % Vm::count;
  if (buttonManager.isJustPressed(KEY_DOWN))
    Vm::sel = (Vm::sel + 1) % Vm::count;

  if (Vm::sel != oldSel) {
    const int visible = (UiLayout::FOOTER_Y - UiLayout::CONTENT_Y) / SymbianUI::LIST_ROW_H;
    if (Vm::sel < Vm::scroll) Vm::scroll = Vm::sel;
    if (Vm::sel >= Vm::scroll + visible) Vm::scroll = Vm::sel - visible + 1;
    if (Vm::scroll != oldScroll) {
      vmDrawRows();
    } else {
      vmDrawRow(oldSel, false);
      vmDrawRow(Vm::sel, true);
    }
  }

  if (buttonManager.isJustPressed(KEY_START)) {
    vmLaunch(Vm::sel);
    return;
  }
  if (buttonManager.isJustPressed(KEY_A)) {
    currentMode = MODE_LAUNCHER;
    drawLauncherContent();
  }
}

#endif