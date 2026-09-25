// http.cpp — HTTP/1.1 client cho ESP_Browser.
// Port co che HttpConnection/HttpRequest/HttpResponse/Cookie cua DMAX_QQ_Browser:
// GET + User-Agent WAP, doc header, chunked transfer, Location redirect (toi da 5),
// Set-Cookie luu theo domain. Khong dung WiFiClientSecure (thoi WAP = HTTP plain).
#include "browser.h"
#include <WiFi.h>
#include <WiFiClient.h>

extern "C" {
#include <string.h>
#include <strings.h>
}

static bool ci_contains(const char *hay, const char *needle) {
  size_t nl = strlen(needle);
  for (const char *p = hay; *p; p++) {
    size_t k = 0;
    while (k < nl && p[k] && tolower((unsigned char)p[k]) == tolower((unsigned char)needle[k])) k++;
    if (k == nl) return true;
  }
  return false;
}

static bool read_line(WiFiClient &c, char *out, int cap, uint32_t timeout_ms) {
  int n = 0;
  uint32_t t0 = millis();
  while (n < cap - 1) {
    if (millis() - t0 > timeout_ms) return false;
    if (!c.available()) {
      if (!c.connected()) break;          // host bridge: socket dong -> khong doi them
      delay(1);
      continue;
    }
    char ch = c.read();
    if (ch == '\n') break;
    if (ch == '\r') continue;
    out[n++] = ch;
  }
  out[n] = 0;
  return n >= 0;
}

static bool read_chunked(WiFiClient &c, char *buf, size_t cap, size_t *out_len, uint32_t tmo) {
  size_t total = 0;
  uint32_t t0 = millis();
  for (;;) {
    if (millis() - t0 > tmo) break;
    char hex[12];
    if (!read_line(c, hex, sizeof hex, tmo)) break;
    long sz = strtol(hex, 0, 16);
    if (sz <= 0) break;
    while (sz-- > 0) {
      if (!c.available()) {
        if (!c.connected()) break;
        delay(1);
        continue;
      }
      char ch = c.read();
      if (total < cap - 1) buf[total++] = ch;
    }
    char nl[4];
    read_line(c, nl, sizeof nl, 2000);   // CRLF cuoi chunk
    t0 = millis();
  }
  buf[total] = 0;
  *out_len = total;
  return total > 0;
}

static bool read_fixed(WiFiClient &c, char *buf, size_t cap, size_t *out_len,
                       long content_len, uint32_t tmo) {
  size_t total = 0;
  uint32_t t0 = millis();
  long want = (content_len > 0) ? content_len : (long)cap;
  while (total < (size_t)want && total < cap - 1) {
    if (millis() - t0 > tmo) break;
    int avail = c.available();
    if (!avail) {
      if (!c.connected()) break;
      delay(1);
      continue;
    }
    while (avail-- > 0 && total < cap - 1) {
      buf[total++] = (char)c.read();
    }
    t0 = millis();
  }
  buf[total] = 0;
  *out_len = total;
  return total > 0;
}

bool url_split(const char *url, char *scheme, char *host, int *port, char *path) {
  const char *p = strstr(url, "://");
  if (!p) return false;
  size_t sl = p - url; if (sl >= 8) sl = 7;
  memcpy(scheme, url, sl); scheme[sl] = 0;
  for (char *q = scheme; *q; q++) *q = tolower(*q);
  p += 3;
  const char *slash = strchr(p, '/');
  const char *colon = strchr(p, ':');
  const char *hend = slash ? slash : p + strlen(p);
  if (colon && colon < hend) {
    size_t hl = colon - p; if (hl >= 64) hl = 63;
    memcpy(host, p, hl); host[hl] = 0;
    *port = atoi(colon + 1);
  } else {
    size_t hl = hend - p; if (hl >= 64) hl = 63;
    memcpy(host, p, hl); host[hl] = 0;
    *port = strcmp(scheme, "https") ? 80 : 443;
  }
  if (slash) { strncpy(path, slash, 191); path[191] = 0; }
  else { strcpy(path, "/"); }
  return host[0] != 0;
}

