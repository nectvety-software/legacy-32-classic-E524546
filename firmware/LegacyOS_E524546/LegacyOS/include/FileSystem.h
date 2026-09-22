#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#include <SPI.h>
#include "HardwareConfig.h"

// ═══════════════════════════════════════════════════════════
//  FileSystem - SD Card + SPIFFS unified filesystem
// ═══════════════════════════════════════════════════════════

class FileSystem {
public:
  bool sdMounted    = false;
  bool spiffsMounted = false;
  
  uint64_t sdTotalBytes  = 0;
  uint64_t sdUsedBytes   = 0;
  uint64_t spiffsTotal   = 0;
  uint64_t spiffsUsed    = 0;
  
  void init() {
    // Init SPIFFS
    if (SPIFFS.begin(true)) {
      spiffsMounted = true;
      spiffsTotal   = SPIFFS.totalBytes();
      spiffsUsed    = SPIFFS.usedBytes();
      Serial.printf("[FS] SPIFFS: %d/%d KB\n", spiffsUsed/1024, spiffsTotal/1024);
    }
    
    // Init SD card on custom SPI
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    if (SD.begin(SD_CS)) {
      sdMounted     = true;
      sdTotalBytes  = SD.totalBytes();
      sdUsedBytes   = SD.usedBytes();
      Serial.printf("[FS] SD Card: %llu/%llu MB\n",
        sdUsedBytes/1048576, sdTotalBytes/1048576);
      _createDirectories();
    } else {
      Serial.println(F("[FS] SD Card not found"));
    }
  }
  
  // ─── File operations ──────────────────────────────────────
  String readFile(const String& path) {
    File f = _open(path, "r");
    if (!f) return "";
    String content = f.readString();
    f.close();
    return content;
  }
  
  bool writeFile(const String& path, const String& content) {
    File f = _open(path, "w");
    if (!f) return false;
    f.print(content);
    f.close();
    return true;
  }
  
  bool appendFile(const String& path, const String& line) {
    File f = _open(path, "a");
    if (!f) return false;
    f.println(line);
    f.close();
    return true;
  }
  
  bool deleteFile(const String& path) {
    if (_isSD(path)) return SD.remove(path.c_str());
    return SPIFFS.remove(path.c_str());
  }
  
  bool fileExists(const String& path) {
    if (_isSD(path)) return SD.exists(path.c_str());
    return SPIFFS.exists(path.c_str());
  }
  
  size_t fileSize(const String& path) {
    File f = _open(path, "r");
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
  }
  
  bool makeDir(const String& path) {
    if (_isSD(path)) return SD.mkdir(path.c_str());
    return false; // SPIFFS doesn't have real dirs
  }
  
  // ─── Directory listing ────────────────────────────────────
  std::vector<String> listDir(const String& path) {
    std::vector<String> result;
    File dir;
    
    if (_isSD(path)) {
      dir = SD.open(path.c_str());
    } else {
      dir = SPIFFS.open(path.c_str());
    }
    
    if (!dir || !dir.isDirectory()) return result;
    
    File f;
    while ((f = dir.openNextFile())) {
      result.push_back(f.name());
      f.close();
    }
    dir.close();
    return result;
  }
  
  // ─── Script loaders ───────────────────────────────────────
  String loadLuaScript(const String& name) {
    String path = "/scripts/lua/" + name;
    if (!path.endsWith(".lua")) path += ".lua";
    return readFile(path);
  }
  
  String loadJSScript(const String& name) {
    String path = "/scripts/js/" + name;
    if (!path.endsWith(".js")) path += ".js";
    return readFile(path);
  }
  
  // ─── ROM loading ──────────────────────────────────────────
  bool loadROM(const String& path, uint8_t* buffer, size_t maxSize, size_t& actualSize) {
    File f = _open(path, "r");
    if (!f) return false;
    actualSize = f.size();
    if (actualSize > maxSize) { f.close(); return false; }
    f.read(buffer, actualSize);
    f.close();
    return true;
  }
  
  // ─── SD card stats ────────────────────────────────────────
  void refresh() {
    if (sdMounted) {
      sdTotalBytes = SD.totalBytes();
      sdUsedBytes  = SD.usedBytes();
    }
  }
  
  String getStorageInfo() {
    char buf[80];
    snprintf(buf, 80, "SD: %llu/%llu MB | SPIFFS: %llu/%llu KB",
      sdUsedBytes/1048576, sdTotalBytes/1048576,
      spiffsUsed/1024, spiffsTotal/1024);
    return String(buf);
  }
  
  bool isSDMounted() { return sdMounted; }
  
  // ─── Log helper ───────────────────────────────────────────
  void log(const String& msg) {
    String path = sdMounted ? "/logs/system.log" : "/syslog";
    String entry = "[" + String(millis()/1000) + "] " + msg;
    appendFile(path, entry);
  }

private:
  bool _isSD(const String& path) {
    return sdMounted && (path.startsWith("/sd") || path.startsWith("/roms") ||
           path.startsWith("/scripts") || path.startsWith("/saves") ||
           path.startsWith("/logs"));
  }
  
  File _open(const String& path, const char* mode) {
    if (_isSD(path)) return SD.open(path.c_str(), mode);
    return SPIFFS.open(path.c_str(), mode);
  }
  
  void _createDirectories() {
    const char* dirs[] = {
      "/scripts", "/scripts/lua", "/scripts/js",
      "/roms", "/roms/gb", "/roms/nes", "/roms/gba",
      "/saves", "/apps", "/logs", "/music", "/pictures",
      nullptr
    };
    for (int i = 0; dirs[i]; i++) {
      if (!SD.exists(dirs[i])) SD.mkdir(dirs[i]);
    }
    Serial.println(F("[FS] Directory structure ready"));
    
    // Create sample Lua script
    if (!SD.exists("/scripts/lua/hello.lua")) {
      File f = SD.open("/scripts/lua/hello.lua", "w");
      if (f) {
        f.println("-- LegacyOS Lua Hello World");
        f.println("print('Hello from Lua!')");
        f.println("os_print('Hello LegacyOS!')");
        f.println("for i = 1, 5 do");
        f.println("  print('Count: ' .. i)");
        f.println("end");
        f.close();
      }
    }
    
    // Create sample JS script
    if (!SD.exists("/scripts/js/hello.js")) {
      File f = SD.open("/scripts/js/hello.js", "w");
      if (f) {
        f.println("// LegacyOS JavaScript Hello World");
        f.println("print('Hello from JavaScript!');");
        f.println("var x = 42;");
        f.println("print('Answer: ' + x);");
        f.close();
      }
    }
  }
};
