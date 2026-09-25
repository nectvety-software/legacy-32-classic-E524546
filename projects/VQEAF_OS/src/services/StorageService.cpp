#include "StorageService.h"
#include <SD_MMC.h>
#include "BoardConfig.h"

bool StorageService::mountCard() {
  // SD_MMC.begin() can return false after a partial init. The caller must
  // end() before re-trying; never remount while a WAV/file is open.
  ok = SD_MMC.begin("/sdcard", true, false, 20000, 8);
  if (!ok) return false;
  // open("/") confirms that FAT is actually readable, beyond stale cardSize().
  if (!mediaPresent()) { ok = false; return false; }
  if (!ensureSystemLayout()) {
    ++errors; // Mounted but read-only/broken layout: callers get explicit errors.
  }
  return true;
}

bool StorageService::begin() {
  SD_MMC.setPins(Board::SD_CLK, Board::SD_CMD, Board::SD_D0, -1, -1, Board::SD_D3);
  lastProbeAt = millis();
  retryAt = lastProbeAt + 15000UL;
  if (mountCard()) return true;
  ++errors;
  SD_MMC.end();
  return false;
}

bool StorageService::mediaPresent() {
  if (!ok || SD_MMC.cardType() == CARD_NONE || SD_MMC.cardSize() == 0) return false;
  File dir = SD_MMC.open("/", FILE_READ);
  const bool healthy = dir && dir.isDirectory();
  if (dir) dir.close();
  return healthy;
}

StorageService::CardEvent StorageService::tick(bool idle) { return tick(idle, millis()); }

StorageService::CardEvent StorageService::tick(bool idle, uint32_t now) {
  // Calls that hold open File handles (audio playback or paused WAV) suppress
  // probing and remount to avoid invalidating FAT handles under an application.
  if (!idle) return CardEvent::None;
  if (ok) {
    if ((uint32_t)(now - lastProbeAt) < 8000UL) return CardEvent::None;
    lastProbeAt = now;
    if (mediaPresent()) return CardEvent::None;
    ok = false;
    ++errors;
    retryAt = now + 5000UL; // give contacts/card power time to settle
    return CardEvent::Removed;
  }
  if ((int32_t)(now - retryAt) < 0) return CardEvent::None;
  retryAt = now + 15000UL; // bounded retries; don't spin in loop()
  SD_MMC.end();
  if (mountCard()) {
    lastProbeAt = now;
    return CardEvent::Mounted;
  }
  ++errors;
  SD_MMC.end();
  return CardEvent::None;
}

bool StorageService::ensureDir(const String &path) {
  if (!ok || !path.length() || path[0] != '/') return false;
  if (SD_MMC.exists(path)) {
    File f = SD_MMC.open(path);
    const bool isDir = f && f.isDirectory();
    if (f) f.close();
    return isDir;
  }
  return SD_MMC.mkdir(path);
}

bool StorageService::ensureSystemLayout() {
  if (!ok) return false;
  // Create parents before children: SD_MMC.mkdir() does not recursively create
  // missing ancestors on every Arduino-ESP32 build. The layout is intentionally
  // small and deterministic so every service has a stable home on the card.
  const char *paths[] = {
    StoragePaths::SYSTEM, StoragePaths::CACHE, StoragePaths::CACHE_WEB, StoragePaths::CACHE_THUMBS,
    StoragePaths::THEMES, StoragePaths::APPS, StoragePaths::APPS_INSTALLED, StoragePaths::APPS_INBOX, StoragePaths::APPS_DATA,
    StoragePaths::DOWNLOADS, StoragePaths::LOGS, StoragePaths::TEMP,
    StoragePaths::MEDIA, StoragePaths::MUSIC, StoragePaths::PICTURES, StoragePaths::DOCUMENTS,
    "/Themes" // legacy compatibility with v1.2 theme cards
  };
  bool all = true;
  for (const char *p : paths) if (!ensureDir(p)) all = false;
  return all;
}

bool StorageService::exists(const String &path) const {
  return ok && path.length() && SD_MMC.exists(path);
}

// FAT rename is not a hardware transaction. A .bak journal protects the last
// complete version if a reset interrupts a cache/theme metadata replacement.
// A caller must recover an individual file before reading it after reboot.
bool StorageService::recoverAtomicFile(const String &path) {
  if (!ok || !path.startsWith("/") || path.length() > 220 ||
      path.indexOf("..") >= 0 || path.indexOf('\\') >= 0) return false;
  String backup = path + ".bak";
  String pending = path + ".tmp";
  if (SD_MMC.exists(backup)) {
    if (!SD_MMC.exists(path)) {
      if (!SD_MMC.rename(backup, path)) return false;
    } else if (!SD_MMC.remove(backup)) return false;
  }
  // The temporary file is never considered valid by itself (it might be
  // partially written when power failed).
  if (SD_MMC.exists(pending) && !SD_MMC.remove(pending)) return false;
  return true;
}

