#include "lua_vm.h"
#include <string.h>

LuaVM luaVM;

LuaVM::LuaVM() : code(nullptr), pc(0), lineNum(1), stackTop(0), callDepth(0), error(false), outputLen(0) {
  output[0] = '\0';
}

void LuaVM::reset() {
  for (int i = 0; i < LUA_MAX_VARS; i++) vars[i].used = false;
  output[0] = '\0';
  outputLen = 0;
  error = false;
  errorMsg = "";
  stackTop = 0;
  callDepth = 0;
}

const char* LuaVM::getOutput() { return output; }
bool LuaVM::hasError() { return error; }
const char* LuaVM::getError() { return errorMsg.c_str(); }

void LuaVM::printResult(const String& s) {
  if (outputLen > 0 && outputLen < LUA_OUTPUT_SIZE - 1) {
    output[outputLen++] = '\n';
  }
  for (int i = 0; i < s.length() && outputLen < LUA_OUTPUT_SIZE - 1; i++) {
    output[outputLen++] = s[i];
  }
  output[outputLen] = '\0';
}

void LuaVM::push(const LuaValue& v) {
  if (stackTop < LUA_STACK_SIZE - 1) stack[stackTop++] = v;
}

LuaValue LuaVM::pop() {
  if (stackTop > 0) return stack[--stackTop];
  LuaValue v = {LUA_NIL}; return v;
}

bool LuaVM::isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool LuaVM::isDigit(char c) {
  return (c >= '0' && c <= '9');
}

bool LuaVM::isAlphaNum(char c) {
  return isAlpha(c) || isDigit(c) || c == '_';
}

void LuaVM::skipSpaces() {
  while (code[pc] == ' ' || code[pc] == '\t' || code[pc] == '\r') pc++;
  if (code[pc] == '-' && code[pc+1] == '-') skipToLineEnd();
  while (code[pc] == ' ' || code[pc] == '\t' || code[pc] == '\r') pc++;
}

void LuaVM::skipToLineEnd() {
  while (code[pc] != '\n' && code[pc] != '\0') pc++;
}

String LuaVM::parseId() {
  String id = "";
  while (isAlphaNum(code[pc])) id += code[pc++];
  return id;
}

String LuaVM::parseString() {
  char q = code[pc++];
  String s = "";
  while (code[pc] != q && code[pc] != '\0' && code[pc] != '\n') {
    if (code[pc] == '\\') {
      pc++;
      if (code[pc] == 'n') s += '\n';
      else if (code[pc] == 't') s += '\t';
      else if (code[pc] == '"') s += '"';
      else if (code[pc] == '\'') s += '\'';
      else s += code[pc];
    } else s += code[pc];
    pc++;
  }
  if (code[pc] == q) pc++;
  return s;
}

double LuaVM::parseNumber() {
  String n = "";
  while (isDigit(code[pc]) || code[pc] == '.') n += code[pc++];
  return n.toFloat();
}

int LuaVM::findVar(const String& name) {
  for (int i = 0; i < LUA_MAX_VARS; i++)
    if (vars[i].used && vars[i].name == name) return i;
  return -1;
}

int LuaVM::addVar(const String& name) {
  for (int i = 0; i < LUA_MAX_VARS; i++)
    if (!vars[i].used) { vars[i].name = name; vars[i].used = true; return i; }
  return -1;
}

void LuaVM::setVar(const String& name, const LuaValue& val) {
  int idx = findVar(name);
  if (idx < 0) idx = addVar(name);
  if (idx >= 0) vars[idx].value = val;
}

LuaValue LuaVM::getVar(const String& name) {
  int idx = findVar(name);
  if (idx >= 0) return vars[idx].value;
  LuaValue v = {LUA_NIL}; return v;
}

String LuaVM::evalString() {
  skipSpaces();
  if (code[pc] == '"' || code[pc] == '\'') {
    pc++;
    String s = parseString();
    skipSpaces();
    while (code[pc] == '.' && code[pc+1] == '.') {
      pc += 2;
      skipSpaces();
      if (code[pc] == '"' || code[pc] == '\'') {
        pc++;
        s += parseString();
        skipSpaces();
      } else if (isAlpha(code[pc]) || code[pc] == '_') {
        String vname = parseId();
        LuaValue v = getVar(vname);
        if (v.type == LUA_STRING) s += v.str;
        else if (v.type == LUA_NUMBER) s += String(v.number);
        skipSpaces();
      }
    }
    return s;
  }
  return "";
}

