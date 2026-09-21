#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════
//  LuaEngine - Lua 5.1 scripting engine for LegacyOS
//  Uses: EloquentLua or lua-arduino library
//  Install: EloquentLua from Library Manager
// ═══════════════════════════════════════════════════════════

// NOTE: To use Lua, install "EloquentLua" library or
// "Arduino-Lua" from the Arduino Library Manager.
// This header provides the interface; the implementation
// bridges to whichever Lua library you install.

// For now we use a simple embedded interpreter concept.
// Add: #include <LuaWrapper.h> or similar per your installed lib.

struct LuaResult {
  bool   success;
  String output;
  String error;
  float  execTime;
};

class LuaEngine {
public:
  bool available = false;
  std::vector<String> outputLines;
  
  // OS API callbacks registered into Lua
  std::function<void(const String&)> onPrint;
  std::function<void(TFT_eSprite*, int, int, const String&)> onDraw;
  
  void init() {
    // Try to init Lua
    // Requires EloquentLua or similar library
    // Available at: https://github.com/eloquentarduino/EloquentLua
    available = false;  // Set true when lib is installed
    Serial.println(F("[Lua] Engine initialized (stub)"));
    Serial.println(F("[Lua] Install 'EloquentLua' library for full Lua support"));
  }
  
  LuaResult execute(const String& script) {
    LuaResult r;
    r.success = false;
    r.error   = "Lua library not installed";
    r.execTime = 0;
    
    if (!available) {
      outputLines.push_back("ERROR: Lua not available");
      outputLines.push_back("Install 'EloquentLua' library");
      return r;
    }
    
    /* ── With EloquentLua installed, code would be: ──────────
    
    using namespace Eloquent::Lua;
    Interpreter lua;
    
    // Register OS API
    lua.registerFunction("print", [](lua_State* L) -> int {
      String s = lua_tostring(L, -1);
      outputLines.push_back(s);
      if (onPrint) onPrint(s);
      return 0;
    });
    
    lua.registerFunction("os_print", [](lua_State* L) -> int {
      // Same as print for now
      return 0;
    });
    
    lua.registerFunction("os_delay", [](lua_State* L) -> int {
      int ms = lua_tointeger(L, -1);
      delay(ms);
      return 0;
    });
    
    uint32_t t = millis();
    bool ok = lua.execute(script.c_str());
    r.execTime = (millis() - t) / 1000.0f;
    r.success  = ok;
    if (!ok) r.error = lua.getError();
    r.output = outputLines.join('\n');
    return r;
    ────────────────────────────────────────────────────────── */
    
    // Fallback: simple line evaluator for demo
    _simpleLuaDemo(script, r);
    return r;
  }
  
  // Run a .lua file from filesystem
  LuaResult executeFile(const String& path) {
    // Read file content
    if (!SD.exists(path.c_str())) {
      LuaResult r;
      r.success = false;
      r.error   = "File not found: " + path;
      return r;
    }
    File f = SD.open(path.c_str(), "r");
    String code = f.readString();
    f.close();
    return execute(code);
  }
  
  void reset() {
    outputLines.clear();
  }
  
private:
  void _simpleLuaDemo(const String& script, LuaResult& r) {
    // Minimal demo evaluator - shows lines that call print()
    outputLines.clear();
    uint32_t t = millis();
    
    int start = 0;
    while (start < (int)script.length()) {
      int end = script.indexOf('\n', start);
      if (end == -1) end = script.length();
      String line = script.substring(start, end);
      line.trim();
      
      // Handle print("...") calls
      if (line.startsWith("print(") && line.endsWith(")")) {
        String content = line.substring(6, line.length()-1);
        if (content.startsWith("'") || content.startsWith("\"")) {
          content = content.substring(1, content.length()-1);
        }
        outputLines.push_back(content);
        if (onPrint) onPrint(content);
      } else if (line.startsWith("--")) {
        // Comment: skip
      } else if (line.length() > 0) {
        outputLines.push_back(">> " + line);
      }
      
      start = end + 1;
    }
    
    r.success  = true;
    r.execTime = (millis() - t) / 1000.0f;
    r.output   = "";
    for (auto& l : outputLines) r.output += l + "\n";
  }
};

