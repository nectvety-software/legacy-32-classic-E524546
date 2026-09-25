#pragma once
#include "StorageService.h"
#include "AppInstallerService.h"

// Per-application sandboxed persistent data for future QEAPP runtimes.
// Not exposed as a general-purpose writable filesystem to web/text packages.
// Survives signed updates and uninstalls until the owner explicitly purges it.
class QeappDataService {
public:
  enum class Slot : uint8_t { Prefs, State, Draft };
  static const size_t MAX_SLOT_BYTES = 16 * 1024;
  static const size_t MAX_APP_BYTES = 32 * 1024;
  void begin(StorageService &store, AppInstallerService &installed) {
    card = &store; apps = &installed;
  }
  bool save(const String &id, Slot slot, const uint8_t *data, size_t size, String &error);
  bool load(const String &id, Slot slot, uint8_t *buffer, size_t capacity, size_t &used, String &error);
  // Explicit user action only; no automatic purge on uninstall/upgrade.
  bool purge(const String &id, String &error);
private:
  StorageService *card = nullptr;
  AppInstallerService *apps = nullptr;
  bool authorized(const String &id, String &error) const;
  String directory(const String &id) const;
  const char *filename(Slot slot) const;
};