bool url_is_mtt(const char *url) { return !strncmp(url, "mtt:", 4); }

// ------------------- GET voi redirect -------------------
static bool http_get_once(const char *url, char *buf, size_t cap, size_t *out_len,
                          HttpMeta *meta) {
  char scheme[8], host[64], path[192];
  int port;
  if (!url_split(url, scheme, host, &port, path)) return false;
  if (strcmp(scheme, "http")) {           // https: WAP-era khong ho tro tren thiet bi nay
    meta->status = -1;
    return false;
  }
  WiFiClient client;
  if (!client.connect(host, port, 8000)) { meta->status = -2; return false; }

  // DMAX_QQ_Browser src_c/http.c: HTTP/1.0 GET + User-Agent DMAX/1.0
  client.print("GET "); client.print(path); client.print(" HTTP/1.0\r\n");
  client.print("Host: "); client.print(host);
  client.print("\r\nUser-Agent: DMAX/1.0\r\n");
  client.print("Accept: text/vnd.wap.wml, text/html, */*\r\n");
  client.print("Accept-Encoding: identity\r\n");
  client.print("Connection: close\r\n");
  client.print("Accept-Language: en\r\n");
  char ck[160]; cookie_get(host, ck, sizeof ck);
  if (ck[0]) { client.print("Cookie: "); client.print(ck); client.print("\r\n"); }
  client.print("Connection: close\r\n\r\n");

  char line[512];
  if (!read_line(client, line, sizeof line, 10000)) { meta->status = -3; client.stop(); return false; }
  meta->status = atoi(strstr(line, " ") ? strstr(line, " ") + 1 : "0");
  meta->redirect[0] = 0;
  meta->gzipped = false;
  long clen = -1;
  bool chunked = false;
  while (read_line(client, line, sizeof line, 10000)) {
    if (!line[0]) break;
    if (!strncasecmp(line, "Content-Length:", 15)) clen = atol(line + 15);
    else if (!strncasecmp(line, "Transfer-Encoding:", 18) && ci_contains(line + 18, "chunked")) chunked = true;
    else if (!strncasecmp(line, "Location:", 9)) {
      const char *v = line + 9; while (*v == ' ') v++;
      if (*v == '/') { snprintf(meta->redirect, sizeof meta->redirect, "%s://%s%s", scheme, host, v); }
      else { strncpy(meta->redirect, v, sizeof meta->redirect - 1); }
    }
    else if (!strncasecmp(line, "Content-Encoding:", 17) && ci_contains(line + 17, "gzip")) meta->gzipped = true;
    else if (!strncasecmp(line, "Content-Type:", 13)) {
      const char *v = line + 13; while (*v == ' ') v++;
      strncpy(meta->content_type, v, sizeof meta->content_type - 1);
    }
    else if (!strncasecmp(line, "Set-Cookie:", 11)) {
      const char *v = line + 11; while (*v == ' ') v++;
      cookie_set(host, v);
    }
  }
  bool ok;
  if (chunked) ok = read_chunked(client, buf, cap, out_len, 15000);
  else         ok = read_fixed(client, buf, cap, out_len, clen, 15000);
  client.stop();
  return ok;
}

bool http_get(const char *url, char *buf, size_t cap, size_t *out_len, HttpMeta *meta) {
  char cur[256];
  strncpy(cur, url, sizeof cur - 1); cur[sizeof cur - 1] = 0;
  for (int hop = 0; hop < 5; hop++) {
    meta->content_type[0] = 0;
    if (!http_get_once(cur, buf, cap, out_len, meta)) {
      if (meta->status == -1) { snprintf(buf, cap, "HTTPS khong ho tro (WAP-era browser)."); *out_len = strlen(buf); return true; }
      return false;
    }
    if (meta->status >= 300 && meta->status < 400 && meta->redirect[0]) {
      strncpy(cur, meta->redirect, sizeof cur - 1); cur[sizeof cur - 1] = 0;
      continue;
    }
    return true;
  }
  return false;
}
