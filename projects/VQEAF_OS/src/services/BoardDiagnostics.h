#pragma once
#include <Arduino.h>

class StorageService;

// Interactive bench checks. THESE ARE NOT BOOT-TIME SELF TESTS. Tests are
// manually triggered via serial (diag ...) or on-screen Shell (tlsdiag/sddiag).
// No test disables TLS verification, modifies existing user files or
// deliberately ejects a mounted card while a FAT File is open.
class BoardDiagnostics {
public:
  enum class Result : uint8_t { Pass, Fail, Inconclusive };
  static const char *label(Result result);
  static Result testTls(const String &target, bool expectedTrusted, String &detail);
  static Result testSdReadWrite(StorageService &storage, String &detail);
  static String sdStatus(const StorageService &storage);
};
