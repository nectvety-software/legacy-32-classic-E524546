#pragma once
#include <Arduino.h>
#include "StorageService.h"
#include "QeappFormat.h"
#include "QeappSignature.h"
#include "QeappVersion.h"

// Purely declarative applications (HTTPS launchers + bundled text).
// No loading .vxp/.elf/.js or arbitrary downloaded native code.
class AppInstallerService {
public:
  static constexpr int MAX_INSTALLED = 12;
  static constexpr int MAX_INBOX = 12;
  typedef void (*ProgressCallback)(uint8_t percent,const char *phase,void *user);
  void setProgressCallback(ProgressCallback fn,void *user) {progressFn=fn;progressUser=user;}
  struct Installed {
    Qeapp::Meta info;
    char path[96];
    uint8_t verifiedIconHash[32] = {}; // from fully verified receipt at catalog scan
  };
  struct RecoveryStats { uint8_t restored, finalized, blocked, discardedStages; };
  static void printBootInstallDiagnostics();
  void begin(StorageService &storage) { card = &storage; refresh(); }
  // Safe to re-run after boot or SD reinsert; never removes unexpected files.
  RecoveryStats recoverTransactions();
  RecoveryStats recoveryStats() const { return recovery; }
  bool updateAvailable(const Qeapp::Meta &candidate) const;
  void refresh();
  void refreshIfNeeded() { if (!catalogReady) refresh(); }
  void invalidateCatalog() { catalogReady = false; used = 0; ++catalogRevision; }
  uint32_t revision() const { return catalogRevision; }
  int count() const { return used; }
  // Caller must check n < count(); no out-of-range read of uninitialized Meta.
  const Installed &at(int n) const { return installed[n >= 0 && n < used ? n : MAX_INSTALLED]; }
  bool get(const String &id, Qeapp::Meta &meta, String *why = nullptr) const;
  bool inspect(const String &pkg, Qeapp::Meta &meta, String &error);
  // Single full signature verification + optional bounded verified icon preview.
  bool inspectWithIcon(const String &pkg, Qeapp::Meta &meta, String &error,
                       uint16_t out[1024], bool &iconReady);
  // Installs new apps or replaces an already trusted, older version using
  // a staged copy plus signed backup. Downgrades and unsigned apps fail.
  bool install(const String &pkg, Qeapp::Meta &result, String &error);
  bool uninstall(const String &id, String &error);
  bool loadIcon(const String &id, uint16_t out[1024]);
  bool previewIcon(const String &pkg, uint16_t out[1024]);
  String installedPath(const String &id) const;
private:
  StorageService *card = nullptr;
  Installed installed[MAX_INSTALLED + 1] = {}; // final slot is zero-initialized sentinel
  int used = 0;
  bool catalogReady = false;
  uint32_t catalogRevision = 0;
  RecoveryStats recovery = {0, 0, 0, 0};
  ProgressCallback progressFn = nullptr;
  void *progressUser = nullptr;
  uint32_t progressCopied = 0, progressTotal = 0;
  uint8_t lastProgress = 0;
  void progress(uint8_t percent,const char *phase);
  bool scanPackage(File &f, Qeapp::Header &header, Qeapp::Meta &meta, String &error, bool verify);
  bool copySection(File &src, const String &dst, uint32_t bytes, const uint8_t hash[32], String &error);
  bool cleanKnownFiles(const String &dir);
  bool verifyInstalled(const String &id, Qeapp::Meta &meta, String &error) const;
  bool verifyDirectory(const String &dir, const String &id, Qeapp::Meta &meta, String &error) const;
  bool onlyKnownFiles(const String &dir) const;
};
