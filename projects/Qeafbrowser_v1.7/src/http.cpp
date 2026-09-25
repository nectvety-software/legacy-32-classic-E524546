// http.cpp — HTTP/HTTPS client cho Qeafbrowser.
// Muc tieu: mot browser nho kieu Qeafbrowser/Opera Mini tren ESP32-S3, khong chi UI mock.
// Ho tro HTTP/1.1, TLS, redirect, chunked, cookie, URL relative va body gioi han PSRAM.
#include "browser.h"
#include <WiFi.h>
#include <WiFiClient.h>
#if __has_include(<WiFiClientSecure.h>)
  #include <WiFiClientSecure.h>
  #define ESP_BROWSER_HAS_TLS 1
#else
  #define ESP_BROWSER_HAS_TLS 0
#endif

extern "C" {
#include <string.h>
#include <strings.h>
}

static bool ci_contains(const char *hay, const char *needle) {
  if (!hay || !needle) return false;
  size_t nl = strlen(needle);
  for (const char *p = hay; *p; p++) {
    size_t k = 0;
    while (k < nl && p[k] && tolower((unsigned char)p[k]) == tolower((unsigned char)needle[k])) k++;
    if (k == nl) return true;
  }
  return false;
}

static void trim_ascii(char *s) {
  if (!s) return;
  char *p = s;
  while (*p && isspace((unsigned char)*p)) p++;
  if (p != s) memmove(s, p, strlen(p) + 1);
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

// ---------------- URL helpers ----------------
bool url_split(const char *url, char *scheme, char *host, int *port, char *path) {
  if (!url || !scheme || !host || !port || !path) return false;
  const char *p = strstr(url, "://");
  if (!p) return false;
  size_t sl = (size_t)(p - url); if (sl == 0 || sl >= 8) return false;
  memcpy(scheme, url, sl); scheme[sl] = 0;
  for (char *q = scheme; *q; q++) *q = (char)tolower((unsigned char)*q);
  if (strcmp(scheme, "http") && strcmp(scheme, "https")) return false;

  p += 3;
  const char *end = url + strlen(url);
  const char *frag = strchr(p, '#');
  if (frag && frag < end) end = frag;
  const char *slash = (const char *)memchr(p, '/', (size_t)(end - p));
  const char *query = (const char *)memchr(p, '?', (size_t)(end - p));
  const char *hend = end;
  if (slash && slash < hend) hend = slash;
  if (query && query < hend) hend = query;

  // IPv6 literal [::1] duoc parse host, port tuy chon sau ']'.
  const char *colon = nullptr;
  if (p < hend && *p == '[') {
    const char *rb = (const char *)memchr(p, ']', (size_t)(hend - p));
    if (!rb) return false;
    if (rb + 1 < hend && rb[1] == ':') colon = rb + 1;
  } else {
    for (const char *q = p; q < hend; q++) if (*q == ':') colon = q;
  }

  const char *host_end = colon ? colon : hend;
  size_t hl = (size_t)(host_end - p);
  if (hl == 0 || hl >= 64) return false;
  memcpy(host, p, hl); host[hl] = 0;

  if (colon) {
    int v = atoi(colon + 1);
    if (v <= 0 || v > 65535) return false;
    *port = v;
  } else {
    *port = !strcmp(scheme, "https") ? 443 : 80;
  }

  const char *ps = slash ? slash : (query ? query : nullptr);
  if (!ps) strcpy(path, "/");
  else {
    size_t pl = (size_t)(end - ps);
    if (pl >= 192) pl = 191;
    if (*ps == '?') {
      path[0] = '/';
      memcpy(path + 1, ps, pl);
      path[pl + 1] = 0;
    } else {
      memcpy(path, ps, pl); path[pl] = 0;
    }
  }
  return host[0] != 0;
}

bool url_is_mtt(const char *url) { return url && !strncmp(url, "mtt:", 4); }

static bool has_uri_scheme(const char *s) {
  if (!s || !isalpha((unsigned char)s[0])) return false;
  for (const char *p = s + 1; *p; p++) {
    if (*p == ':') return true;
    if (!(isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.')) return false;
  }
  return false;
}

bool url_normalize_input(const char *input, char *out, size_t cap) {
  if (!input || !out || cap < 2) return false;
  char tmp[256];
  strncpy(tmp, input, sizeof tmp - 1); tmp[sizeof tmp - 1] = 0;
  trim_ascii(tmp);
  if (!tmp[0]) { out[0] = 0; return false; }

  if (!strncasecmp(tmp, "mtt:", 4) || !strncasecmp(tmp, "http://", 7) ||
      !strncasecmp(tmp, "https://", 8)) {
    // chi con scheme (prefill https:// ma user GO rong) -> xem nhu rong
    if (!strcmp(tmp, "http://") || !strcmp(tmp, "https://")) { out[0] = 0; return false; }
    strncpy(out, tmp, cap - 1); out[cap - 1] = 0; return true;
  }
  if (!strncmp(tmp, "//", 2)) {
    snprintf(out, cap, "https:%s", tmp); return true;
  }
  // Qeafbrowser cu thuong chap nhan "example.com". Ban moi uu tien HTTPS.
  snprintf(out, cap, "https://%s", tmp);
  return true;
}

static void remove_dot_segments(char *path) {
  // In-place, chi xu ly path (khong query). Gioi han nho nen dung stack offset tinh.
  char query[192] = "";
  char *q = strchr(path, '?');
  if (q) { strncpy(query, q, sizeof query - 1); query[sizeof query - 1] = 0; *q = 0; }

  char src[192]; strncpy(src, path, sizeof src - 1); src[sizeof src - 1] = 0;
  const bool abs = src[0] == '/';
  const bool trailing = strlen(src) > 1 && src[strlen(src) - 1] == '/';
  char *parts[32]; int n = 0;
  char *p = src;
  while (*p) {
    while (*p == '/') p++;
    if (!*p) break;
    char *s = p;
    while (*p && *p != '/') p++;
    if (*p) *p++ = 0;
    if (!strcmp(s, ".")) continue;
    if (!strcmp(s, "..")) { if (n > 0) n--; continue; }
    if (n < 32) parts[n++] = s;
  }
  size_t o = 0;
  if (abs && o < 191) path[o++] = '/';
  for (int i = 0; i < n; i++) {
    if (i && o < 191) path[o++] = '/';
    size_t l = strlen(parts[i]); if (o + l > 191) l = 191 - o;
    memcpy(path + o, parts[i], l); o += l;
  }
  if (trailing && o && path[o - 1] != '/' && o < 191) path[o++] = '/';
  if (o == 0) path[o++] = '/';
  path[o] = 0;
  strncat(path, query, 191 - strlen(path));
}

bool url_resolve(const char *base, const char *href, char *out, size_t cap) {
  if (!href || !out || cap < 2) return false;
  out[0] = 0;
  if (!href[0]) {
    if (base) { strncpy(out, base, cap - 1); out[cap - 1] = 0; return true; }
    return false;
  }
  if (!strncmp(href, "mtt:", 4) || has_uri_scheme(href)) {
    strncpy(out, href, cap - 1); out[cap - 1] = 0; return true;
  }
  if (!base || !strstr(base, "://")) {
    strncpy(out, href, cap - 1); out[cap - 1] = 0; return true;
  }

  char scheme[8], host[64], bpath[192]; int port = 0;
  if (!url_split(base, scheme, host, &port, bpath)) return false;
  char authority[96];
  bool default_port = (!strcmp(scheme, "https") && port == 443) || (!strcmp(scheme, "http") && port == 80);
  if (default_port) snprintf(authority, sizeof authority, "%s://%s", scheme, host);
  else snprintf(authority, sizeof authority, "%s://%s:%d", scheme, host, port);

  if (!strncmp(href, "//", 2)) {
    snprintf(out, cap, "%s:%s", scheme, href); return true;
  }
  if (href[0] == '#') {
    char nofrag[256]; strncpy(nofrag, base, sizeof nofrag - 1); nofrag[sizeof nofrag - 1] = 0;
    char *f = strchr(nofrag, '#'); if (f) *f = 0;
    snprintf(out, cap, "%s%s", nofrag, href); return true;
  }
  if (href[0] == '?') {
    char p[192]; strncpy(p, bpath, sizeof p - 1); p[sizeof p - 1] = 0;
    char *q = strchr(p, '?'); if (q) *q = 0;
    snprintf(out, cap, "%s%s%s", authority, p, href); return true;
  }

  char path[192];
  if (href[0] == '/') {
    strncpy(path, href, sizeof path - 1); path[sizeof path - 1] = 0;
  } else {
    char dir[192]; strncpy(dir, bpath, sizeof dir - 1); dir[sizeof dir - 1] = 0;
    char *q = strchr(dir, '?'); if (q) *q = 0;
    char *slash = strrchr(dir, '/');
    if (slash) slash[1] = 0; else strcpy(dir, "/");
    snprintf(path, sizeof path, "%s%s", dir, href);
  }
  remove_dot_segments(path);
  snprintf(out, cap, "%s%s", authority, path);
  return true;
}

void url_encode_query(const char *src, char *out, size_t cap) {
  static const char H[] = "0123456789ABCDEF";
  if (!out || cap == 0) return;
  size_t o = 0; out[0] = 0;
  if (!src) return;
  for (const unsigned char *p = (const unsigned char *)src; *p && o + 1 < cap; p++) {
    unsigned char c = *p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[o++] = (char)c;
    else if (c == ' ') out[o++] = '+';
    else if (o + 3 < cap) { out[o++] = '%'; out[o++] = H[c >> 4]; out[o++] = H[c & 15]; }
    else break;
  }
  out[o] = 0;
}

// ---------------- stream readers ----------------
static HttpProgressFn s_progress = nullptr;
void http_set_progress(HttpProgressFn fn) { s_progress = fn; }

template <typename TClient>
static bool read_line(TClient &c, char *out, int cap, uint32_t timeout_ms) {
  int n = 0;
  uint32_t t0 = millis();
  while (n < cap - 1) {
    if (millis() - t0 > timeout_ms) return false;
    if (!c.available()) {
      if (!c.connected()) break;
      delay(1); continue;
    }
    char ch = (char)c.read();
    if (ch == '\n') break;
    if (ch == '\r') continue;
    out[n++] = ch;
    t0 = millis();
  }
  out[n] = 0;
  return true;
}

template <typename TClient>
static bool read_chunked(TClient &c, char *buf, size_t cap, size_t *out_len,
                         bool *truncated, uint32_t tmo) {
  size_t total = 0;          // stored body bytes
  long wire = 0;             // payload bytes for progress
  bool any = false;
  uint32_t t0 = millis();
  for (;;) {
    if (millis() - t0 > tmo) break;
    char hex[32];
    if (!read_line(c, hex, sizeof hex, tmo)) break;
    char *semi = strchr(hex, ';'); if (semi) *semi = 0;
    long sz = strtol(hex, nullptr, 16);
    if (sz < 0) break;
    if (sz == 0) {
      // consume trailers
      static char trailer[256];
      do { if (!read_line(c, trailer, sizeof trailer, 2000)) break; } while (trailer[0]);
      break;
    }
    any = true;
    long remain = sz;
    while (remain > 0) {
      if (!c.available()) {
        if (!c.connected()) break;
        if (millis() - t0 > tmo) break;
        delay(1); continue;
      }
      char ch = (char)c.read(); remain--; t0 = millis();
      wire++;
      if (total < cap - 1) buf[total++] = ch;
      else if (truncated) *truncated = true;
    }
    // CRLF sau chunk
    char nl[4]; read_line(c, nl, sizeof nl, 2000);
    if (remain > 0) break;
    if (s_progress) s_progress(wire, -1);
  }
  buf[total] = 0; *out_len = total;
  if (s_progress) s_progress(wire, -1);
  return any || total == 0;
}

template <typename TClient>
static bool read_fixed(TClient &c, char *buf, size_t cap, size_t *out_len,
                       long content_len, bool *truncated, uint32_t tmo) {
  size_t total = 0;
  uint32_t t0 = millis();
  long got_wire = 0;
  for (;;) {
    if (content_len >= 0 && got_wire >= content_len) break;
    if (millis() - t0 > tmo) break;
    int avail = c.available();
    if (!avail) {
      if (!c.connected()) break;
      delay(1); continue;
    }
    while (avail-- > 0 && (content_len < 0 || got_wire < content_len)) {
      char ch = (char)c.read(); got_wire++;
      if (total < cap - 1) buf[total++] = ch;
      else if (truncated) *truncated = true;
    }
    t0 = millis();
    if (s_progress) s_progress(got_wire, content_len);
  }
  buf[total] = 0; *out_len = total;
  if (content_len > (long)(cap - 1) && truncated) *truncated = true;
  if (s_progress) s_progress(got_wire, content_len);
  return content_len == 0 || total > 0;
}

template <typename TClient>
static bool perform_request(TClient &client, const char *url, const char *scheme,
                            const char *host, int port, const char *path,
                            char *buf, size_t cap, size_t *out_len, HttpMeta *meta) {
  (void)url;
  static char host_hdr[96];
  bool default_port = (!strcmp(scheme, "https") && port == 443) || (!strcmp(scheme, "http") && port == 80);
  if (default_port) snprintf(host_hdr, sizeof host_hdr, "%s", host);
  else snprintf(host_hdr, sizeof host_hdr, "%s:%d", host, port);

  client.print("GET "); client.print(path); client.print(" HTTP/1.1\r\n");
  client.print("Host: "); client.print(host_hdr); client.print("\r\n");
  client.print("User-Agent: Opera/9.80 (J2ME/MIDP; Opera Mini/4.5.33867/191.323; U; en) Presto/2.12.423 Version/12.16 Qeafbrowser-ESP/1.3\r\n");
  client.print("Accept: text/html, application/xhtml+xml, application/vnd.wap.xhtml+xml, text/vnd.wap.wml, application/xml;q=0.9, text/plain;q=0.8, */*;q=0.2\r\n");
  client.print("Accept-Encoding: identity\r\n");
  client.print("Accept-Language: en-US,en;q=0.9\r\n");
  client.print("Accept-Charset: utf-8,*;q=0.7\r\n");
  client.print("Cache-Control: no-cache\r\n");
  static char ck[256]; cookie_get(host, ck, sizeof ck);
  if (ck[0]) { client.print("Cookie: "); client.print(ck); client.print("\r\n"); }
  client.print("Connection: close\r\n\r\n");

  static char line[512];
  if (!read_line(client, line, sizeof line, 12000) || strncmp(line, "HTTP/", 5)) {
    meta->status = -3; return false;
  }
  const char *sp = strchr(line, ' ');
  meta->status = sp ? atoi(sp + 1) : 0;
  meta->redirect[0] = 0;
  meta->content_type[0] = 0;
  meta->content_length = -1;
  meta->gzipped = false;
  meta->truncated = false;
  bool chunked = false;

  while (read_line(client, line, sizeof line, 10000)) {
    if (!line[0]) break;
    if (!strncasecmp(line, "Content-Length:", 15)) meta->content_length = atol(line + 15);
    else if (!strncasecmp(line, "Transfer-Encoding:", 18) && ci_contains(line + 18, "chunked")) chunked = true;
    else if (!strncasecmp(line, "Location:", 9)) {
      const char *v = line + 9; while (*v && isspace((unsigned char)*v)) v++;
      strncpy(meta->redirect, v, sizeof meta->redirect - 1); meta->redirect[sizeof meta->redirect - 1] = 0;
      trim_ascii(meta->redirect);
    } else if (!strncasecmp(line, "Content-Encoding:", 17) &&
               (ci_contains(line + 17, "gzip") || ci_contains(line + 17, "br"))) {
      meta->gzipped = true;
    } else if (!strncasecmp(line, "Content-Type:", 13)) {
      const char *v = line + 13; while (*v && isspace((unsigned char)*v)) v++;
      strncpy(meta->content_type, v, sizeof meta->content_type - 1);
      meta->content_type[sizeof meta->content_type - 1] = 0;
      char *semi = strchr(meta->content_type, ';'); if (semi) *semi = 0;
      trim_ascii(meta->content_type);
    } else if (!strncasecmp(line, "Set-Cookie:", 11)) {
      const char *v = line + 11; while (*v && isspace((unsigned char)*v)) v++;
      cookie_set(host, v);
    }
  }

  if ((meta->status == 204 || meta->status == 304) && meta->content_length <= 0) {
    if (cap) buf[0] = 0; *out_len = 0; return true;
  }
  if (chunked) return read_chunked(client, buf, cap, out_len, &meta->truncated, 20000);
  return read_fixed(client, buf, cap, out_len, meta->content_length, &meta->truncated, 20000);
}

static bool http_get_once(const char *url, char *buf, size_t cap, size_t *out_len, HttpMeta *meta) {
  static char scheme[8], host[64], path[192]; int port = 0;
  if (!url_split(url, scheme, host, &port, path)) { meta->status = -4; return false; }
  meta->secure = !strcmp(scheme, "https");

  if (!strcmp(scheme, "https")) {
#if ESP_BROWSER_HAS_TLS
    WiFiClientSecure client;
    // ESP32 khong co RTC/CA store mac dinh on dinh. setInsecure cho phep truy cap web that;
    // co the thay bang setCACert() neu firmware duoc cap root CA.
    client.setInsecure();
    client.setHandshakeTimeout(10);
    if (!client.connect(host, port)) { meta->status = -2; return false; }
    bool ok = perform_request(client, url, scheme, host, port, path, buf, cap, out_len, meta);
    client.stop(); return ok;
#else
    // Simulator Win32 cu chi co TCP; giu build test logic. HTTPS thuc te duoc test tren ESP32.
    meta->status = -5;
    return false;
#endif
  }

  WiFiClient client;
  if (!client.connect(host, port, 8000)) { meta->status = -2; return false; }
  bool ok = perform_request(client, url, scheme, host, port, path, buf, cap, out_len, meta);
  client.stop(); return ok;
}

bool http_get(const char *url, char *buf, size_t cap, size_t *out_len, HttpMeta *meta) {
  if (!url || !buf || cap < 2 || !out_len || !meta) return false;
  memset(meta, 0, sizeof *meta);
  meta->content_length = -1;
  *out_len = 0;

  static char cur[256]; strncpy(cur, url, sizeof cur - 1); cur[sizeof cur - 1] = 0;
  static char next[256];
  for (int hop = 0; hop < 6; hop++) {
    meta->redirect[0] = 0;
    if (!http_get_once(cur, buf, cap, out_len, meta)) return false;
    if (meta->status >= 300 && meta->status < 400 && meta->redirect[0]) {
      if (!url_resolve(cur, meta->redirect, next, sizeof next)) return false;
      strncpy(cur, next, sizeof cur - 1); cur[sizeof cur - 1] = 0;
      continue;
    }
    strncpy(meta->final_url, cur, sizeof meta->final_url - 1);
    meta->final_url[sizeof meta->final_url - 1] = 0;
    return true;
  }
  meta->status = -6; // redirect loop / too many redirects
  return false;
}