double LuaVM::evalNumber() {
  skipSpaces();
  double val = 0;
  if (code[pc] == '-') { pc++; val = -parseNumber(); }
  else if (isDigit(code[pc])) val = parseNumber();
  skipSpaces();
  return val;
}

bool LuaVM::evalBool() {
  skipSpaces();
  if (code[pc] == 't' && code[pc+1] == 'r' && code[pc+2] == 'u' && code[pc+3] == 'e') {
    pc += 4; return true;
  }
  if (code[pc] == 'f' && code[pc+1] == 'a' && code[pc+2] == 'l' && code[pc+3] == 's' && code[pc+4] == 'e') {
    pc += 5; return false;
  }
  double n = evalNumber();
  return n != 0;
}

LuaValue LuaVM::evalFactor() {
  skipSpaces();
  LuaValue result = {LUA_NUMBER, 0, "", false};
  
  if (code[pc] == '-') {
    pc++;
    result = evalFactor();
    if (result.type == LUA_NUMBER) result.number = -result.number;
    return result;
  }
  
  if (code[pc] == '(') {
    pc++;
    result = evalExpr();
    if (code[pc] == ')') pc++;
    return result;
  }
  
  if (code[pc] == '"' || code[pc] == '\'') {
    result.type = LUA_STRING;
    result.str = evalString();
    return result;
  }
  
  if (code[pc] == 'n' && code[pc+1] == 'i' && code[pc+2] == 'l') {
    pc += 3; result.type = LUA_NIL; return result;
  }
  
  if (code[pc] == 't' && code[pc+1] == 'r' && code[pc+2] == 'u' && code[pc+3] == 'e') {
    pc += 4; result.type = LUA_BOOLEAN; result.boolean = true; return result;
  }
  
  if (code[pc] == 'f' && code[pc+1] == 'a' && code[pc+2] == 'l' && code[pc+3] == 's' && code[pc+4] == 'e') {
    pc += 5; result.type = LUA_BOOLEAN; result.boolean = false; return result;
  }
  
  if (isDigit(code[pc]) || (code[pc] == '-' && isDigit(code[pc+1]))) {
    result.type = LUA_NUMBER;
    result.number = evalNumber();
    return result;
  }
  
  if (isAlpha(code[pc]) || code[pc] == '_') {
    String name = parseId();
    skipSpaces();
    
    if (code[pc] == '=' && code[pc+1] != '=') {
      pc++;
      result = evalExpr();
      setVar(name, result);
      return result;
    }
    
    return getVar(name);
  }
  
  return result;
}

LuaValue LuaVM::evalTerm() {
  LuaValue result = evalFactor();
  skipSpaces();
  
  while (code[pc] == '*' || code[pc] == '/') {
    char op = code[pc++];
    LuaValue rhs = evalFactor();
    if (result.type == LUA_NUMBER && rhs.type == LUA_NUMBER) {
      if (op == '*') result.number *= rhs.number;
      else if (op == '/' && rhs.number != 0) result.number /= rhs.number;
    }
    skipSpaces();
  }
  return result;
}

LuaValue LuaVM::evalExpr() {
  LuaValue result = evalTerm();
  skipSpaces();
  
  while (code[pc] == '+' || code[pc] == '-' || (code[pc] == '.' && code[pc+1] == '.' && code[pc+2] == '.')) {
    if (code[pc] == '.' && code[pc+1] == '.' && code[pc+2] == '.') {
      pc += 3;
      skipSpaces();
      LuaValue rhs = evalTerm();
      if (result.type == LUA_STRING && rhs.type == LUA_STRING) result.str += rhs.str;
      else if (result.type == LUA_STRING && rhs.type == LUA_NUMBER) result.str += String(rhs.number);
      else if (result.type == LUA_NUMBER && rhs.type == LUA_STRING) result.str = String(result.number) + rhs.str;
      skipSpaces();
    } else {
      char op = code[pc++];
      LuaValue rhs = evalTerm();
      if (result.type == LUA_NUMBER && rhs.type == LUA_NUMBER) {
        if (op == '+') result.number += rhs.number;
        else if (op == '-') result.number -= rhs.number;
      }
      skipSpaces();
    }
  }
  return result;
}

