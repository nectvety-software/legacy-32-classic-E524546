#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "HardwareConfig.h"

// ═══════════════════════════════════════════════════════════
//  RetroEmu - Retro Emulation App for LegacyOS
//  Supports: GB/GBC, NES, GBA (via PSRAM ROM buffer)
//  Full emulators require specific libraries:
//    - GB:  gnuboy-esp32 or similar
//    - NES: nofrendo-esp32
//    - GBA: Lameboy (limited)
// ═══════════════════════════════════════════════════════════

enum class EmuType { NONE, GAMEBOY, NES, GBA, SMS };

struct RomInfo {
  String   name;
  String   path;
  EmuType  type;
  size_t   size;
};

class RetroEmu {
public:
  EmuType   activeEmu = EmuType::NONE;
  bool      running   = false;
  RomInfo   currentROM;
  
  // ROM buffer in PSRAM (up to 4MB for GBA)
  uint8_t*  romBuffer = nullptr;
  size_t    romSize   = 0;
  
  // Framebuffer for emulation output
  uint16_t* framebuf  = nullptr;
  int       fbWidth   = 160;
  int       fbHeight  = 144;
  
  void init() {
    // Allocate PSRAM for ROM
    romBuffer = (uint8_t*)ps_malloc(4 * 1024 * 1024); // 4MB
    if (romBuffer) {
      Serial.println(F("[Emu] 4MB PSRAM ROM buffer allocated"));
    } else {
      romBuffer = (uint8_t*)malloc(512 * 1024);
      Serial.println(F("[Emu] 512KB heap ROM buffer (PSRAM unavailable)"));
    }
    
    // Allocate framebuffer
    framebuf = (uint16_t*)ps_malloc(160 * 144 * 2); // GB size
    Serial.println(F("[RetroEmu] Emulation framework ready"));
  }
  
  // Scan for ROMs on SD
  std::vector<RomInfo> scanROMs() {
    std::vector<RomInfo> roms;
    
    const struct { const char* path; const char* ext; EmuType type; } dirs[] = {
      {"/roms/gb",  ".gb",  EmuType::GAMEBOY},
      {"/roms/gb",  ".gbc", EmuType::GAMEBOY},
      {"/roms/nes", ".nes", EmuType::NES},
      {"/roms/gba", ".gba", EmuType::GBA},
      {nullptr, nullptr, EmuType::NONE}
    };
    
    for (int d = 0; dirs[d].path; d++) {
      if (!SD.exists(dirs[d].path)) continue;
      File dir = SD.open(dirs[d].path);
      if (!dir) continue;
      File f;
      while ((f = dir.openNextFile())) {
        String name = f.name();
        if (name.endsWith(dirs[d].ext)) {
          RomInfo ri;
          ri.name = name.substring(0, name.lastIndexOf('.'));
          ri.path = String(dirs[d].path) + "/" + name;
          ri.type = dirs[d].type;
          ri.size = f.size();
          roms.push_back(ri);
        }
        f.close();
      }
      dir.close();
    }
    return roms;
  }
  
  bool loadROM(const RomInfo& rom) {
    if (!romBuffer) return false;
    File f = SD.open(rom.path.c_str());
    if (!f) return false;
    
    romSize = f.read(romBuffer, 4 * 1024 * 1024);
    f.close();
    currentROM = rom;
    activeEmu  = rom.type;
    
    Serial.printf("[Emu] ROM loaded: %s (%d bytes)\n", rom.name.c_str(), romSize);
    return romSize > 0;
  }
  
  // Start emulation
  bool startEmulation(TFT_eSPI* tft) {
    if (!romBuffer || romSize == 0) return false;
    running = true;
    
    switch (activeEmu) {
      case EmuType::GAMEBOY: return _startGB(tft);
      case EmuType::NES:     return _startNES(tft);
      case EmuType::GBA:     return _startGBA(tft);
      default: return false;
    }
  }
  
  void stop() {
    running = false;
    activeEmu = EmuType::NONE;
  }
  
  // Emulation loop tick (called from app loop)
  bool tick(TFT_eSPI* tft, uint8_t buttons) {
    if (!running) return false;
    switch (activeEmu) {
      case EmuType::GAMEBOY: return _tickGB(tft, buttons);
      case EmuType::NES:     return _tickNES(tft, buttons);
      case EmuType::GBA:     return _tickGBA(tft, buttons);
      default: return false;
    }
  }
  
  // Convert LegacyOS buttons to emulator buttons
  uint8_t getButtonState(InputManager* input) {
    uint8_t b = 0;
    if (input->right())  b |= 0x01;
    if (input->left())   b |= 0x02;
    if (input->up())     b |= 0x04;
    if (input->down())   b |= 0x08;
    if (input->a())      b |= 0x10;  // A button
    if (input->b())      b |= 0x20;  // B button
    if (input->select()) b |= 0x40;  // Select
    if (input->start())  b |= 0x80;  // Start
    return b;
  }
  
  // Scale and blit GB framebuffer (160x144) to 240x216
  void blitFramebuffer(TFT_eSPI* tft, int offsetX = 40, int offsetY = 52) {
    if (!framebuf) return;
    // 1.5x scale: 160->240, 144->216
    for (int y = 0; y < fbHeight; y++) {
      for (int x = 0; x < fbWidth; x++) {
        uint16_t c = framebuf[y * fbWidth + x];
        tft->drawPixel(offsetX + x * 3/2, offsetY + y * 3/2, c);
      }
    }
  }
  
  static const char* emuName(EmuType t) {
    switch(t) {
      case EmuType::GAMEBOY: return "Game Boy";
      case EmuType::NES:     return "NES";
      case EmuType::GBA:     return "GBA";
      case EmuType::SMS:     return "Master System";
      default:               return "Unknown";
    }
  }

private:
  bool _startGB(TFT_eSPI* tft) {
    // Stub: show "connect gnuboy-esp32 library"
    Serial.println(F("[Emu] GB: Install gnuboy-esp32 for full emulation"));
    return true;
  }
  
  bool _startNES(TFT_eSPI* tft) {
    Serial.println(F("[Emu] NES: Install nofrendo-esp32 for full emulation"));
    return true;
  }
  
  bool _startGBA(TFT_eSPI* tft) {
    Serial.println(F("[Emu] GBA: Requires custom GBA emulator port"));
    return true;
  }
  
  bool _tickGB(TFT_eSPI* tft, uint8_t btns) {
    // Demo: show ROM header info
    return true;
  }
  
  bool _tickNES(TFT_eSPI* tft, uint8_t btns) { return true; }
  bool _tickGBA(TFT_eSPI* tft, uint8_t btns) { return true; }
};