bool StorageService::writeAtomic(const String &path, const uint8_t *data, size_t len) {
  if (!ok || !data || !len || path.length() > 220 || !path.startsWith("/") ||
      path.indexOf("..") >= 0 || path.indexOf('\\') >= 0 ||
      (freeBytes() && freeBytes() < len + 4096ULL) || !recoverAtomicFile(path)) return false;
  const String tmp = path + ".tmp";
  const String backup = path + ".bak";
  File f = SD_MMC.open(tmp, FILE_WRITE);
  if (!f) return false;
  const size_t wrote = f.write(data, len);
  f.flush();
  f.close();
  if (wrote != len) { ++errors; SD_MMC.remove(tmp); return false; }
  // Reopen and compare in fixed chunks. This detects truncated/changed files
  // before replacing the previous known-good version; it cannot guarantee
  // durability against a card lying about flush or a sudden FAT power loss.
  File check = SD_MMC.open(tmp, FILE_READ);
  bool verified = check && check.size() == len;
  uint8_t block[256];
  for (size_t offset = 0; verified && offset < len; offset += sizeof block) {
    const size_t n = min(sizeof block, len - offset);
    verified = check.read(block, n) == (int)n && memcmp(block, data + offset, n) == 0;
  }
  if (check) check.close();
  if (!verified) { ++errors; SD_MMC.remove(tmp); return false; }
  const bool existed = SD_MMC.exists(path);
  if (existed && !SD_MMC.rename(path, backup)) {
    SD_MMC.remove(tmp); return false;
  }
  if (!SD_MMC.rename(tmp, path)) {
    // Roll back the previous complete version if we can. If rollback itself
    // fails, leave .bak untouched so recoverAtomicFile can repair next boot.
    if (existed) SD_MMC.rename(backup, path);
    SD_MMC.remove(tmp);
    return false;
  }
  if (existed) SD_MMC.remove(backup);
  return true;
}

uint64_t StorageService::usedBytes() const { return ok ? SD_MMC.usedBytes() : 0; }
uint64_t StorageService::totalBytes() const { return ok ? SD_MMC.totalBytes() : 0; }
uint64_t StorageService::freeBytes() const {
  const uint64_t total = totalBytes(), used = usedBytes();
  return total > used ? total - used : 0;
}


uint64_t StorageService::directoryBytes(const String &path, int maxEntries) {
  if (!ok || maxEntries <= 0) return 0;
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) return 0;
  uint64_t total = 0; int count = 0;
  File f = dir.openNextFile();
  while (f && count++ < maxEntries) {
    if (!f.isDirectory()) total += f.size();
    f.close(); f = dir.openNextFile();
  }
  dir.close();
  return total;
}

int StorageService::pruneFlatDirectory(const String &path, int maxFiles, uint64_t maxBytes) {
  if (!ok || maxFiles < 1 || maxFiles > 64 || maxBytes < 1 ||
      (path != StoragePaths::CACHE_WEB && path != StoragePaths::CACHE_THUMBS)) return -1;
  int removed = 0;
  // Hard cap keeps an unexpected card containing thousands of files from
  // monopolizing the event loop. A negative return signals an unfinished prune.
  for (int pass = 0; pass < 64; ++pass) {
    File dir = SD_MMC.open(path, FILE_READ);
    if (!dir || !dir.isDirectory()) return -1;
    int count = 0, inspected = 0;
    uint64_t total = 0, biggest = 0;
    String victim;
    File entry = dir.openNextFile();
    while (entry && inspected++ < 128) {
      if (!entry.isDirectory()) {
        ++count; total += entry.size();
        if (entry.size() >= biggest) {
          biggest = entry.size();
          String n = String(entry.name());
          const int slash = n.lastIndexOf('/');
          if (slash >= 0) n = n.substring(slash + 1);
          // Never trust paths returned by drivers; act only on flat cache names.
          if (n.length() && n != "." && n != ".." && n.indexOf('/') < 0 &&
              n.indexOf('\\') < 0) victim = path + "/" + n;
        }
      }
      entry.close();
      entry = dir.openNextFile();
    }
    const bool capped = (bool)entry;
    if (entry) entry.close();
    dir.close();
    if (!capped && count <= maxFiles && total <= maxBytes) return removed;
    if (!victim.length() || !SD_MMC.remove(victim)) return -1;
    ++removed;
  }
  return -1;
}