bool LuaVM::checkCond() {
  skipSpaces();
  LuaValue v = evalExpr();
  if (v.type == LUA_BOOLEAN) return v.boolean;
  if (v.type == LUA_NUMBER) return v.number != 0;
  if (v.type == LUA_STRING) return v.str.length() > 0;
  return false;
}

int LuaVM::findBlockEnd(const char* keyword) {
  int depth = 1;
  int saved = pc;
  while (code[pc] != '\0') {
    skipSpaces();
    if (code[pc] == '\n') { pc++; lineNum++; skipSpaces(); }
    
    if (strncmp(code + pc, "if", 2) == 0 && !isAlphaNum(code[pc+2])) { depth++; pc += 2; }
    else if (strncmp(code + pc, "for", 3) == 0 && !isAlphaNum(code[pc+3])) { depth++; pc += 3; }
    else if (strncmp(code + pc, "while", 5) == 0 && !isAlphaNum(code[pc+5])) { depth++; pc += 5; }
    else if (strncmp(code + pc, "function", 8) == 0 && !isAlphaNum(code[pc+8])) { depth++; pc += 8; }
    else if (strncmp(code + pc, "end", 3) == 0 && !isAlphaNum(code[pc+3])) {
      depth--; pc += 3;
      if (depth == 0) return pc;
    }
    else pc++;
  }
  pc = saved;
  return -1;
}

void LuaVM::doPrint() {
  skipSpaces();
  if (code[pc] == '(') pc++;
  skipSpaces();
  
  String result = "";
  bool first = true;
  
  while (code[pc] != '\n' && code[pc] != '\0' && code[pc] != ')') {
    skipSpaces();
    if (code[pc] == '"' || code[pc] == '\'') {
      result += evalString();
    } else if (code[pc] == '.' && code[pc+1] == '.' && code[pc+2] == '.') {
      pc += 3;
    } else if (code[pc] == '.') {
      pc++;
    } else if (isAlpha(code[pc]) || code[pc] == '_') {
      String name = parseId();
      LuaValue v = getVar(name);
      if (v.type == LUA_STRING) result += v.str;
      else if (v.type == LUA_NUMBER) result += String(v.number);
      else if (v.type == LUA_BOOLEAN) result += (v.boolean ? "true" : "false");
      else if (v.type == LUA_NIL) result += "nil";
    } else if (isDigit(code[pc]) || code[pc] == '-') {
      double n = evalNumber();
      result += String(n);
    } else {
      break;
    }
    skipSpaces();
  }
  
  if (code[pc] == ')') pc++;
  printResult(result);
}

void LuaVM::doSleep() {
  skipSpaces();
  if (code[pc] == '(') pc++;
  skipSpaces();
  String n = "";
  while (isDigit(code[pc])) n += code[pc++];
  if (code[pc] == ')') pc++;
  delay(n.toInt());
}

void LuaVM::doIf() {
  skipSpaces();
  bool cond = checkCond();
  
  int endPos = findBlockEnd("end");
  int elsePos = -1;
  
  if (endPos < 0) { error = true; errorMsg = "missing 'end'"; return; }
  
  int saved = pc;
  pc = saved;
  while (pc < endPos) {
    skipSpaces();
    if (code[pc] == '\n') { pc++; lineNum++; continue; }
    if (strncmp(code + pc, "else", 4) == 0 && !isAlphaNum(code[pc+4])) {
      elsePos = pc; break;
    }
    pc++;
  }
  
  if (cond) {
    pc = saved;
    while (pc < (elsePos > 0 ? elsePos : endPos)) {
      skipSpaces();
      if (code[pc] == '\n') { pc++; lineNum++; continue; }
      if (strncmp(code + pc, "end", 3) == 0 && !isAlphaNum(code[pc+3])) break;
      runStatement();
    }
  } else if (elsePos > 0) {
    pc = elsePos + 4;
    while (pc < endPos) {
      skipSpaces();
      if (code[pc] == '\n') { pc++; lineNum++; continue; }
      if (strncmp(code + pc, "end", 3) == 0 && !isAlphaNum(code[pc+3])) break;
      runStatement();
    }
  }
  
  pc = endPos + 3;
}

