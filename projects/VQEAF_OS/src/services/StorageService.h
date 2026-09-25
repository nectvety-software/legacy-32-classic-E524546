#pragma once
#include <Arduino.h>
#include <FS.h>

namespace StoragePaths {
static const char * const SYSTEM = "/System";
static const char * const CACHE = "/System/Cache";
static const char * const CACHE_WEB = "/System/Cache/Web";
static const char * const CACHE_THUMBS = "/System/Cache/Thumbs";
static const char * const THEMES = "/System/Themes";
static const char * const APPS = "/System/Apps";
static const char * const APPS_INSTALLED = "/System/Apps/Installed";
static const char * const APPS_INBOX = "/System/Apps/Inbox";
static const char * const APPS_DATA = "/System/Apps/Data";
static const char * const DOWNLOADS = "/System/Downloads";
static const char * const LOGS = "/System/Logs";
static const char * const TEMP = "/System/Temp";
static const char * const MEDIA = "/Media";
static const char * const MUSIC = "/Media/Music";
static const char * const PICTURES = "/Media/Pictures";
static const char * const DOCUMENTS = "/Documents";
}

struct FsEntry {
  String name;
  String path;
  bool isDir;
  uint64_t size;

  FsEntry() : name(), path(), isDir(false), size(0) {}
  FsEntry(const String &entryName, const String &entryPath, bool directory, uint64_t entrySize)
      : name(entryName), path(entryPath), isDir(directory), size(entrySize) {}
};

class StorageService {
public:
  enum class CardEvent : uint8_t { None, Removed, Mounted };
  // Poll only from the main loop when no application holds a live SD file.
  // A missing CD pin means this is best-effort media probing, not instant hotplug.
  CardEvent tick(bool idle = true);
  CardEvent tick(bool idle, uint32_t now);
  bool mediaPresent();
  uint16_t ioErrors() const { return errors; }

  bool begin();
  bool ensureSystemLayout();
  bool ensureDir(const String &path);
  bool exists(const String &path) const;
  bool writeAtomic(const String &path, const uint8_t *data, size_t len);
  bool recoverAtomicFile(const String &path);
  uint64_t usedBytes() const;
  uint64_t totalBytes() const;
  uint64_t freeBytes() const;
  uint64_t directoryBytes(const String &path, int maxEntries = 64);
  int pruneFlatDirectory(const String &path, int maxFiles, uint64_t maxBytes);
  int clearFlatDirectory(const String &path, int maxRemovals = 128);
  bool mounted() const { return ok; }
  uint64_t cardSizeMB() const;
  int list(const String &path, FsEntry *out, int maxEntries);
  int scanMedia(const String &root, const char *const *extensions, int extCount, FsEntry *out, int maxEntries, int depth = 2);
  int countMedia(const String &root, const char *const *extensions, int extCount, int depth = 2, int limit = 999);
  bool remove(const String &path);
  fs::FS &fs();
private:
  bool ok = false;
  uint16_t errors = 0;
  uint32_t lastProbeAt = 0;
  uint32_t retryAt = 0;
  bool mountCard();
  int scanRecursive(const String &path, const char *const *extensions, int extCount, FsEntry *out, int maxEntries, int &count, int depth);
  int countRecursive(const String &path, const char *const *extensions, int extCount, int &count, int depth, int limit);
  bool matches(const String &name, const char *const *extensions, int extCount);
};
