#pragma once
#include <Arduino.h>
#include "StorageService.h"
#include "SystemService.h"

class NotificationService;
class WiFiConnectionService;

// BusyBox-inspired diagnostic shell for the ESP32-S3 firmware.
// Bounded by design: no fork/exec, no arbitrary binaries, no unbounded output.
class ShellService {
public:
  static constexpr int MAX_LINES = 20;
  static constexpr int LINE_CHARS = 39;
  static constexpr int HISTORY_MAX = 8;
  static constexpr int COMMAND_CHARS = 127;

  void begin(StorageService &storage, SystemService &system, WiFiConnectionService *wifi = nullptr);
  void clear();
  void execute(const String &command, NotificationService &notifications);

  int lineCount() const { return usedLines; }
  const char *lineAt(int index) const;
  String cwd() const { return currentDir; }

  int historyCount() const { return historyUsed; }
  String historyAt(int index) const;

  bool takeRebootRequest();
  uint64_t copiedBytes() const { return fileCopyBytes; }
  uint64_t downloadedBytes() const { return networkDownloadBytes; }

private:
  StorageService *storageService = nullptr;
  WiFiConnectionService *radioService = nullptr;
  SystemService *systemService = nullptr;
  char lines[MAX_LINES][LINE_CHARS + 1] = {{0}};
  char history[HISTORY_MAX][COMMAND_CHARS + 1] = {{0}};
  int usedLines = 0;
  int historyUsed = 0;
  String currentDir = "/";
  bool rebootRequested = false;
  uint64_t fileCopyBytes = 0;
  uint64_t networkDownloadBytes = 0;
  uint32_t lastNetworkActionMs = 0;

  void push(const String &text);
  void pushWrapped(const String &text);
  void remember(const String &command);
  String normalizePath(const String &path) const;
  bool changeDir(const String &path);

  // File commands.
  void commandLs(const String &path);
  void commandCat(const String &path);
  void commandStat(const String &path);
  void commandMkdir(const String &path);
  void commandRm(const String &path, bool directory);
  void commandTouch(const String &path);
  void commandCopy(const String &source, const String &dest, bool move);
  void commandHexdump(const String &path, uint32_t offset);
  void commandWrite(const String &path, const String &text, bool append);

  // Network and monitoring commands.
  void commandIfconfig();
  void commandNetmon();
  void commandNslookup(const String &host);
  void commandPing(const String &host, int count);
  void commandWget(const String &url, const String &path);
  void commandTop();
  void commandLayout();
  void commandCache(const String &action);

  void commandHelp();
  static String trimCopy(String value);
  static int splitArgs(const String &line, String *out, int maxArgs);
  static String joinArgs(String *argv, int argc, int first);
};
