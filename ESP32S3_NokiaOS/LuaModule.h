
/*
  Lua Interpreter Module for ESP32-S3 Nokia OS
  Simplified Lua 5.1 compatible interpreter
*/

#ifndef LUA_MODULE_H
#define LUA_MODULE_H

#include <Arduino.h>
#include <vector>
#include <map>

// Lua value types
enum LuaType {
  LUA_TNIL,
  LUA_TBOOLEAN,
  LUA_TNUMBER,
  LUA_TSTRING,
  LUA_TTABLE,
  LUA_TFUNCTION,
  LUA_TUSERDATA
};

struct LuaValue {
  LuaType type;
  union {
    bool b;
    double n;
    void* p;
  };
  String s;
};

class LuaState {
private:
  std::map<String, LuaValue> globals;
  std::vector<LuaValue> stack;
  String output;

  // Built-in functions
  static int lua_print(LuaState* L);
  static int lua_type(LuaState* L);
  static int lua_tostring(LuaState* L);
  static int lua_tonumber(LuaState* L);
  static int lua_gpio_write(LuaState* L);
  static int lua_gpio_read(LuaState* L);
  static int lua_delay(LuaState* L);
  static int lua_millis(LuaState* L);
  static int lua_wifi_connect(LuaState* L);
  static int lua_http_get(LuaState* L);

public:
  LuaState();
  ~LuaState();

  bool doString(const String& code);
  bool doFile(const String& filename);
  String getOutput() { return output; }
  void clearOutput() { output = ""; }

  void setGlobal(const String& name, const LuaValue& value);
  LuaValue getGlobal(const String& name);

  // Stack operations
  void push(const LuaValue& value);
  LuaValue pop();
  LuaValue peek(int index);

private:
  // Parser helpers
  String trim(const String& s);
  std::vector<String> tokenize(const String& code);
  bool executeLine(const String& line);
  bool executeAssignment(const String& line);
  bool executeFunction(const String& line);
  bool executeIf(const String& line);
  bool executeWhile(const String& line);
  bool executeFor(const String& line);

  // Expression evaluation
  LuaValue evaluateExpression(String expr);
  LuaValue parseValue(const String& token);
  bool isNumber(const String& s);
  bool isString(const String& s);
  bool isBoolean(const String& s);
};

// Implementation
LuaState::LuaState() {
  // Register built-in functions
  LuaValue printFunc;
  printFunc.type = LUA_TFUNCTION;
  printFunc.p = (void*)lua_print;
  globals["print"] = printFunc;

  LuaValue typeFunc;
  typeFunc.type = LUA_TFUNCTION;
  typeFunc.p = (void*)lua_type;
  globals["type"] = typeFunc;

  LuaValue gpioWrite;
  gpioWrite.type = LUA_TFUNCTION;
  gpioWrite.p = (void*)lua_gpio_write;
  globals["gpio_write"] = gpioWrite;

  LuaValue gpioRead;
  gpioRead.type = LUA_TFUNCTION;
  gpioRead.p = (void*)lua_gpio_read;
  globals["gpio_read"] = gpioRead;

  LuaValue delayFunc;
  delayFunc.type = LUA_TFUNCTION;
  delayFunc.p = (void*)lua_delay;
  globals["delay"] = delayFunc;

  LuaValue millisFunc;
  millisFunc.type = LUA_TFUNCTION;
  millisFunc.p = (void*)lua_millis;
  globals["millis"] = millisFunc;
}

LuaState::~LuaState() {}

bool LuaState::doString(const String& code) {
  output = "";

  // Simple line-by-line execution
  int start = 0;
  int end = code.indexOf('\n');

  while (end != -1) {
    String line = code.substring(start, end);
    line.trim();

    if (line.length() > 0 && !line.startsWith("--")) {
      if (!executeLine(line)) {
        output += "Error in: " + line + "\n";
        return false;
      }
    }

    start = end + 1;
    end = code.indexOf('\n', start);
  }

  // Last line
  String lastLine = code.substring(start);
  lastLine.trim();
  if (lastLine.length() > 0 && !lastLine.startsWith("--")) {
    executeLine(lastLine);
  }

  return true;
}

bool LuaState::executeLine(const String& line) {
  // Check for assignment
  if (line.indexOf('=') > 0 && line.indexOf('(') < 0) {
    return executeAssignment(line);
  }

  // Check for function call
  if (line.indexOf('(') >= 0) {
    return executeFunction(line);
  }

  // Check for control structures
  if (line.startsWith("if ")) {
    return executeIf(line);
  }

  if (line.startsWith("while ")) {
    return executeWhile(line);
  }

  if (line.startsWith("for ")) {
    return executeFor(line);
  }

  return true;
}

bool LuaState::executeAssignment(const String& line) {
  int eqPos = line.indexOf('=');
  String var = line.substring(0, eqPos);
  String expr = line.substring(eqPos + 1);

  var.trim();
  expr.trim();

  LuaValue value = evaluateExpression(expr);
  setGlobal(var, value);

  return true;
}

