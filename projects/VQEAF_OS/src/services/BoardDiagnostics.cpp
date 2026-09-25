#include "BoardDiagnostics.h"
#include "StorageService.h"
#include "TrustedTls.h"
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

const char *BoardDiagnostics::label(Result r) {
  if (r == Result::Pass) return "PASS";
  if (r == Result::Fail) return "FAIL";
  return "INCONCLUSIVE";
}

static bool diagnosticHostValid(const String &host) {
  if (!host.length() || host.length() > 90 || host[0] == '-' || host[host.length()-1] == '-') return false;
  bool dot = false;
  for (size_t i = 0; i < host.length(); ++i) {
    const char ch = host[i];
    if (ch == '.') dot = true;
    else if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') || ch == '-')) return false;
  }
  return dot;
}

BoardDiagnostics::Result BoardDiagnostics::testTls(const String &target, bool expectedTrusted, String &detail) {
  detail = "";
  if (!diagnosticHostValid(target)) { detail = "Invalid host (use hostname only)"; return Result::Fail; }
  if (WiFi.status() != WL_CONNECTED) { detail = "Connect WiFi first"; return Result::Inconclusive; }
  if (!TrustedTls::timeValidAt(time(nullptr))) { detail = "Wait for real NTP time"; return Result::Inconclusive; }
  IPAddress resolved;
  if (WiFi.hostByName(target.c_str(), resolved) != 1) {
    detail = "DNS failed: no TLS conclusion";
    return Result::Inconclusive;
  }
  const uint32_t freeBefore = ESP.getFreeHeap();
  WiFiClientSecure client;
  String setupError;
  if (!TrustedTls::configure(client, setupError)) { detail = setupError; return Result::Inconclusive; }
  // WiFiClientSecure::connect(host, port) sets SNI and checks the requested
  // hostname as part of the TLS handshake with the compiled-in CA roots.
  const uint32_t t0 = millis();
  const bool connected = client.connect(target.c_str(), 443);
  const uint32_t duration = millis() - t0;
  if (connected) client.stop();
  const uint32_t freeAfter = ESP.getFreeHeap();
  int tlsCode = 0;
#if defined(ARDUINO_ARCH_ESP32)
  if (!connected) {
    char tlsDetail[96] = {0};
    tlsCode = client.lastError(tlsDetail, sizeof(tlsDetail));
    Serial.printf("[S3DIAG][TLS] mbedtls_error=%d detail=%s\n", tlsCode, tlsDetail);
  }
#endif
  Serial.printf("[S3DIAG][TLS] host=%s expected=%s dns=ok handshake=%s elapsed_ms=%lu heap_before=%lu heap_after=%lu\n",
                target.c_str(), expectedTrusted ? "accept" : "reject", connected ? "accept" : "reject",
                (unsigned long)duration, (unsigned long)freeBefore, (unsigned long)freeAfter);
  if (connected == expectedTrusted) {
    // Only a certificate-verification error (mbedTLS X509 -0x2700) is
    // evidence of rejected trust/hostname/expiry. Still run the positive
    // control to exclude a globally broken TLS stack or blocked internet.
    if (!expectedTrusted) {
      if (tlsCode == -0x2700) {
        detail = "Cert rejected (-0x2700); compare with valid control";
        return Result::Pass;
      }
      detail = "Connection rejected but not proven certificate-related";
      return Result::Inconclusive;
    }
    detail = String("Verified TLS connection in ") + duration + " ms";
    return Result::Pass;
  }
  if (connected && !expectedTrusted) {
    detail = "SECURITY FAILURE: TLS accepted a negative-case host";
    return Result::Fail;
  }
  detail = "Expected valid host rejected: inspect DNS, CA, time & network";
  return Result::Fail;
}

String BoardDiagnostics::sdStatus(const StorageService &storage) {
  return String(storage.mounted() ? "mounted" : "offline") +
         " errors=" + String(storage.ioErrors()) +
         " capacity_mb=" + String((unsigned long)storage.cardSizeMB());
}

BoardDiagnostics::Result BoardDiagnostics::testSdReadWrite(StorageService &storage, String &detail) {
  detail = "";
  if (!storage.mounted()) { detail = "SD not mounted"; return Result::Inconclusive; }
  // Never overwrite any file, even one named like our scratch file from a
  // previous aborted test. The operator must remove stale files manually.
  const char *path = "/System/Temp/.s3_diag_scratch.bin";
  if (!storage.ensureDir("/System/Temp")) { detail = "Cannot create Temp directory"; return Result::Fail; }
  if (storage.fs().exists(path)) {
    detail = "Scratch path exists; inspect and remove manually";
    return Result::Inconclusive;
  }
  File out = storage.fs().open(path, FILE_WRITE);
  if (!out) { detail = "Cannot open scratch file"; return Result::Fail; }
  uint8_t block[256];
  bool wrote = true;
  for (unsigned page = 0; page < 16; ++page) {
    for (unsigned i = 0; i < sizeof(block); ++i) {
      const uint32_t offset = page * (unsigned)sizeof(block) + i;
      block[i] = (uint8_t)((offset * 37UL + 91UL) & 0xff);
    }
    if (out.write(block, sizeof(block)) != sizeof(block)) { wrote = false; break; }
  }
  out.flush();
  out.close();
  if (!wrote) {
    // Do not attempt aggressive retries on a potentially removed card.
    detail = "Partial SD write; scratch retained for manual inspection";
    return Result::Fail;
  }
  File in = storage.fs().open(path, FILE_READ);
  if (!in) { detail = "Cannot reopen scratch; retained"; return Result::Fail; }
  bool valid = in.size() == 4096;
  uint32_t crc = 0xffffffffUL;
  uint8_t readback[256];
  for (unsigned page = 0; page < 16 && valid; ++page) {
    valid = in.read(readback, sizeof(readback)) == (int)sizeof(readback);
    for (unsigned i = 0; i < sizeof(readback) && valid; ++i) {
      const uint32_t offset = page * (unsigned)sizeof(readback) + i;
      const uint8_t value = readback[i];
      if (value != (uint8_t)((offset * 37UL + 91UL) & 0xff)) { valid = false; break; }
      crc ^= value;
      for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
    }
  }
  in.close();
  if (!valid) { detail = "Readback mismatch; scratch retained for inspection"; return Result::Fail; }
  if (!storage.fs().remove(path)) { detail = "Readback good, scratch deletion failed"; return Result::Fail; }
  char buf[88];
  snprintf(buf, sizeof(buf), "4096 bytes write/read verified; crc32=%08lx; scratch removed", (unsigned long)(crc ^ 0xffffffffUL));
  detail = buf;
  return Result::Pass;
}
