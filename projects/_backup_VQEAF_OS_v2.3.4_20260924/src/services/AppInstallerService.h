#pragma once
#include <Arduino.h>
#include "StorageService.h"
#include "QeappFormat.h"
#include "QeappSignature.h"

// Purely declarative applications (HTTPS launchers + bundled text).
// No loading .vxp/.elf/.js or arbitrary downloaded native code.
class AppInstallerService {
public:
  static constexpr int MAX_INSTALLED = 12;
  static constexpr int MAX_INBOX = 12;
  struct Installed {
    Qeapp::Meta info;
    char path[96];
  };
  void begin(StorageService &storage) { card = &storage; refresh(); }
  void refresh();
  int count() const { return used; }
  // Caller must check n < count(); no out-of-range read of uninitialized Meta.
  const Installed &at(int n) const { return installed[n >= 0 && n < used ? n : MAX_INSTALLED]; }
  bool get(const String &id, Qeapp::Meta &meta) const;
  bool inspect(const String &pkg, Qeapp::Meta &meta, String &error);
  bool install(const String &pkg, Qeapp::Meta &result, String &error);
  bool uninstall(const String &id, String &error);
  bool loadIcon(const String &id, uint16_t out[1024]);
  bool previewIcon(const String &pkg, uint16_t out[1024]);
  String installedPath(const String &id) const;
private:
  StorageService *card = nullptr;
  Installed installed[MAX_INSTALLED + 1] = {}; // final slot is zero-initialized sentinel
  int used = 0;
  bool scanPackage(File &f, Qeapp::Header &header, Qeapp::Meta &meta, String &error, bool verify);
  bool copySection(File &src, const String &dst, uint32_t bytes, const uint8_t hash[32], String &error);
  bool cleanKnownFiles(const String &dir);
  bool verifyInstalled(const String &id, Qeapp::Meta &meta, String &error) const;
};