void LuaVM::doFor() {
  skipSpaces();
  String varName = parseId();
  skipSpaces();
  if (code[pc] != '=') return;
  pc++;
  
  double start = evalNumber();
  skipSpaces();
  while (code[pc] == ',') { pc++; skipSpaces(); while (isAlpha(code[pc])) pc++; skipSpaces(); }
  
  double end = evalNumber();
  skipSpaces();
  double step = 1;
  if (code[pc] == ',') {
    pc++; step = evalNumber();
  }
  
  skipSpaces();
  while (code[pc] == 'd' && code[pc+1] == 'o') pc += 2;
  skipSpaces();
  
  int endPos = findBlockEnd("end");
  if (endPos < 0) { error = true; errorMsg = "missing 'end' for 'for'"; return; }
  
  int bodyStart = pc;
  int bodyLen = endPos - pc;
  
  for (double i = start; (step > 0 ? i <= end : i >= end); i += step) {
    LuaValue v; v.type = LUA_NUMBER; v.number = i;
    setVar(varName, v);
    
    char body[256] = "";
    int j = 0;
    for (int k = 0; k < bodyLen && j < 255; k++) {
      body[j++] = code[bodyStart + k];
    }
    body[j] = '\0';
    
    LuaVM sub;
    sub.code = body;
    sub.pc = 0;
    sub.lineNum = 1;
    for (int k = 0; k < LUA_MAX_VARS; k++) {
      if (vars[k].used) {
        int idx = sub.addVar(vars[k].name);
        if (idx >= 0) sub.vars[idx].value = vars[k].value;
      }
    }
    sub.runProgram();
    
    for (int k = 0; k < LUA_MAX_VARS; k++) {
      if (sub.vars[k].used) {
        vars[k] = sub.vars[k];
      }
    }
    
    if (sub.outputLen > 0 && outputLen < LUA_OUTPUT_SIZE - sub.outputLen) {
      if (outputLen > 0) output[outputLen++] = '\n';
      for (int k = 0; k < sub.outputLen && outputLen < LUA_OUTPUT_SIZE - 1; k++) {
        output[outputLen++] = sub.output[k];
      }
      output[outputLen] = '\0';
    }
    
    if (sub.error) { error = true; errorMsg = sub.errorMsg; return; }
  }
  
  pc = endPos + 3;
}

void LuaVM::doWhile() {
  int loopStart = pc;
  skipSpaces();
  
  int condPos = pc;
  bool cond = checkCond();
  
  int endPos = findBlockEnd("end");
  if (endPos < 0) { error = true; errorMsg = "missing 'end' for 'while'"; return; }
  
  int bodyStart = pc;
  int bodyLen = endPos - pc;
  
  while (cond && !error) {
    char body[256] = "";
    int j = 0;
    for (int k = 0; k < bodyLen && j < 255; k++) {
      body[j++] = code[bodyStart + k];
    }
    body[j] = '\0';
    
    LuaVM sub;
    sub.code = body;
    sub.pc = 0;
    sub.lineNum = 1;
    for (int k = 0; k < LUA_MAX_VARS; k++) {
      if (vars[k].used) {
        int idx = sub.addVar(vars[k].name);
        if (idx >= 0) sub.vars[idx].value = vars[k].value;
      }
    }
    sub.runProgram();
    
    for (int k = 0; k < LUA_MAX_VARS; k++) {
      if (sub.vars[k].used) {
        vars[k] = sub.vars[k];
      }
    }
    
    if (sub.outputLen > 0 && outputLen < LUA_OUTPUT_SIZE - sub.outputLen) {
      if (outputLen > 0) output[outputLen++] = '\n';
      for (int k = 0; k < sub.outputLen && outputLen < LUA_OUTPUT_SIZE - 1; k++) {
        output[outputLen++] = sub.output[k];
      }
      output[outputLen] = '\0';
    }
    
    if (sub.error) { error = true; errorMsg = sub.errorMsg; return; }
    
    pc = condPos;
    skipSpaces();
    cond = checkCond();
  }
  
  pc = endPos + 3;
}

