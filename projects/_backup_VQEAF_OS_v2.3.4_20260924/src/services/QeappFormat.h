#pragma once
#include <stddef.h>
#include <stdint.h>

// Versioned, bounded on-disk format; see docs/QEAPP_V14.md.
// This module is freestanding C++11 and can be tested on the host.
namespace Qeapp {
static const size_t HEADER_BYTES = 116; // unchanged field offsets; QEAPP/2 has a signed trailer
static const uint32_t MAX_MANIFEST = 2048;
static const uint32_t MAX_PAYLOAD = 256 * 1024;
static const uint32_t ICON_BYTES = 32 * 32 * 2;
struct Header {
  uint32_t manifestLen, iconLen, payloadLen;
  uint8_t manifestHash[32], iconHash[32], payloadHash[32];
};
struct Meta {
  char id[25];
  char name[41];
  char version[20];
  char type[8]; // web, text
  char entry[193]; // HTTPS URL for web; unused for text
  bool hasIcon;
  Meta();
};
class Sha256 {
public:
  Sha256();
  void update(const uint8_t *data, size_t n);
  void finish(uint8_t out[32]);
private:
  uint32_t state[8];
  uint8_t block[64];
  size_t used;
  uint64_t processed;
  void transform();
};
bool parseHeader(const uint8_t bytes[HEADER_BYTES], uint64_t fileSize, Header &out, const char *&error);
bool parseManifest(const char *src, size_t n, Meta &out, const char *&error);
bool equalHash(const uint8_t a[32], const uint8_t b[32]);
} // namespace Qeapp