// Clear the recognized cache directories without loading an array of paths
// into the task stack. Reopening after each deletion avoids FAT iterator skips.
int StorageService::clearFlatDirectory(const String &path, int maxRemovals) {
  if (!ok || (path != StoragePaths::CACHE_WEB && path != StoragePaths::CACHE_THUMBS) ||
      maxRemovals < 1 || maxRemovals > 128) return -1;
  int removed = 0;
  for (;;) {
    File dir = SD_MMC.open(path, FILE_READ);
    if (!dir || !dir.isDirectory()) return -1;
    File f = dir.openNextFile();
    String candidate;
    while (f) {
      if (!f.isDirectory()) {
        String name = String(f.name());
        const int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);
        if (name.length() && name != "." && name != ".." &&
            name.indexOf('/') < 0 && name.indexOf('\\') < 0) {
          candidate = path + "/" + name;
          f.close(); break;
        }
      }
      f.close(); f = dir.openNextFile();
    }
    if (f) f.close();
    dir.close();
    if (!candidate.length()) return removed;
    if (removed >= maxRemovals || !SD_MMC.remove(candidate)) return -1;
    ++removed;
  }
}

uint64_t StorageService::cardSizeMB() const {
  return ok ? SD_MMC.cardSize() / (1024ULL * 1024ULL) : 0;
}

fs::FS &StorageService::fs() { return SD_MMC; }

static String joinPath(const String &base, const String &name) {
  if (name.startsWith("/")) return name;
  if (base == "/") return "/" + name;
  return base + "/" + name;
}

int StorageService::list(const String &path, FsEntry *out, int maxEntries) {
  if (!ok || !out || maxEntries <= 0) return 0;
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) return 0;
  int count = 0;
  File f = dir.openNextFile();
  while (f && count < maxEntries) {
    String n = String(f.name());
    int slash = n.lastIndexOf('/');
    if (slash >= 0 && slash + 1 < (int)n.length()) n = n.substring(slash + 1);
    out[count].name = n;
    out[count].path = joinPath(path, n);
    out[count].isDir = f.isDirectory();
    out[count].size = f.size();
    count++;
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  // Directories first, then alphabetical; small N keeps O(n^2) acceptable.
  for (int i = 0; i < count - 1; ++i) {
    for (int j = i + 1; j < count; ++j) {
      bool swap = false;
      if (out[i].isDir != out[j].isDir) swap = !out[i].isDir && out[j].isDir;
      else swap = out[i].name.compareTo(out[j].name) > 0;
      if (swap) { FsEntry t = out[i]; out[i] = out[j]; out[j] = t; }
    }
  }
  return count;
}

bool StorageService::matches(const String &name, const char *const *extensions, int extCount) {
  String lower = name;
  lower.toLowerCase();
  for (int i = 0; i < extCount; ++i) if (lower.endsWith(extensions[i])) return true;
  return false;
}

int StorageService::scanRecursive(const String &path, const char *const *extensions, int extCount, FsEntry *out, int maxEntries, int &count, int depth) {
  if (depth < 0 || count >= maxEntries) return count;
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) return count;
  File f = dir.openNextFile();
  while (f && count < maxEntries) {
    String full = String(f.name());
    String n = full;
    int slash = n.lastIndexOf('/');
    if (slash >= 0 && slash + 1 < (int)n.length()) n = n.substring(slash + 1);
    String normalized = joinPath(path, n);
    if (f.isDirectory() && depth > 0) {
      f.close();
      scanRecursive(normalized, extensions, extCount, out, maxEntries, count, depth - 1);
    } else if (!f.isDirectory() && matches(n, extensions, extCount)) {
      out[count++] = FsEntry(n, normalized, false, f.size());
      f.close();
    } else {
      f.close();
    }
    f = dir.openNextFile();
  }
  dir.close();
  return count;
}

int StorageService::scanMedia(const String &root, const char *const *extensions, int extCount, FsEntry *out, int maxEntries, int depth) {
  if (!ok) return 0;
  int count = 0;
  return scanRecursive(root, extensions, extCount, out, maxEntries, count, depth);
}


int StorageService::countRecursive(const String &path, const char *const *extensions, int extCount,
                                   int &count, int depth, int limit) {
  if (depth < 0 || count >= limit) return count;
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) return count;
  File f = dir.openNextFile();
  while (f && count < limit) {
    String full = String(f.name());
    String n = full;
    int slash = n.lastIndexOf('/');
    if (slash >= 0 && slash + 1 < (int)n.length()) n = n.substring(slash + 1);
    if (f.isDirectory() && depth > 0) {
      String next = joinPath(path, n);
      f.close();
      countRecursive(next, extensions, extCount, count, depth - 1, limit);
    } else {
      if (!f.isDirectory() && matches(n, extensions, extCount)) ++count;
      f.close();
    }
    f = dir.openNextFile();
  }
  dir.close();
  return count;
}

int StorageService::countMedia(const String &root, const char *const *extensions, int extCount, int depth, int limit) {
  if (!ok || limit <= 0) return 0;
  int count = 0;
  return countRecursive(root, extensions, extCount, count, depth, limit);
}

bool StorageService::remove(const String &path) {
  if (!ok) return false;
  return SD_MMC.remove(path);
}