void LuaVM::doAssignment(const String& name) {
  skipSpaces();
  if (code[pc] != '=') return;
  pc++;
  skipSpaces();
  
  if (code[pc] == '"' || code[pc] == '\'') {
    LuaValue v; v.type = LUA_STRING; v.str = evalString();
    setVar(name, v);
  } else {
    LuaValue v = evalExpr();
    setVar(name, v);
  }
}

void LuaVM::runStatement() {
  skipSpaces();
  if (code[pc] == '\n' || code[pc] == '\0') return;
  
  if (strncmp(code + pc, "print", 5) == 0 && !isAlphaNum(code[pc+5])) {
    pc += 5; doPrint(); return;
  }
  
  if (strncmp(code + pc, "sleep", 5) == 0 && !isAlphaNum(code[pc+5])) {
    pc += 5; doSleep(); return;
  }
  
  if (strncmp(code + pc, "delay", 5) == 0 && !isAlphaNum(code[pc+5])) {
    pc += 5; doSleep(); return;
  }
  
  if (strncmp(code + pc, "screen.clear", 12) == 0 && !isAlphaNum(code[pc+12])) {
    pc += 12; doScreenCmd(CMD_CLEAR); return;
  }
  
  if (strncmp(code + pc, "screen.pixel", 12) == 0 && !isAlphaNum(code[pc+12])) {
    pc += 12; doScreenCmd(CMD_PIXEL); return;
  }
  
  if (strncmp(code + pc, "screen.line", 11) == 0 && !isAlphaNum(code[pc+11])) {
    pc += 11; doScreenCmd(CMD_LINE); return;
  }
  
  if (strncmp(code + pc, "screen.rect", 11) == 0 && !isAlphaNum(code[pc+11])) {
    pc += 11; doScreenCmd(CMD_RECT); return;
  }
  
  if (strncmp(code + pc, "screen.fill", 11) == 0 && !isAlphaNum(code[pc+11])) {
    pc += 11; doScreenCmd(CMD_FILL); return;
  }
  
  if (strncmp(code + pc, "screen.text", 11) == 0 && !isAlphaNum(code[pc+11])) {
    pc += 11; doScreenCmd(CMD_TEXT); return;
  }
  
  if (strncmp(code + pc, "screen.color", 12) == 0 && !isAlphaNum(code[pc+12])) {
    pc += 12; doScreenCmd(CMD_COLOR); return;
  }
  
  if (strncmp(code + pc, "screen.circle", 13) == 0 && !isAlphaNum(code[pc+13])) {
    pc += 13; doScreenCmd(CMD_CIRCLE); return;
  }
  
  if (strncmp(code + pc, "if", 2) == 0 && !isAlphaNum(code[pc+2])) {
    pc += 2; doIf(); return;
  }
  
  if (strncmp(code + pc, "for", 3) == 0 && !isAlphaNum(code[pc+3])) {
    pc += 3; doFor(); return;
  }
  
  if (strncmp(code + pc, "while", 5) == 0 && !isAlphaNum(code[pc+5])) {
    pc += 5; doWhile(); return;
  }
  
  if (strncmp(code + pc, "end", 3) == 0 && !isAlphaNum(code[pc+3])) {
    pc += 3; return;
  }
  
  if (strncmp(code + pc, "return", 6) == 0) {
    pc += 6; return;
  }
  
  if (isAlpha(code[pc]) || code[pc] == '_') {
    String name = parseId();
    skipSpaces();
    if (code[pc] == '=') {
      doAssignment(name);
    }
  }
}

