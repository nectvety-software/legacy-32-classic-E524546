#ifndef LUA_VM_H
#define LUA_VM_H

#include <Arduino.h>
#include <SD.h>

#define LUA_MAX_VARS 64
#define LUA_STACK_SIZE 128
#define LUA_OUTPUT_SIZE 4096
#define LUA_MAX_DEPTH 16

#define CMD_CLEAR 1
#define CMD_PIXEL 2
#define CMD_LINE 3
#define CMD_RECT 4
#define CMD_FILL 5
#define CMD_TEXT 6
#define CMD_COLOR 7
#define CMD_DELAY 8
#define CMD_CIRCLE 9

enum LuaValueType {
  LUA_NIL,
  LUA_NUMBER,
  LUA_STRING,
  LUA_BOOLEAN,
  LUA_FUNCTION
};

struct LuaValue {
  LuaValueType type;
  double number;
  String str;
  bool boolean;
};

struct LuaVariable {
  String name;
  LuaValue value;
  bool used;
};

class LuaVM {
private:
  LuaVariable vars[LUA_MAX_VARS];
  char output[LUA_OUTPUT_SIZE];
  int outputLen;
  bool error;
  String errorMsg;
  const char* code;
  int pc;
  int lineNum;
  LuaValue stack[LUA_STACK_SIZE];
  int stackTop;
  int callDepth;
  
  LuaValue evalExpr();
  LuaValue evalTerm();
  LuaValue evalFactor();
  String evalString();
  String evalConcat();
  double evalNumber();
  bool evalBool();
  void skipSpaces();
  void skipToLineEnd();
  bool isAlpha(char c);
  bool isDigit(char c);
  bool isAlphaNum(char c);
  String parseId();
  String parseString();
  double parseNumber();
  int findVar(const String& name);
  int addVar(const String& name);
  void setVar(const String& name, const LuaValue& val);
  LuaValue getVar(const String& name);
  void push(const LuaValue& v);
  LuaValue pop();
  void printResult(const String& s);
  void doPrint();
  void doSleep();
  void doIf();
  void doFor();
  void doWhile();
  void doFunction();
  void doReturn();
  void doAssignment(const String& name);
  bool checkCond();
  int findBlockEnd(const char* keyword);
  void runStatement();
  void runProgram();
  void doScreenCmd(int cmd);

public:
  LuaVM();
  void reset();
  void run(const String& script);
  void runFile(const String& path);
  const char* getOutput();
  bool hasError();
  const char* getError();
};

extern LuaVM luaVM;

#endif