bool LuaState::executeFunction(const String& line) {
  int parenPos = line.indexOf('(');
  String funcName = line.substring(0, parenPos);
  String args = line.substring(parenPos + 1, line.lastIndexOf(')'));

  funcName.trim();
  args.trim();

  // Parse arguments
  std::vector<LuaValue> argValues;
  int start = 0;
  int comma = args.indexOf(',');

  while (comma != -1) {
    String arg = args.substring(start, comma);
    arg.trim();
    argValues.push_back(evaluateExpression(arg));
    start = comma + 1;
    comma = args.indexOf(',', start);
  }

  String lastArg = args.substring(start);
  lastArg.trim();
  if (lastArg.length() > 0) {
    argValues.push_back(evaluateExpression(lastArg));
  }

  // Push arguments to stack
  for (auto& arg : argValues) {
    push(arg);
  }

  // Call function
  if (funcName == "print") {
    lua_print(this);
  } else if (funcName == "type") {
    lua_type(this);
  } else if (funcName == "gpio_write") {
    lua_gpio_write(this);
  } else if (funcName == "gpio_read") {
    lua_gpio_read(this);
  } else if (funcName == "delay") {
    lua_delay(this);
  } else if (funcName == "millis") {
    lua_millis(this);
  }

  return true;
}

bool LuaState::executeIf(const String& line) {
  // Simplistic placeholder for if statement
  return true;
}

bool LuaState::executeWhile(const String& line) {
  // Simplistic placeholder for while statement
  return true;
}

bool LuaState::executeFor(const String& line) {
  // Simplistic placeholder for for statement
  return true;
}

LuaValue LuaState::evaluateExpression(String expr) {
  expr.trim();

  // Check for string
  if (expr.startsWith("\"") && expr.endsWith("\"")) {
    LuaValue v;
    v.type = LUA_TSTRING;
    v.s = expr.substring(1, expr.length() - 1);
    return v;
  }

  // Check for number
  if (isNumber(expr)) {
    LuaValue v;
    v.type = LUA_TNUMBER;
    v.n = expr.toFloat();
    return v;
  }

  // Check for boolean
  if (expr == "true") {
    LuaValue v;
    v.type = LUA_TBOOLEAN;
    v.b = true;
    return v;
  }
  if (expr == "false") {
    LuaValue v;
    v.type = LUA_TBOOLEAN;
    v.b = false;
    return v;
  }

  if (expr == "nil") {
    LuaValue v;
    v.type = LUA_TNIL;
    return v;
  }

  // Variable lookup
  return getGlobal(expr);
}

void LuaState::setGlobal(const String& name, const LuaValue& value) {
  globals[name] = value;
}

LuaValue LuaState::getGlobal(const String& name) {
  if (globals.count(name) > 0) {
    return globals[name];
  }
  LuaValue v;
  v.type = LUA_TNIL;
  return v;
}

void LuaState::push(const LuaValue& value) {
  stack.push_back(value);
}

LuaValue LuaState::pop() {
  if (stack.empty()) {
    LuaValue v;
    v.type = LUA_TNIL;
    return v;
  }
  LuaValue v = stack.back();
  stack.pop_back();
  return v;
}

LuaValue LuaState::peek(int index) {
  if (index <= 0 || index > (int)stack.size()) {
    LuaValue v;
    v.type = LUA_TNIL;
    return v;
  }
  return stack[stack.size() - index];
}

bool LuaState::isNumber(const String& s) {
  for (int i = 0; i < s.length(); i++) {
    if (!isdigit(s[i]) && s[i] != '.' && s[i] != '-') {
      return false;
    }
  }
  return true;
}

// Built-in functions
int LuaState::lua_print(LuaState* L) {
  LuaValue arg = L->pop();
  String text;

  switch (arg.type) {
    case LUA_TSTRING:
      text = arg.s;
      break;
    case LUA_TNUMBER:
      text = String(arg.n);
      break;
    case LUA_TBOOLEAN:
      text = arg.b ? "true" : "false";
      break;
    case LUA_TNIL:
      text = "nil";
      break;
    default:
      text = "[object]";
  }

  L->output += text + "\n";
  Serial.println(text);
  return 0;
}

int LuaState::lua_type(LuaState* L) {
  LuaValue arg = L->pop();
  String typeName;

  switch (arg.type) {
    case LUA_TNIL: typeName = "nil"; break;
    case LUA_TBOOLEAN: typeName = "boolean"; break;
    case LUA_TNUMBER: typeName = "number"; break;
    case LUA_TSTRING: typeName = "string"; break;
    case LUA_TTABLE: typeName = "table"; break;
    case LUA_TFUNCTION: typeName = "function"; break;
    default: typeName = "userdata";
  }

  LuaValue result;
  result.type = LUA_TSTRING;
  result.s = typeName;
  L->push(result);
  return 1;
}

int LuaState::lua_gpio_write(LuaState* L) {
  LuaValue pinVal = L->pop();
  LuaValue stateVal = L->pop();

  if (pinVal.type == LUA_TNUMBER && stateVal.type == LUA_TNUMBER) {
    int pin = (int)pinVal.n;
    int state = (int)stateVal.n;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state);
  }
  return 0;
}

int LuaState::lua_gpio_read(LuaState* L) {
  LuaValue pinVal = L->pop();

  if (pinVal.type == LUA_TNUMBER) {
    int pin = (int)pinVal.n;
    pinMode(pin, INPUT);
    int val = digitalRead(pin);

    LuaValue result;
    result.type = LUA_TNUMBER;
    result.n = val;
    L->push(result);
    return 1;
  }
  return 0;
}

int LuaState::lua_delay(LuaState* L) {
  LuaValue ms = L->pop();
  if (ms.type == LUA_TNUMBER) {
    delay((int)ms.n);
  }
  return 0;
}

int LuaState::lua_millis(LuaState* L) {
  LuaValue result;
  result.type = LUA_TNUMBER;
  result.n = millis();
  L->push(result);
  return 1;
}

#endif