void LuaVM::doScreenCmd(int cmd) {
  skipSpaces();
  if (code[pc] != '(') return;
  pc++;
  
  char obuf[32];
  obuf[0] = '\x02';
  obuf[1] = cmd;
  int olen = 2;
  
  if (cmd == CMD_CLEAR) {
    if (outputLen < LUA_OUTPUT_SIZE - 3) {
      output[outputLen++] = '\x02';
      output[outputLen++] = CMD_CLEAR;
      output[outputLen++] = '\x03';
      output[outputLen] = '\0';
    }
  } else if (cmd == CMD_COLOR) {
    double r = evalNumber(); skipSpaces();
    double g = evalNumber(); skipSpaces();
    double b = evalNumber();
    if (outputLen < LUA_OUTPUT_SIZE - 8) {
      output[outputLen++] = '\x02';
      output[outputLen++] = CMD_COLOR;
      output[outputLen++] = (char)r;
      output[outputLen++] = (char)g;
      output[outputLen++] = (char)b;
      output[outputLen++] = '\x03';
      output[outputLen] = '\0';
    }
  } else {
    double x = evalNumber(); skipSpaces();
    double y = evalNumber();
    
    if (cmd == CMD_PIXEL) {
      if (outputLen < LUA_OUTPUT_SIZE - 8) {
        output[outputLen++] = '\x02';
        output[outputLen++] = CMD_PIXEL;
        output[outputLen++] = (char)((int)x);
        output[outputLen++] = (char)((int)y);
        output[outputLen++] = '\x03';
        output[outputLen] = '\0';
      }
    } else if (cmd == CMD_TEXT) {
      skipSpaces();
      String txt = "";
      if (code[pc] == '"' || code[pc] == '\'') {
        pc++;
        txt = parseString();
        if (code[pc] == '"' || code[pc] == '\'') pc++;
      }
      if (outputLen < LUA_OUTPUT_SIZE - txt.length() - 8) {
        output[outputLen++] = '\x02';
        output[outputLen++] = CMD_TEXT;
        output[outputLen++] = (char)((int)x);
        output[outputLen++] = (char)((int)y);
        for (int i = 0; i < txt.length() && outputLen < LUA_OUTPUT_SIZE - 1; i++) {
          output[outputLen++] = txt[i];
        }
        output[outputLen++] = '\x03';
        output[outputLen] = '\0';
      }
    } else {
      double x2 = 0, y2 = 0, r = 0;
      if (cmd == CMD_LINE || cmd == CMD_RECT || cmd == CMD_FILL || cmd == CMD_CIRCLE) {
        skipSpaces();
        if (cmd == CMD_CIRCLE) {
          r = evalNumber();
        } else {
          x2 = evalNumber(); skipSpaces();
          y2 = evalNumber();
        }
      }
      if (outputLen < LUA_OUTPUT_SIZE - 10) {
        output[outputLen++] = '\x02';
        output[outputLen++] = cmd;
        output[outputLen++] = (char)((int)x);
        output[outputLen++] = (char)((int)y);
        output[outputLen++] = (char)((int)x2);
        output[outputLen++] = (char)((int)y2);
        output[outputLen++] = (char)((int)r);
        output[outputLen++] = '\x03';
        output[outputLen] = '\0';
      }
    }
  }
  
  skipSpaces();
  if (code[pc] == ')') pc++;
}

void LuaVM::runProgram() {
  while (code[pc] != '\0' && !error) {
    skipSpaces();
    if (code[pc] == '\n') { pc++; lineNum++; continue; }
    if (code[pc] == '\0') break;
    if (strncmp(code + pc, "end", 3) == 0 && !isAlphaNum(code[pc+3])) break;
    runStatement();
  }
}

void LuaVM::run(const String& script) {
  reset();
  char* buf = new char[script.length() + 2];
  strcpy(buf, script.c_str());
  buf[script.length()] = '\0';
  code = buf;
  pc = 0;
  lineNum = 1;
  runProgram();
  delete[] buf;
}

void LuaVM::runFile(const String& path) {
  File f = SD.open(path);
  if (!f) { error = true; errorMsg = "Cannot open: " + path; return; }
  String content = "";
  while (f.available()) content += (char)f.read();
  f.close();
  run(content);
}
