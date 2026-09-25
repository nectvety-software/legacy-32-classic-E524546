#pragma once
#include <Arduino.h>
#include <FS.h>
#include "StorageService.h"
#include "../core/Theme.h"
#include "../launcher/LauncherTheme.h"

// VQEAF is a Theme Studio declarative format, not executable code.
// Only a bounded subset of theme metadata and palette colors is imported.
// Scan: microSD root + folders up to two levels deep. All arrays are fixed.
class ThemeFileService {
public:
  static constexpr int MAX_THEMES = 16;
  static constexpr size_t MAX_FILE_BYTES = 512 * 1024; // bounded Studio base64 themes
  struct Entry {
    char label[40];
    char path[120];
    uint32_t bytes;
    Entry() : label{0}, path{0}, bytes(0) {}
  };

  int scan(StorageService &storage);
  int count() const { return used; }
  bool hasCard() const { return cardPresent; }
  const Entry &at(int i) const { return entries[(i >= 0 && i < used) ? i : MAX_THEMES]; }
  int find(const String &path) const;
  bool load(StorageService &storage, const String &path, ThemeColors &out, String &name, String &error, LauncherStyle *skin = nullptr) const;

private:
  Entry entries[MAX_THEMES + 1]; // extra empty sentinel for out-of-bounds access
  int used = 0;
  bool cardPresent = false;
  static bool parse(File &file, ThemeColors &colors, String &name, String &error, LauncherStyle *skin);
};
