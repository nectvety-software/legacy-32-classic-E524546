#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════
//  JSEngine - JavaScript runtime for LegacyOS
//  Uses: Duktape (duktape-arduino) or ArduinoJS
//  Install: Search "duktape" in Arduino Library Manager
//           or https://github.com/nicedoc/duktape-arduino
// ═══════════════════════════════════════════════════════════

struct JSResult {
  bool   success;
  String output;
  String error;
  String returnValue;
  float  execTime;
};

class JSEngine {
public:
  bool available = false;
  std::vector<String> outputLines;
  
  std::function<void(const String&)> onPrint;
  
  void init() {
    // Requires Duktape library
    available = false;
    Serial.println(F("[JS] Engine initialized (stub)"));
    Serial.println(F("[JS] Install 'duktape-arduino' library for full JS support"));
  }
  
  JSResult execute(const String& code) {
    JSResult r;
    r.success = false;
    r.execTime = 0;
    
    if (!available) {
      r.error = "JS engine not available";
      outputLines.push_back("ERROR: Install duktape-arduino");
      return r;
    }
    
    /* ── With Duktape installed: ──────────────────────────────
    
    duk_context* ctx = duk_create_heap_default();
    if (!ctx) { r.error = "Heap create failed"; return r; }
    
    // Register print function
    duk_push_c_function(ctx, [](duk_context* cx) -> duk_ret_t {
      const char* s = duk_to_string(cx, -1);
      outputLines.push_back(String(s));
      if (onPrint) onPrint(String(s));
      return 0;
    }, 1);
    duk_put_global_string(ctx, "print");
    
    // Register os object
    duk_push_object(ctx);
    duk_push_c_function(ctx, [](duk_context* cx) -> duk_ret_t {
      duk_push_int(cx, millis() / 1000);
      return 1;
    }, 0);
    duk_put_prop_string(ctx, -2, "uptime");
    duk_put_global_string(ctx, "os");
    
    uint32_t t = millis();
    if (duk_peval_string(ctx, code.c_str()) == 0) {
      r.success = true;
      r.returnValue = duk_to_string(ctx, -1);
    } else {
      r.error = duk_to_string(ctx, -1);
    }
    r.execTime = (millis() - t) / 1000.0f;
    duk_pop(ctx);
    duk_destroy_heap(ctx);
    
    ────────────────────────────────────────────────────────── */
    
    // Demo fallback
    _simpleJSDemo(code, r);
    return r;
  }
  
  JSResult executeFile(const String& path) {
    if (!SD.exists(path.c_str())) {
      JSResult r;
      r.success = false;
      r.error = "File not found: " + path;
      return r;
    }
    File f = SD.open(path.c_str(), "r");
    String code = f.readString();
    f.close();
    return execute(code);
  }
  
  void reset() { outputLines.clear(); }

private:
  void _simpleJSDemo(const String& code, JSResult& r) {
    outputLines.clear();
    uint32_t t = millis();
    
    int start = 0;
    while (start < (int)code.length()) {
      int end = code.indexOf('\n', start);
      if (end == -1) end = code.length();
      String line = code.substring(start, end);
      line.trim();
      
      if (line.startsWith("//")) {
        // comment
      } else if (line.startsWith("print(") || line.startsWith("console.log(")) {
        int ps = line.indexOf('(') + 1;
        int pe = line.lastIndexOf(')');
        if (pe > ps) {
          String content = line.substring(ps, pe);
          content.replace("\"", "");
          content.replace("'", "");
          outputLines.push_back(content);
          if (onPrint) onPrint(content);
        }
      } else if (line.length() > 0 && !line.startsWith("var ") && 
                 !line.startsWith("let ") && !line.startsWith("const ")) {
        outputLines.push_back(">> " + line);
      }
      start = end + 1;
    }
    
    r.success = true;
    r.execTime = (millis() - t) / 1000.0f;
    for (auto& l : outputLines) r.output += l + "\n";
  }
};

