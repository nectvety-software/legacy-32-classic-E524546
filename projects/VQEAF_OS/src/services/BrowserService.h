#pragma once
#include <Arduino.h>
#include "StorageService.h"

// Line styles from the Qeafbrowser v2.2 WML/HTML engine (wml.cpp heading_style).
// The OS Browser app may render them differently; unknown styles draw as body.
enum : uint8_t {
  BR_STYLE_BODY = 0,
  BR_STYLE_FIELD = 1,   // <input>/<field>
  BR_STYLE_H1 = 2,      // h1/h2
  BR_STYLE_FOLDER = 3,  // <folder>/<dir> list row
  BR_STYLE_H3 = 4,      // h3..h6
  BR_STYLE_SMALL = 5,   // small/cite/meta
  BR_STYLE_BOLD = 6,    // b/strong
  BR_STYLE_IMAGE = 7,   // image placeholder / alt
};

struct BrowserLine {
  char text[58];
  int8_t link;
  uint8_t style;
  uint16_t block;
  BrowserLine() : text{0}, link(-1), style(BR_STYLE_BODY), block(0) {}
};

struct BrowserLink {
  char url[192];
  char label[48];
  BrowserLink() : url{0}, label{0} {}
};

// In-OS adapter for the Qeafbrowser keypad-browser architecture. It keeps the
// same small-device principles: HTTP/HTTPS, redirects, fixed pools, no JS/CSS
// engine and no allocation in the input loop.
// v2.5.2 engine upgrade: Qeafbrowser v2.2 WML/HTML parser (styles, heading
// hierarchy, folder list rows, image alt/srcset), larger PSRAM line/link pools.
class BrowserService {
public:
  // Owns one fixed pool set; release it on host teardown. The firmware's
  // global BrowserService retains its pools for the entire OS lifetime.
  BrowserService() = default;
  ~BrowserService();
  BrowserService(const BrowserService &) = delete;
  BrowserService &operator=(const BrowserService &) = delete;
  static constexpr int MAX_LINES = 240;
  static constexpr int MAX_LINKS = 64;
  static constexpr int HISTORY_MAX = 16;

  bool begin(StorageService *storage = nullptr);
  bool available() const { return poolsReady; }
  bool load(const String &inputUrl);
  bool reload();
  bool goBack();
  bool openLink(int index);
  bool download(const String &inputUrl, String &savedPath, String &error);
  bool pageFromCache() const { return cachedPage; }

  const char *title() const { return pageTitle; }
  const char *url() const { return retryPending ? requestedUrl : currentUrl; }
  // Navigation failure keeps the requested URL so Reload retries the failing
  // address instead of unexpectedly opening the previous page.
  bool hasPendingRetry() const { return retryPending; }
  int status() const { return httpStatus; }
  const char *error() const { return errorText; }
  int lineCount() const { return lineUsed; }
  const BrowserLine &lineAt(int i) const { return lines[constrain(i, 0, max(0, lineUsed - 1))]; }
  int linkCount() const { return linkUsed; }
  const BrowserLink &linkAt(int i) const { return links[constrain(i, 0, max(0, linkUsed - 1))]; }

  static bool normalizeUrl(const String &input, char *out, size_t cap);
  static bool resolveUrl(const char *base, const char *href, char *out, size_t cap);

private:
  // Large browser pools are allocated once from PSRAM in begin() so adding the
  // browser does not permanently consume scarce internal DRAM/BSS.
  BrowserLine *lines = nullptr;
  BrowserLink *links = nullptr;
  char (*history)[192] = nullptr;
  bool poolsReady = false;
  StorageService *storage = nullptr;
  bool cachedPage = false;
  int historyUsed = 0;
  char currentUrl[192] = {0};
  char requestedUrl[192] = {0};
  bool retryPending = false;
  char pageTitle[64] = {0};
  char errorText[80] = {0};
  int httpStatus = 0;
  int lineUsed = 0;
  int linkUsed = 0;

  bool fetchAndParse(const char *url, bool pushHistory);
  void resetPage();
  void parseHtml(const char *src, size_t len);
  int addLink(const char *href, const char *label);
  void addWrappedText(const char *text, int linkIndex = -1,
                      uint8_t style = BR_STYLE_BODY, uint16_t block = 0);
  void pushHistory(const char *url);
  uint32_t cacheKey(const char *url) const;
  String cachePath(const char *url) const;
  bool loadCache(const char *url, char *body, size_t cap, size_t &used);
  void saveCache(const char *url, const char *body, size_t used);
  static String safeDownloadName(const char *url);
};
