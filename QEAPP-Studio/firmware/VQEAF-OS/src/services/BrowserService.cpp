#include "BrowserService.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "TrustedTls.h"
#include "HttpChunkedDecoder.h"
#include <esp_heap_caps.h>
#include <ctype.h>
#include <new>

constexpr int BrowserService::MAX_LINES;
constexpr int BrowserService::MAX_LINKS;
constexpr int BrowserService::HISTORY_MAX;

static void trimAscii(char *s) {
  if (!s) return;
  char *p = s;
  while (*p && isspace((unsigned char)*p)) ++p;
  if (p != s) memmove(s, p, strlen(p) + 1);
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

// URLs are kept in fixed 192-byte buffers. Reject overflow instead of silently
// truncating the host/path (which could fetch a different resource).
static bool copyUrl(char *out, size_t cap, const char *value) {
  if (!out || !value || strlen(value) >= cap) return false;
  strcpy(out, value);
  return true;
}
static bool hasScheme(const char *s) {
  return s && (strncasecmp(s, "http://", 7) == 0 || strncasecmp(s, "https://", 8) == 0);
}
static bool validWebUrl(const char *s) {
  if (!hasScheme(s)) return false;
  const char *host = strstr(s, "://") + 3;
  if (!*host || *host == '/' || *host == '?' || *host == '#') return false;
  for (const unsigned char *p = (const unsigned char *)s; *p; ++p)
    if (*p <= ' ' || *p == 0x7f || *p == '\\' || *p == '@') return false;
  return true;
}

bool BrowserService::normalizeUrl(const String &input, char *out, size_t cap) {
  if (!out || cap < 12) return false;
  out[0] = 0;
  String candidate = input;
  candidate.trim();
  if (!candidate.length()) return false;
  if (!hasScheme(candidate.c_str())) {
    // Never interpret javascript:, file:, etc. as a web host.
    if (candidate.indexOf("://") >= 0) return false;
    if (!candidate.startsWith("//")) {
      // A numeric host port is valid (example.org:8443), but javascript:
      // and other URI schemes must not be treated as host names.
      const char *raw = candidate.c_str();
      const char *end = strpbrk(raw, "/?#");
      if (!end) end = raw + strlen(raw);
      const char *colon = (const char *)memchr(raw, ':', end - raw);
      if (colon) {
        bool validPort = (memchr(raw, '.', colon - raw) != nullptr ||
                          ((size_t)(colon - raw) == 9 && !strncmp(raw, "localhost", 9)));
        if (colon + 1 == end || end - colon > 6) validPort = false;
        for (const char *p = colon + 1; p < end; ++p)
          if (!isdigit((unsigned char)*p)) validPort = false;
        if (!validPort) return false;
      }
    }
    if (candidate.startsWith("//")) candidate = String("https:") + candidate;
    else candidate = String("https://") + candidate;
  }
  return validWebUrl(candidate.c_str()) && copyUrl(out, cap, candidate.c_str());
}

static bool splitBase(const char *base, char *scheme, size_t sc, char *host,
                      size_t hc, char *path, size_t pc) {
  if (!base || !validWebUrl(base)) return false;
  const char *sep = strstr(base, "://");
  if (!sep) return false;
  const size_t sl = (size_t)(sep - base);
  if (!sl || sl >= sc) return false;
  memcpy(scheme, base, sl); scheme[sl] = 0;
  const char *begin = sep + 3;
  const char *end = begin;
  while (*end && *end != '/' && *end != '?' && *end != '#') ++end;
  size_t hl = (size_t)(end - begin);
  if (!hl || hl >= hc) return false;
  memcpy(host, begin, hl); host[hl] = 0;
  // A domain-only URL can carry a query: https://host/?q is the canonical path.
  if (*end == '?') {
    if (strlen(end) + 1 >= pc) return false;
    path[0] = '/'; strcpy(path + 1, end);
  } else if (*end == '/') {
    if (!copyUrl(path, pc, end)) return false;
  } else strcpy(path, "/");
  char *frag = strchr(path, '#'); if (frag) *frag = 0;
  return true;
}

bool BrowserService::resolveUrl(const char *base, const char *href, char *out, size_t cap) {
  if (!href || !href[0] || !out || cap < 12) return false;
  out[0] = 0;
  if (hasScheme(href)) return validWebUrl(href) && copyUrl(out, cap, href);
  // Never treat another scheme (javascript:, data:, file:) as a relative link.
  if (strncmp(href, "//", 2) != 0) {
    const char *firstDelimiter = strpbrk(href, "/?#");
    if (!firstDelimiter) firstDelimiter = href + strlen(href);
    if (memchr(href, ':', firstDelimiter - href)) return false;
  }
  for (const unsigned char *p = (const unsigned char *)href; *p; ++p)
    if (*p <= ' ' || *p == 0x7f || *p == '\\') return false;
  char scheme[8], host[96], path[192];
  if (!splitBase(base, scheme, sizeof(scheme), host, sizeof(host), path, sizeof(path))) return false;
  if (!strncmp(href, "//", 2)) {
    char absolute[384];
    int n = snprintf(absolute, sizeof(absolute), "%s:%s", scheme, href);
    return n > 0 && (size_t)n < sizeof(absolute) && validWebUrl(absolute) && copyUrl(out, cap, absolute);
  }
  if (href[0] == '#') {
    // An in-document anchor does not need another HTTP request.
    return copyUrl(out, cap, base);
  }
  if (href[0] == '?') {
    char *q = strchr(path, '?'); if (q) *q = 0;
    char absolute[384];
    int n = snprintf(absolute, sizeof(absolute), "%s://%s%s%s", scheme, host, path, href);
    return n > 0 && (size_t)n < sizeof(absolute) && copyUrl(out, cap, absolute);
  }
  // Preserve the query and fragment separately: dot-segment processing should
  // touch only the pathname, not the query string.
  char combined[384];
  if (href[0] == '/') {
    if (strlen(href) >= sizeof(combined)) return false;
    strcpy(combined, href);
  } else {
    char *query = strchr(path, '?'); if (query) *query = 0;
    char *slash = strrchr(path, '/');
    if (slash) slash[1] = 0; else strcpy(path, "/");
    int n = snprintf(combined, sizeof(combined), "%s%s", path, href);
    if (n < 0 || (size_t)n >= sizeof(combined)) return false;
  }
  char suffix[192] = {0};
  char *startSuffix = strpbrk(combined, "?#");
  if (startSuffix) {
    if (!copyUrl(suffix, sizeof(suffix), startSuffix)) return false;
    *startSuffix = 0;
  }
  const size_t combinedLen = strlen(combined);
  const bool hadTrailingSlash = combinedLen > 0 && combined[combinedLen-1] == '/';
  char normalized[192] = "";
  char *parts[32]; int used = 0;
  char *save = nullptr;
  for (char *part = strtok_r(combined, "/", &save); part;
       part = strtok_r(nullptr, "/", &save)) {
    if (!strcmp(part, ".")) continue;
    if (!strcmp(part, "..")) { if (used) --used; continue; }
    if (used >= 32) return false;
    parts[used++] = part;
  }
  for (int i = 0; i < used; ++i) {
    size_t current = strlen(normalized), size = strlen(parts[i]);
    if (current + size + 2 >= sizeof(normalized)) return false;
    normalized[current++] = '/'; memcpy(normalized + current, parts[i], size + 1);
  }
  if (!normalized[0]) strcpy(normalized, "/");
  // Keep trailing '/' in a directory link after canonicalization.
  if (hadTrailingSlash &&
      normalized[strlen(normalized)-1] != '/') strcat(normalized, "/");
  char absolute[384];
  int n = snprintf(absolute, sizeof(absolute), "%s://%s%s%s",
                   scheme, host, normalized, suffix);
  return n > 0 && (size_t)n < sizeof(absolute) && copyUrl(out, cap, absolute);
}

bool BrowserService::begin(StorageService *storageRef) {
  storage = storageRef;
  if (!poolsReady) {
    lines = (BrowserLine*)heap_caps_malloc(sizeof(BrowserLine) * MAX_LINES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    links = (BrowserLink*)heap_caps_malloc(sizeof(BrowserLink) * MAX_LINKS, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    history = (char (*)[192])heap_caps_malloc(sizeof(char[192]) * HISTORY_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // Fallback keeps the browser usable on boards where PSRAM init failed, but
    // Safe Mode can still be used if internal memory becomes constrained.
    if (!lines) lines = (BrowserLine*)malloc(sizeof(BrowserLine) * MAX_LINES);
    if (!links) links = (BrowserLink*)malloc(sizeof(BrowserLink) * MAX_LINKS);
    if (!history) history = (char (*)[192])malloc(sizeof(char[192]) * HISTORY_MAX);
    if (!lines || !links || !history) {
      if (lines) free(lines);
      if (links) free(links);
      if (history) free(history);
      lines = nullptr; links = nullptr; history = nullptr; poolsReady = false;
      snprintf(errorText, sizeof(errorText), "Browser memory unavailable");
      return false;
    }
    for (int i = 0; i < MAX_LINES; ++i) new (&lines[i]) BrowserLine();
    for (int i = 0; i < MAX_LINKS; ++i) new (&links[i]) BrowserLink();
    memset(history, 0, sizeof(char[192]) * HISTORY_MAX);
    poolsReady = true;
  }
  resetPage();
  cachedPage = false;
  historyUsed = 0; requestedUrl[0] = 0; retryPending = false;
  snprintf(currentUrl, sizeof(currentUrl), "%s", "https://qeafivels.com/");
  return true;
}

void BrowserService::resetPage() {
  lineUsed = 0;
  linkUsed = 0;
  httpStatus = 0;
  pageTitle[0] = 0;
  errorText[0] = 0;
  cachedPage = false;
  if (lines) for (int i = 0; i < MAX_LINES; ++i) { lines[i].text[0] = 0; lines[i].link = -1; }
  if (links) for (int i = 0; i < MAX_LINKS; ++i) { links[i].url[0] = 0; links[i].label[0] = 0; }
}


uint32_t BrowserService::cacheKey(const char *url) const {
  uint32_t h = 2166136261UL;
  if (!url) return h;
  for (const unsigned char *p = (const unsigned char *)url; *p; ++p) {
    h ^= *p;
    h *= 16777619UL;
  }
  return h;
}

String BrowserService::cachePath(const char *url) const {
  char name[24];
  const bool secure = url && !strncasecmp(url, "https://", 8);
  snprintf(name, sizeof(name), secure ? "tls20_%08lX.htm" : "plain20_%08lX.htm",
           (unsigned long)cacheKey(url));
  return String(StoragePaths::CACHE_WEB) + "/" + name;
}

bool BrowserService::loadCache(const char *url, char *body, size_t cap, size_t &used) {
  used = 0;
  if (!storage || !storage->mounted() || !body || cap < 2) return false;
  const String path = cachePath(url);
  if (!storage->recoverAtomicFile(path)) return false;
  File f = storage->fs().open(path, FILE_READ);
  if (!f || f.isDirectory()) { if (f) f.close(); return false; }
  const size_t expected = (size_t)f.size();
  if (!expected || expected >= cap) { f.close(); return false; }
  used = f.readBytes(body, expected);
  body[used] = 0;
  f.close();
  // Do not silently render a partially read offline page after an SD error.
  if (used != expected) { used = 0; body[0] = 0; return false; }
  cachedPage = true;
  if (cachedPage) snprintf(errorText, sizeof(errorText), "Offline cache");
  return cachedPage;
}

void BrowserService::saveCache(const char *url, const char *body, size_t used) {
  if (!storage || !storage->mounted() || !body || used == 0 || used > 32767) return;
  storage->ensureDir(StoragePaths::CACHE_WEB);
  if (storage->writeAtomic(cachePath(url), (const uint8_t *)body, used))
    storage->pruneFlatDirectory(StoragePaths::CACHE_WEB, 16, 512UL * 1024UL);
}

String BrowserService::safeDownloadName(const char *url) {
  String name = url ? String(url) : String();
  int q = name.indexOf('?'); if (q >= 0) name.remove(q);
  int h = name.indexOf('#'); if (h >= 0) name.remove(h);
  int slash = name.lastIndexOf('/'); if (slash >= 0) name = name.substring(slash + 1);
  if (!name.length()) name = "download.bin";
  String clean;
  clean.reserve(min(64, (int)name.length()));
  for (size_t i = 0; i < name.length() && clean.length() < 63; ++i) {
    char c = name[i];
    if (isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_') clean += c;
    else clean += '_';
  }
  if (!clean.length() || clean == "." || clean == "..") clean = "download.bin";
  return clean;
}

bool BrowserService::download(const String &inputUrl, String &savedPath, String &error) {
  savedPath = ""; error = "";
  if (!storage || !storage->mounted()) { error = "microSD is required"; return false; }
  if (WiFi.status() != WL_CONNECTED) { error = "WiFi is not connected"; return false; }

  char url[192];
  if (!normalizeUrl(inputUrl, url, sizeof(url))) { error = "Invalid download URL"; return false; }
  // Never deliver installable content over plaintext or allow a downgrade on
  // any redirect. HTTP pages may still be browsed outside the downloader.
  if (strncasecmp(url, "https://", 8)) { error = "Downloads require verified HTTPS"; return false; }
  String fileName = safeDownloadName(url);
  String lower = fileName; lower.toLowerCase();
  const char *folder = StoragePaths::DOWNLOADS;
  if (lower.endsWith(".vqeaf")) folder = StoragePaths::THEMES;
  else if (lower.endsWith(".qeapp") || lower.endsWith(".zip") || lower.endsWith(".vxp")) folder = StoragePaths::APPS_INBOX;
  storage->ensureDir(folder);
  savedPath = String(folder) + "/" + fileName;
  // Repeated downloads must not silently overwrite a theme or signed app
  // already saved on removable storage.
  if (storage->exists(savedPath)) {
    error = "File already exists; rename it in File manager first";
    savedPath = "";
    return false;
  }
  String tmpPath = savedPath + ".part";
  storage->fs().remove(tmpPath);

  char requestUrl[192]; snprintf(requestUrl, sizeof(requestUrl), "%s", url);
  char nextUrl[192];
  for (int hop = 0; hop < 6; ++hop) {
    HTTPClient http;
    http.setConnectTimeout(9000); http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    const char *transferHeaders[]={"Transfer-Encoding"};
    http.collectHeaders(transferHeaders,1);
    http.setUserAgent("Opera/9.80 (J2ME/MIDP; Opera Mini/4.5; U; vi) Qeafbrowser-VQEAF/2.1");
    if (strncasecmp(requestUrl, "https://", 8)) {
      error = "Unsafe redirect: HTTPS required"; break;
    }
    WiFiClientSecure secure;
    if (!TrustedTls::configure(secure, error)) break;
    bool begun = http.begin(secure, requestUrl);
    if (!begun) { error = "Cannot open download"; break; }
    http.addHeader("Accept-Encoding", "identity");
    int code = http.GET();
    if (code >= 300 && code < 400) {
      String location = http.getLocation();
      if (!location.length() || !resolveUrl(requestUrl, location.c_str(), nextUrl, sizeof(nextUrl))) {
        error = "Bad redirect"; http.end(); break;
      }
      snprintf(requestUrl, sizeof(requestUrl), "%s", nextUrl); http.end(); continue;
    }
    if (code <= 0) { error = "HTTPS verification or connection failed"; http.end(); break; }
    if (code < 200 || code >= 300) { error = String("HTTP ") + code; http.end(); break; }
    const int len = http.getSize();
    static const int32_t MAX_DOWNLOAD = 4 * 1024 * 1024;
    String transfer = http.header("Transfer-Encoding");
    transfer.toLowerCase();
    const bool chunked=transfer.indexOf("chunked")>=0;
    if (len <= 0 && !chunked) {
      error = "Download needs Content-Length or chunked transfer";
      http.end(); break;
    }
    if (len > MAX_DOWNLOAD && !chunked) {error = "File exceeds 4 MB limit"; http.end(); break;}
    File out = storage->fs().open(tmpPath, FILE_WRITE);
    if (!out) { error = "Cannot create download file"; http.end(); break; }
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[512]; int32_t total = 0;
    const uint32_t responseStart=millis();uint32_t lastData=responseStart;
    bool failed = false;
    if (chunked) {
      // Stream chunks through a 512-byte stack buffer into a .part file;
      // never allocate the whole app (up to 4 MB) in PSRAM or trust an
      // unknown-size HTTP response. Hash/signature is checked by installer.
      bool timedOut=false;
      auto readByte=[&]()->int {
        while(true) {
          const uint32_t now=millis();
          if((uint32_t)(now-responseStart)>180000UL ||
             (uint32_t)(now-lastData)>10000UL) {timedOut=true;return -1;}
          if(stream->available()) {
            int c=stream->read();
            if(c>=0) {lastData=millis();return c;}
          }
          if(!http.connected())return -1;
          delay(1);
        }
      };
      auto sink=[&](const uint8_t *bytes,size_t n)->bool {
        if(!storage->mounted())return false;
        return out.write(bytes,n)==n;
      };
      uint32_t decoded=0;const char *frameError="";
      if(!HttpChunkedDecoder::decodeTo(readByte,sink,MAX_DOWNLOAD,decoded,frameError)) {
        failed=true;
        error=timedOut ? "Download timed out" : frameError;
      }
      total=(int32_t)decoded;
    }else{
      // Declared-length transfers: never publish a partial `.qeapp` or
      // `.vqeaf`. Drain buffered bytes even after peer closed TCP.
      while ((http.connected() || stream->available()) && total<len) {
        const uint32_t now=millis();
        if((uint32_t)(now-responseStart)>180000UL ||
           (uint32_t)(now-lastData)>10000UL){error="Download timed out";failed=true;break;}
        size_t avail=stream->available();
        if(!avail) {delay(1);continue;}
        size_t want=min(avail,sizeof(buf));
        if(total+(int32_t)want>len)want=len-total;
        int n=stream->readBytes(buf,want);
        if(n<=0)continue;
        if(total+n>MAX_DOWNLOAD || out.write(buf,n)!=(size_t)n) {
          error="Download write or size limit failed";failed=true;break;
        }
        total+=n;lastData=millis();
      }
    }
    out.flush(); out.close(); http.end();
    if (!storage->mounted()) { failed = true; error = "microSD was removed"; }
    if (failed || total <= 0 || (!chunked && total != len)) {
      storage->fs().remove(tmpPath);
      if (!error.length()) error = failed ? "Download write failed" : "Download incomplete";
      break;
    }
    // Reopen the staging file before publishing it. An SD error during flush
    // must not make a truncated app/theme look like a finished download.
    File staged = storage->fs().open(tmpPath, FILE_READ);
    const bool stagedOkay = staged && staged.size() == (size_t)total;
    if (staged) staged.close();
    if (!stagedOkay) {
      storage->fs().remove(tmpPath); error = "Download SD readback failed"; break;
    }
    if (storage->fs().exists(savedPath)) {
      storage->fs().remove(tmpPath);
      error = "Destination appeared during download";
      break;
    }
    if (!storage->fs().rename(tmpPath, savedPath)) {
      storage->fs().remove(tmpPath); error = "Cannot finalize download"; break;
    }
    return true;
  }
  storage->fs().remove(tmpPath);
  savedPath = "";
  if (!error.length()) error = "Download failed";
  return false;
}

void BrowserService::pushHistory(const char *url) {
  if (!history || !url || !url[0]) return;
  if (historyUsed && !strcmp(history[0], url)) return;
  int last = min(historyUsed, HISTORY_MAX - 1);
  for (int i = last; i > 0; --i) snprintf(history[i], sizeof(history[i]), "%s", history[i - 1]);
  snprintf(history[0], sizeof(history[0]), "%s", url);
  if (historyUsed < HISTORY_MAX) ++historyUsed;
}

bool BrowserService::load(const String &inputUrl) {
  char normalized[192];
  if (!normalizeUrl(inputUrl, normalized, sizeof(normalized))) {
    snprintf(errorText, sizeof(errorText), "Invalid address");
    return false;
  }
  snprintf(requestedUrl, sizeof(requestedUrl), "%s", normalized);
  retryPending = true;
  return fetchAndParse(normalized, true);
}

bool BrowserService::reload() {
  if (retryPending && requestedUrl[0]) return fetchAndParse(requestedUrl, true);
  if (!currentUrl[0]) return load("https://qeafivels.com/");
  return fetchAndParse(currentUrl, false);
}

bool BrowserService::goBack() {
  if (historyUsed < 2) return false;
  char target[192]; snprintf(target, sizeof(target), "%s", history[1]);
  // Leave history untouched on transport/TLS errors. The user can retry.
  if (!fetchAndParse(target, false)) return false;
  for (int i = 1; i < historyUsed - 1; ++i) snprintf(history[i], sizeof(history[i]), "%s", history[i + 1]);
  --historyUsed;
  return true;
}

bool BrowserService::openLink(int index) {
  if (index < 0 || index >= linkUsed) return false;
  return fetchAndParse(links[index].url, true);
}

bool BrowserService::fetchAndParse(const char *url, bool addHistory) {
  // Capture link URLs *before* resetPage() clears the fixed link pool. A
  // clicked link points into links[index].url, so using that original pointer
  // after resetPage() produced an empty/invalid request on the next fetch.
  if (!url || !url[0] || strlen(url) >= sizeof(requestedUrl)) {
    snprintf(errorText, sizeof(errorText), "Invalid address");
    return false;
  }
  char targetUrl[192];
  snprintf(targetUrl, sizeof(targetUrl), "%s", url);
  snprintf(requestedUrl, sizeof(requestedUrl), "%s", targetUrl);
  retryPending = true;
  url = targetUrl;
  resetPage();
  if (!poolsReady || !lines || !links || !history) {
    snprintf(errorText, sizeof(errorText), "Browser memory unavailable");
    return false;
  }
  constexpr size_t BODY_CAP = 32768;

  if (WiFi.status() != WL_CONNECTED) {
    char *cached = (char*)heap_caps_malloc(BODY_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!cached) cached = (char*)malloc(BODY_CAP);
    size_t cachedLen = 0;
    if (cached && loadCache(url, cached, BODY_CAP, cachedLen)) {
      snprintf(currentUrl, sizeof(currentUrl), "%s", url);
      if (addHistory) pushHistory(currentUrl);
      retryPending = false;
      parseHtml(cached, cachedLen);
      free(cached); httpStatus = 200; cachedPage = true;
      if (!pageTitle[0]) snprintf(pageTitle, sizeof(pageTitle), "%s", "Cached page");
      return true;
    }
    if (cached) free(cached);
    snprintf(errorText, sizeof(errorText), "WiFi is not connected");
    return false;
  }
  char *body = (char*)heap_caps_malloc(BODY_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!body) body = (char*)malloc(BODY_CAP);
  if (!body) {
    snprintf(errorText, sizeof(errorText), "Not enough memory");
    return false;
  }

  char requestUrl[192];
  char nextUrl[192];
  snprintf(requestUrl, sizeof(requestUrl), "%s", url);
  bool success = false;
  bool allowOfflineCache = true;
  size_t used = 0;

  // Qeafbrowser-style explicit redirect loop. Keeping redirect handling here is
  // important because relative links must resolve against the final URL, not
  // the address typed before a 301/302 hop.
  for (int hop = 0; hop < 6; ++hop) {
    HTTPClient http;
    http.setConnectTimeout(9000);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    const char *wantedHeaders[] = {"Content-Type", "Transfer-Encoding"};
    http.collectHeaders(wantedHeaders, 2);
    http.setUserAgent("Opera/9.80 (J2ME/MIDP; Opera Mini/4.5; U; vi) Qeafbrowser-VQEAF/2.1");

    WiFiClient plain;
    WiFiClientSecure secure;
    String errorTextBuffer;
    const bool tls = !strncasecmp(requestUrl, "https://", 8);
    if (tls && !TrustedTls::configure(secure, errorTextBuffer)) {
      // An untrusted date/CA failure must never be hidden by an old cached page.
      snprintf(errorText, sizeof(errorText), "%s", errorTextBuffer.c_str());
      allowOfflineCache = false;
      break;
    }
    bool begun = tls ? http.begin(secure, requestUrl) : http.begin(plain, requestUrl);
    if (!begun) {
      snprintf(errorText, sizeof(errorText), "%s", tls ?
               "HTTPS setup/verification failed" : "Cannot open address");
      if (tls) allowOfflineCache = false;
      break;
    }

    http.addHeader("Accept", "text/html,application/xhtml+xml,application/vnd.wap.xhtml+xml,text/plain;q=0.8,*/*;q=0.2");
    http.addHeader("Accept-Encoding", "identity");
    http.addHeader("Accept-Language", "vi,en;q=0.8");

    httpStatus = http.GET();
    if (httpStatus <= 0) {
      snprintf(errorText, sizeof(errorText), "%s", tls ?
               "HTTPS verification or network failed" : "HTTP request failed");
      if (tls) allowOfflineCache = false;
      http.end();
      break;
    }

    if (httpStatus >= 300 && httpStatus < 400) {
      String location = http.getLocation();
      if (!location.length() || !resolveUrl(requestUrl, location.c_str(), nextUrl, sizeof(nextUrl))) {
        snprintf(errorText, sizeof(errorText), "Bad redirect");
        http.end();
        break;
      }
      if (tls && strncasecmp(nextUrl, "https://", 8)) {
        snprintf(errorText, sizeof(errorText), "Unsafe HTTPS downgrade blocked");
        allowOfflineCache = false;
        http.end(); break;
      }
      http.end();
      snprintf(requestUrl, sizeof(requestUrl), "%s", nextUrl);
      if (retryPending) snprintf(requestedUrl, sizeof(requestedUrl), "%s", requestUrl);
      if (hop == 5) snprintf(errorText, sizeof(errorText), "Too many redirects");
      continue;
    }

    if (httpStatus < 200 || httpStatus >= 300) {
      snprintf(errorText, sizeof(errorText), "HTTP status %d", httpStatus);
      allowOfflineCache = false; // A real 404 must not be replaced by an old cache.
      http.end();
      break;
    }
    const int declaredBytes = http.getSize();
    if (declaredBytes >= (int)BODY_CAP) {
      snprintf(errorText, sizeof(errorText), "Page exceeds 32 KB limit");
      http.end();
      break;
    }
    String ctype = http.header("Content-Type");
    ctype.toLowerCase();
    if (ctype.length() && ctype.indexOf("text/") < 0 && ctype.indexOf("html") < 0 &&
        ctype.indexOf("xml") < 0 && ctype.indexOf("wap") < 0) {
      snprintf(errorText, sizeof(errorText), "Unsupported content type");
      http.end();
      break;
    }

    WiFiClient *stream = http.getStreamPtr();
    used = 0;
    const uint32_t responseStart = millis();
    uint32_t lastData = responseStart;
    bool timedOut = false;
    String transfer = http.header("Transfer-Encoding");
    transfer.toLowerCase();
    if (transfer.indexOf("chunked") >= 0) {
      // HTTPClient::getStreamPtr returns the underlying raw TCP stream on
      // Arduino-ESP32; a chunked body must be de-framed before HTML parsing.
      // The parser streams directly into the existing 32-KiB PSRAM buffer.
      bool timedOutDuringChunk = false;
      auto readByte = [&]() -> int {
        while (true) {
          const uint32_t now = millis();
          if ((uint32_t)(now-responseStart) > 25000UL ||
              (uint32_t)(now-lastData) > 8000UL) {
            timedOutDuringChunk = true; return -1;
          }
          if (stream->available()) {
            int c = stream->read();
            if (c >= 0) {lastData = millis(); return c;}
          }
          if (!http.connected()) return -1;
          delay(1);
        }
      };
      const char *decodeError = nullptr;
      const bool decoded = HttpChunkedDecoder::decode(readByte, body, BODY_CAP, used, decodeError);
      if (!decoded) {
        snprintf(errorText, sizeof(errorText), "%s", timedOutDuringChunk ?
                 "Chunked response timeout" : decodeError);
        http.end(); break;
      }
    } else {
      // HTTP/1.0 close-delimited and declared Content-Length bodies.
      while ((http.connected() || stream->available()) && used < BODY_CAP - 1 &&
             (declaredBytes < 0 || used < (size_t)declaredBytes)) {
        if ((uint32_t)(millis() - responseStart) > 25000UL ||
            (uint32_t)(millis() - lastData) > 8000UL) {
          timedOut = true;
          break;
        }
        size_t avail = stream->available();
        if (avail) {
          size_t room = BODY_CAP - 1 - used;
          size_t want = avail < room ? avail : room;
          int n = stream->readBytes(body + used, want);
          if (n > 0) { used += (size_t)n; lastData = millis(); }
        } else delay(1);
      }
      body[used] = 0;
    }
    http.end();
    if (timedOut) {
      snprintf(errorText, sizeof(errorText), "Page request timeout");
      break;
    }
    if (!used) {
      snprintf(errorText, sizeof(errorText), "%s", "Empty HTML response");
      break;
    }
    if (transfer.indexOf("chunked") >= 0) {success = true; break;}
    if (timedOut || (declaredBytes >= 0 && used != (size_t)declaredBytes) ||
        used == BODY_CAP - 1) {
      snprintf(errorText, sizeof(errorText), "%s", timedOut ? "Page request timeout" : "Page truncated");
      break;
    }
    success = true;
    break;
  }

  if (!success) {
    size_t cachedLen = 0;
    if (allowOfflineCache && loadCache(url, body, BODY_CAP, cachedLen)) {
      snprintf(currentUrl, sizeof(currentUrl), "%s", url);
      if (addHistory) pushHistory(currentUrl);
      retryPending = false;
      parseHtml(body, cachedLen);
      free(body); httpStatus = 200; cachedPage = true;
      if (!pageTitle[0]) snprintf(pageTitle, sizeof(pageTitle), "%s", "Cached page");
      return true;
    }
    free(body);
    return false;
  }

  snprintf(currentUrl, sizeof(currentUrl), "%s", requestUrl);
  if (addHistory) pushHistory(currentUrl);
  retryPending = false;
  saveCache(currentUrl, body, used);
  const bool needsJavaScript = strstr(body, "<script") != nullptr;
  parseHtml(body, used);
  free(body);

  if (!pageTitle[0]) snprintf(pageTitle, sizeof(pageTitle), "%s", "Web page");
  if (!lineUsed) addWrappedText(needsJavaScript ?
      "This page requires JavaScript; Qeafbrowser supports HTML text only." :
      "Page contains no displayable text.");
  return httpStatus >= 200 && httpStatus < 300;
}

int BrowserService::addLink(const char *href, const char *label) {
  if (!href || !href[0] || linkUsed >= MAX_LINKS) return -1;
  char absUrl[192];
  if (!resolveUrl(currentUrl, href, absUrl, sizeof(absUrl))) return -1;
  int i = linkUsed++;
  snprintf(links[i].url, sizeof(links[i].url), "%s", absUrl);
  snprintf(links[i].label, sizeof(links[i].label), "%.*s",
           int(sizeof(links[i].label)-1), (label && label[0]) ? label : absUrl);
  return i;
}

void BrowserService::addWrappedText(const char *text, int linkIndex) {
  if (!text || !text[0] || lineUsed >= MAX_LINES) return;
  char clean[192]; int c = 0; bool space = false;
  for (const char *p = text; *p && c < (int)sizeof(clean)-1; ++p) {
    unsigned char ch = (unsigned char)*p;
    if (isspace(ch)) { space = c > 0; continue; }
    if (space && c < (int)sizeof(clean)-1) clean[c++] = ' ';
    space = false;
    clean[c++] = (char)ch;
  }
  clean[c] = 0;
  trimAscii(clean);
  if (!clean[0]) return;

  const int WRAP = 34;
  const char *p = clean;
  while (*p && lineUsed < MAX_LINES) {
    int n = min((int)strlen(p), WRAP);
    if (p[n] && n == WRAP) {
      int cut = n;
      while (cut > 12 && p[cut] != ' ') --cut;
      if (cut > 12) n = cut;
    }
    while (n > 0 && p[n-1] == ' ') --n;
    BrowserLine &ln = lines[lineUsed++];
    int copy = min(n, (int)sizeof(ln.text)-1);
    memcpy(ln.text, p, copy); ln.text[copy] = 0; ln.link = (int8_t)linkIndex;
    p += n;
    while (*p == ' ') ++p;
  }
}

static void decodeEntities(char *s) {
  struct Pair { const char *a; const char *b; } pairs[] = {
    {"&amp;","&"},{"&lt;","<"},{"&gt;",">"},{"&quot;","\""},{"&#39;","'"},{"&nbsp;"," "}
  };
  for (const auto &pair : pairs) {
    char out[192]; out[0] = 0; const char *p = s;
    while (*p && strlen(out) < sizeof(out)-2) {
      const char *m = strstr(p, pair.a);
      if (!m) { strncat(out, p, sizeof(out)-strlen(out)-1); break; }
      strncat(out, p, min((size_t)(m-p), sizeof(out)-strlen(out)-1));
      strncat(out, pair.b, sizeof(out)-strlen(out)-1);
      p = m + strlen(pair.a);
    }
    snprintf(s, 192, "%s", out);
  }
}

static bool htmlAttr(const char *raw, const char *wanted, char *out, size_t cap) {
  if (!raw || !wanted || !out || cap < 2) return false;
  out[0] = 0;
  const char *p = raw;
  while (*p) {
    while (*p && (isspace((unsigned char)*p) || *p == '/' || *p == '>')) ++p;
    if (!*p) break;
    const char *ks = p;
    while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>' && *p != '/') ++p;
    const char *ke = p;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != '=') continue;
    ++p;
    while (*p && isspace((unsigned char)*p)) ++p;
    char quote = (*p == '\'' || *p == '"') ? *p++ : 0;
    const char *vs = p;
    if (quote) while (*p && *p != quote) ++p;
    else while (*p && !isspace((unsigned char)*p) && *p != '>') ++p;
    const char *ve = p;
    if (quote && *p == quote) ++p;

    size_t kl = (size_t)(ke - ks);
    if (strlen(wanted) == kl && strncasecmp(ks, wanted, kl) == 0) {
      size_t vl = (size_t)(ve - vs);
      if (vl >= cap) vl = cap - 1;
      memcpy(out, vs, vl); out[vl] = 0;
      return vl > 0;
    }
  }
  return false;
}

void BrowserService::parseHtml(const char *src, size_t len) {
  bool inTag = false, skip = false, inTitle = false, inAnchor = false;
  char tag[256] = {0}; int tagN = 0;
  char text[192] = {0}; int textN = 0;
  char anchorHref[192] = {0};
  char anchorText[192] = {0}; int anchorN = 0;

  auto flushText = [&]() {
    if (!textN) return;
    text[textN] = 0; decodeEntities(text);
    if (inTitle) {
      trimAscii(text);
      if (text[0] && !pageTitle[0]) snprintf(pageTitle, sizeof(pageTitle), "%s", text);
    } else if (!skip && !inAnchor) addWrappedText(text);
    else if (inAnchor && anchorN < (int)sizeof(anchorText)-1) {
      for (int i=0;i<textN && anchorN < (int)sizeof(anchorText)-1;++i) anchorText[anchorN++] = text[i];
      anchorText[anchorN] = 0;
    }
    textN = 0; text[0] = 0;
  };

  auto processTag = [&](const char *raw) {
    char lower[256]; snprintf(lower, sizeof(lower), "%s", raw);
    for (char *p=lower; *p; ++p) *p = (char)tolower((unsigned char)*p);
    char *t = lower; while (*t && isspace((unsigned char)*t)) ++t;
    bool closing = *t == '/'; if (closing) ++t;
    while (*t && isspace((unsigned char)*t)) ++t;
    char name[20] = {0}; int n=0;
    while (*t && !isspace((unsigned char)*t) && *t!='>' && *t!='/' && n<19) name[n++]=*t++;
    name[n]=0;

    if (!strcmp(name,"script") || !strcmp(name,"style") || !strcmp(name,"noscript") || !strcmp(name,"svg")) {
      skip = !closing; return;
    }
    if (skip) return;
    if (!strcmp(name,"title")) { inTitle = !closing; return; }
    if (!strcmp(name,"a")) {
      if (!closing) {
        inAnchor = true; anchorN = 0; anchorText[0] = 0; anchorHref[0] = 0;
        htmlAttr(raw, "href", anchorHref, sizeof(anchorHref));
      } else {
        inAnchor = false; anchorText[anchorN]=0; decodeEntities(anchorText); trimAscii(anchorText);
        int li = addLink(anchorHref, anchorText);
        if (anchorText[0]) addWrappedText(anchorText, li);
        anchorN=0; anchorHref[0]=0;
      }
      return;
    }
    if (!closing && !strcmp(name,"img")) {
      char alt[96];
      if (htmlAttr(raw, "alt", alt, sizeof(alt)) && alt[0]) {
        decodeEntities(alt);
        addWrappedText(alt);
      }
      return;
    }
    if (!closing && (!strcmp(name,"br") || !strcmp(name,"p") || !strcmp(name,"div") || !strcmp(name,"li") || !strncmp(name,"h",1))) {
      if (lineUsed < MAX_LINES && lineUsed && lines[lineUsed-1].text[0]) {
        // Visual spacing without creating a large DOM.
      }
    }
  };

  for (size_t i=0;i<len;++i) {
    char ch = src[i];
    if (!inTag && ch=='<') { flushText(); inTag=true; tagN=0; continue; }
    if (inTag) {
      if (ch=='>') { tag[tagN]=0; processTag(tag); inTag=false; tagN=0; }
      else if (tagN < (int)sizeof(tag)-1) tag[tagN++]=ch;
      continue;
    }
    if (textN < (int)sizeof(text)-1) text[textN++]=ch;
    else flushText();
  }
  flushText();
  if (inAnchor && anchorN) {
    anchorText[anchorN]=0; int li=addLink(anchorHref, anchorText); addWrappedText(anchorText, li);
  }
}
