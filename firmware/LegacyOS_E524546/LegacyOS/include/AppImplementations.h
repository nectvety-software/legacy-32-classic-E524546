#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <SPIFFS.h>
#include <vector>

// ─── Built-in App Implementations — declarations only ─────────
// Bodies are in src/AppImplementations.cpp to avoid ODR / code bloat.
// Included AFTER class LegacyOS is fully defined.

// ─── Settings App ─────────────────────────────────────────────
namespace AppSettings {
  void start(AppContext& ctx);
  void render(AppContext& ctx);
  void loop(AppContext& ctx);
  void spr_showSysInfo(AppContext& ctx);
}

// ─── File Manager App ──────────────────────────────────────
namespace AppFileManager {
  void start(AppContext& ctx);
  void render(AppContext& ctx);
  void loop(AppContext& ctx);
  void _loadDir();
}

// ─── WiFi Manager App ──────────────────────────────────────
namespace AppWiFi {
  void start(AppContext& ctx);
  void render(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── BLE App ───────────────────────────────────────────────
namespace AppBLE {
  void start(AppContext& ctx);
  void render(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── Calculator App ────────────────────────────────────────
namespace AppCalc {
  void start(AppContext& ctx);
  void render(AppContext& ctx);
  void pressBtn(int idx);
  void loop(AppContext& ctx);
}

// ─── Terminal App ──────────────────────────────────────────
namespace AppTerminal {
  void print(const String& s);
  void start(AppContext& ctx);
  void render(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── Lua IDE App ─────────────────────────────────────────────
namespace AppLuaIDE {
  void _scanFiles();
  void start(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── JS Runner App ────────────────────────────────────────
namespace AppJSRun {
  void _scanFiles();
  void start(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── Retro Emulator App ──────────────────────────────────
namespace AppRetroEmu {
  void start(AppContext& ctx);
  void renderROMList(AppContext& ctx);
  void renderEmulating(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── LoRa App ────────────────────────────────────────────────
namespace AppLoRa {
  void start(AppContext& ctx);
  void render(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── System Monitor App ─────────────────────────────────────
namespace AppSysMon {
  void _sample(AppContext& ctx);
  void _drawGraph(TFT_eSprite* spr, uint8_t* data, int ox, int oy, int w, int h,
                  uint16_t lineC, const char* label);
  void renderOverview(AppContext& ctx);
  void renderNetwork(AppContext& ctx);
  void renderTasks(AppContext& ctx);
  void renderMemory(AppContext& ctx);
  void start(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── Text Editor App ─────────────────────────────────────────
namespace AppTextEditor {
  std::vector<String> _getLines();
  void renderEditor(AppContext& ctx);
  void renderKeyboard(AppContext& ctx);
  void handleKeyboardInput(AppContext& ctx);
  void handleEditorInput(AppContext& ctx);
  void start(AppContext& ctx);
  void loop(AppContext& ctx);
}

// I2S pins (optional - connect I2S DAC like MAX98357A)
#define I2S_BCLK   -1
#define I2S_LRC    -1
#define I2S_DOUT   -1

struct Track {
  String name;
  String path;
  size_t size;
  uint32_t duration;
};

// ─── Music Player App ─────────────────────────────────────
namespace AppMusicPlayer {
  void _scanMusic();
  void _updateVisualizer();
  String _formatTime(uint32_t sec);
  void render(AppContext& ctx);
  void start(AppContext& ctx);
  void loop(AppContext& ctx);
}

// ─── Snake Game App ─────────────────────────────────────────
namespace AppSnake {
  #define GRID_W   20
  #define GRID_H   20
  #define CELL_SZ  12
  #define GRID_OX  20
  #define GRID_OY  20

  struct Point { int x, y; };
  
  void _placeFood();
  void _reset();
  bool _moveSnake();
  void render(AppContext& ctx);
  void start(AppContext& ctx);
  void loop(AppContext& ctx);
}

/* AppManager::_registerBuiltinApps() is declared in AppManager.h
   and defined in src/AppImplementations.cpp */
