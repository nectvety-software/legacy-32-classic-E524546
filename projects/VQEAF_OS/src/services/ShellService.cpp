#include "ShellService.h"
#include "../core/BuildVersion.h"
#include "NotificationService.h"
#include "WiFiConnectionService.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "TrustedTls.h"
#include "BoardDiagnostics.h"
#if defined(VQEAF_SNAKE_DEMO_KEY)
#include "SnakeDemoTrustKey.h"
#elif defined(QEAPP_TRUST_KEY_HEADER)
#include QEAPP_TRUST_KEY_HEADER
#else
#include "QeappTrustKey.h"
#endif
#include <HTTPClient.h>
#include <SD_MMC.h>
#include <time.h>
#include <ctype.h>

#if __has_include(<ping/ping_sock.h>)
  #include <ping/ping_sock.h>
  #define SYMBIAN_SHELL_HAS_ICMP 1
#else
  #define SYMBIAN_SHELL_HAS_ICMP 0
#endif

constexpr int ShellService::MAX_LINES;
constexpr int ShellService::LINE_CHARS;
constexpr int ShellService::HISTORY_MAX;
constexpr int ShellService::COMMAND_CHARS;

static String basenameOf(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash >= 0 && slash + 1 < (int)path.length()) return path.substring(slash + 1);
  return path;
}

String ShellService::trimCopy(String value) {
  value.trim();
  return value;
}

int ShellService::splitArgs(const String &line, String *out, int maxArgs) {
  if (!out || maxArgs <= 0) return 0;
  int count = 0;
  String token;
  char quote = 0;
  bool escaped = false;
  for (int i = 0; i < (int)line.length(); ++i) {
    const char ch = line[i];
    if (escaped) {
      token += ch;
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (quote) {
      if (ch == quote) quote = 0;
      else token += ch;
      continue;
    }
    if (ch == '"' || ch == '\'') {
      quote = ch;
      continue;
    }
    if (isspace((unsigned char)ch)) {
      if (token.length()) {
        if (count < maxArgs) out[count++] = token;
        token = String();
        if (count >= maxArgs) break;
      }
      continue;
    }
    token += ch;
  }
  if (token.length() && count < maxArgs) out[count++] = token;
  return count;
}

String ShellService::joinArgs(String *argv, int argc, int first) {
  String out;
  for (int i = first; i < argc; ++i) {
    if (out.length()) out += ' ';
    out += argv[i];
  }
  return out;
}

void ShellService::begin(StorageService &storage, SystemService &system, WiFiConnectionService *wifi) {
  storageService = &storage;
  radioService = wifi;
  systemService = &system;
  currentDir = "/";
  clear();
  push("VQEAF Shell v2.1.0");
  push("SD / TLS bench diagnostics");
  push("Type help for commands");
}

void ShellService::clear() {
  usedLines = 0;
  for (int i = 0; i < MAX_LINES; ++i) lines[i][0] = 0;
}

const char *ShellService::lineAt(int index) const {
  if (index < 0 || index >= usedLines) return "";
  return lines[index];
}

String ShellService::historyAt(int index) const {
  if (index < 0 || index >= historyUsed) return String();
  return String(history[index]);
}

bool ShellService::takeRebootRequest() {
  const bool requested = rebootRequested;
  rebootRequested = false;
  return requested;
}

void ShellService::push(const String &text) {
  if (usedLines >= MAX_LINES) {
    for (int i = 1; i < MAX_LINES; ++i) memcpy(lines[i - 1], lines[i], LINE_CHARS + 1);
    usedLines = MAX_LINES - 1;
  }
  String clipped = text;
  if ((int)clipped.length() > LINE_CHARS) clipped = clipped.substring(0, LINE_CHARS);
  snprintf(lines[usedLines], LINE_CHARS + 1, "%s", clipped.c_str());
  ++usedLines;
}

void ShellService::pushWrapped(const String &text) {
  if (!text.length()) { push(""); return; }
  const char *raw = text.c_str();
  int start = 0;
  while (start < (int)text.length()) {
    int end = min(start + LINE_CHARS, (int)text.length());
    if (end < (int)text.length()) {
      int split = end;
      while (split > start + 8 && raw[split] != ' ') --split;
      if (split > start + 8) end = split;
    }
    String part = text.substring(start, end);
    part.trim();
    push(part);
    start = end;
    while (start < (int)text.length() && raw[start] == ' ') ++start;
  }
}

void ShellService::remember(const String &command) {
  if (!command.length()) return;
  if (historyUsed > 0 && String(history[0]) == command) return;
  const int last = min(historyUsed, HISTORY_MAX - 1);
  for (int i = last; i > 0; --i) memcpy(history[i], history[i - 1], COMMAND_CHARS + 1);
  String clipped = command.substring(0, COMMAND_CHARS);
  snprintf(history[0], COMMAND_CHARS + 1, "%s", clipped.c_str());
  if (historyUsed < HISTORY_MAX) ++historyUsed;
}

String ShellService::normalizePath(const String &raw) const {
  String path = trimCopy(raw);
  if (!path.length()) return currentDir;
  if (path == "~") return "/";
  if (!path.startsWith("/")) path = currentDir == "/" ? "/" + path : currentDir + "/" + path;

  String out = "/";
  const char *rawPath = path.c_str();
  int pos = 0;
  while (pos < (int)path.length()) {
    while (pos < (int)path.length() && rawPath[pos] == '/') ++pos;
    if (pos >= (int)path.length()) break;
    int next = pos;
    while (next < (int)path.length() && rawPath[next] != '/') ++next;
    String seg = path.substring(pos, next);
    pos = next;
    if (!seg.length() || seg == ".") continue;
    if (seg == "..") {
      if (out != "/") {
        if (out.endsWith("/")) out.remove(out.length() - 1);
        int slash = out.lastIndexOf('/');
        out = slash <= 0 ? "/" : out.substring(0, slash);
      }
      continue;
    }
    if (out != "/") out += "/";
    out += seg;
  }
  return out.length() ? out : "/";
}

bool ShellService::changeDir(const String &path) {
  if (!storageService || !storageService->mounted()) return false;
  const String normalized = normalizePath(path);
  File f = storageService->fs().open(normalized);
  const bool ok = f && f.isDirectory();
  if (f) f.close();
  if (ok) currentDir = normalized;
  return ok;
}

void ShellService::commandLs(const String &path) {
  if (!storageService || !storageService->mounted()) { push("ls: microSD not mounted"); return; }
  const String normalized = normalizePath(path);
  File dir = storageService->fs().open(normalized);
  if (!dir || !dir.isDirectory()) { push("ls: not a directory"); if (dir) dir.close(); return; }
  int shown = 0;
  File f = dir.openNextFile();
  while (f && shown < 14) {
    String name = basenameOf(String(f.name()));
    if (f.isDirectory()) push(String("d ") + name + "/");
    else push(String("- ") + name + "  " + String((unsigned long)f.size()));
    ++shown;
    f.close();
    f = dir.openNextFile();
  }
  if (f) { f.close(); push("... output truncated"); }
  if (!shown) push("(empty)");
  dir.close();
}

void ShellService::commandCat(const String &path) {
  if (!storageService || !storageService->mounted()) { push("cat: microSD not mounted"); return; }
  const String normalized = normalizePath(path);
  File f = storageService->fs().open(normalized);
  if (!f || f.isDirectory()) { push("cat: file not found"); if (f) f.close(); return; }
  char line[LINE_CHARS + 1];
  int n = 0, emitted = 0;
  while (f.available() && emitted < 12) {
    int v = f.read();
    if (v < 0) break;
    char ch = (char)v;
    if (ch == '\r') continue;
    if (ch == '\n' || n == LINE_CHARS) {
      line[n] = 0; push(String(line)); ++emitted; n = 0;
      if (ch != '\n') line[n++] = ch;
    } else if ((unsigned char)ch >= 32 || ch == '\t') line[n++] = ch == '\t' ? ' ' : ch;
  }
  if (n && emitted < 12) { line[n] = 0; push(String(line)); ++emitted; }
  if (f.available()) push("... file truncated");
  if (!emitted) push("(empty file)");
  f.close();
}

void ShellService::commandStat(const String &path) {
  if (!storageService || !storageService->mounted()) { push("stat: microSD not mounted"); return; }
  const String normalized = normalizePath(path);
  File f = storageService->fs().open(normalized);
  if (!f) { push("stat: not found"); return; }
  pushWrapped(String("Path: ") + normalized);
  push(String("Type: ") + (f.isDirectory() ? "directory" : "file"));
  if (!f.isDirectory()) push(String("Size: ") + String((unsigned long)f.size()) + " bytes");
  f.close();
}

void ShellService::commandMkdir(const String &path) {
  if (!storageService || !storageService->mounted()) { push("mkdir: microSD not mounted"); return; }
  if (!path.length()) { push("usage: mkdir <directory>"); return; }
  String p = normalizePath(path);
  if (p == "/") { push("mkdir: root already exists"); return; }
  File existing = storageService->fs().open(p);
  if (existing) { bool isDir = existing.isDirectory(); existing.close(); push(isDir ? "mkdir: already exists" : "mkdir: file exists"); return; }
  push(storageService->fs().mkdir(p) ? "directory created" : "mkdir: failed");
}

void ShellService::commandRm(const String &path, bool directory) {
  if (!storageService || !storageService->mounted()) { push("rm: microSD not mounted"); return; }
  if (!path.length()) { push(directory ? "usage: rmdir <directory>" : "usage: rm <file>"); return; }
  String p = normalizePath(path);
  if (p == "/") { push("rm: refusing to remove root"); return; }
  File f = storageService->fs().open(p);
  if (!f) { push("rm: not found"); return; }
  bool isDir = f.isDirectory(); f.close();
  if (directory != isDir) { push(directory ? "rmdir: not a directory" : "rm: is a directory"); return; }
  bool ok = directory ? storageService->fs().rmdir(p) : storageService->fs().remove(p);
  push(ok ? (directory ? "directory removed" : "file removed") : "rm: failed (directory may not be empty)");
}

void ShellService::commandTouch(const String &path) {
  if (!storageService || !storageService->mounted()) { push("touch: microSD not mounted"); return; }
  if (!path.length()) { push("usage: touch <file>"); return; }
  String p = normalizePath(path);
  File f = storageService->fs().open(p, FILE_APPEND);
  if (!f) { push("touch: failed"); return; }
  f.close(); push("touch: ok");
}

void ShellService::commandCopy(const String &source, const String &dest, bool move) {
  if (!storageService || !storageService->mounted()) { push("cp: microSD not mounted"); return; }
  if (!source.length() || !dest.length()) { push(move ? "usage: mv <src> <dst>" : "usage: cp <src> <dst>"); return; }
  String srcPath = normalizePath(source);
  String dstPath = normalizePath(dest);
  if (srcPath == dstPath) { push("cp: source equals destination"); return; }

  File src = storageService->fs().open(srcPath);
  if (!src || src.isDirectory()) { push("cp: source file not found"); if (src) src.close(); return; }
  File dstMaybe = storageService->fs().open(dstPath);
  if (dstMaybe && dstMaybe.isDirectory()) {
    dstMaybe.close();
    if (!dstPath.endsWith("/")) dstPath += "/";
    dstPath += basenameOf(srcPath);
  } else if (dstMaybe) dstMaybe.close();
  src.close();

  if (move) {
    if (storageService->fs().rename(srcPath, dstPath)) { push("move: ok"); return; }
    push("mv: rename failed, copying");
  }

  src = storageService->fs().open(srcPath, FILE_READ);
  File dst = storageService->fs().open(dstPath, FILE_WRITE);
  if (!src || !dst) { if (src) src.close(); if (dst) dst.close(); push("cp: cannot open destination"); return; }
  uint8_t buffer[512];
  uint64_t copied = 0;
  bool ok = true;
  while (src.available()) {
    size_t got = src.read(buffer, sizeof(buffer));
    if (!got) break;
    size_t put = dst.write(buffer, got);
    copied += put;
    if (put != got) { ok = false; break; }
    delay(0);
  }
  dst.flush(); src.close(); dst.close();
  if (!ok) { storageService->fs().remove(dstPath); push("cp: write failed"); return; }
  fileCopyBytes += copied;
  if (move && !storageService->fs().remove(srcPath)) { push("mv: copied but source remove failed"); return; }
  pushWrapped(String(move ? "moved " : "copied ") + String((unsigned long)copied) + " bytes");
}

void ShellService::commandHexdump(const String &path, uint32_t offset) {
  if (!storageService || !storageService->mounted()) { push("hexdump: microSD not mounted"); return; }
  if (!path.length()) { push("usage: hexdump <file> [offset]"); return; }
  String p = normalizePath(path);
  File f = storageService->fs().open(p, FILE_READ);
  if (!f || f.isDirectory()) { push("hexdump: file not found"); if (f) f.close(); return; }
  if (offset >= f.size()) { f.close(); push("hexdump: offset past EOF"); return; }
  f.seek(offset);
  uint8_t bytes[8];
  for (int row = 0; row < 8 && f.available(); ++row) {
    size_t n = f.read(bytes, sizeof(bytes));
    char line[LINE_CHARS + 1];
    int pos = snprintf(line, sizeof(line), "%06lX: ", (unsigned long)(offset + row * 8));
    for (size_t i = 0; i < n && pos < LINE_CHARS - 3; ++i) pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", bytes[i]);
    push(String(line));
  }
  f.close();
}

void ShellService::commandWrite(const String &path, const String &text, bool append) {
  if (!storageService || !storageService->mounted()) { push("write: microSD not mounted"); return; }
  if (!path.length()) { push(append ? "usage: append <file> <text>" : "usage: write <file> <text>"); return; }
  String p = normalizePath(path);
  File f = storageService->fs().open(p, append ? FILE_APPEND : FILE_WRITE);
  if (!f) { push("write: cannot open file"); return; }
  size_t n = f.print(text);
  if (append) f.print("\n");
  f.close();
  push(String(append ? "appended " : "written ") + String((unsigned long)n) + " bytes");
}

void ShellService::commandIfconfig() {
  if (WiFi.getMode() == WIFI_OFF) { push("wlan0: DOWN"); return; }
  push(String("wlan0: ") + (WiFi.status() == WL_CONNECTED ? "UP RUNNING" : "UP NO-CARRIER"));
  pushWrapped(String("mac ") + WiFi.macAddress());
  if (WiFi.status() == WL_CONNECTED) {
    push(String("inet ") + WiFi.localIP().toString());
    push(String("mask ") + WiFi.subnetMask().toString());
    push(String("gw   ") + WiFi.gatewayIP().toString());
    push(String("dns  ") + WiFi.dnsIP().toString());
    push(String("rssi ") + String(WiFi.RSSI()) + " dBm");
  }
}

void ShellService::commandNetmon() {
  push("NETWORK MONITOR");
  commandIfconfig();
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    const char *quality = rssi >= -55 ? "excellent" : rssi >= -67 ? "good" : rssi >= -75 ? "fair" : "weak";
    pushWrapped(String("signal: ") + quality + " / " + String(rssi) + " dBm");
  }
  push(String("downloaded: ") + String((unsigned long)networkDownloadBytes) + " B");
  if (lastNetworkActionMs) push(String("last net op: ") + String((millis() - lastNetworkActionMs) / 1000UL) + "s ago");
}

void ShellService::commandNslookup(const String &host) {
  if (!host.length()) { push("usage: nslookup <host>"); return; }
  if (WiFi.status() != WL_CONNECTED) { push("nslookup: WiFi disconnected"); return; }
  IPAddress ip;
  uint32_t start = millis();
  int ok = WiFi.hostByName(host.c_str(), ip);
  lastNetworkActionMs = millis();
  if (!ok) { push("nslookup: resolution failed"); return; }
  pushWrapped(String("Name: ") + host);
  push(String("Address: ") + ip.toString());
  push(String("DNS time: ") + String(millis() - start) + " ms");
}

#if SYMBIAN_SHELL_HAS_ICMP
struct ShellPingContext {
  volatile bool done;
  volatile uint32_t replies;
  volatile uint32_t minMs;
  volatile uint32_t maxMs;
  volatile uint32_t sumMs;
  volatile uint32_t transmitted;
};

static void shellPingSuccess(esp_ping_handle_t hdl, void *args) {
  ShellPingContext *ctx = static_cast<ShellPingContext*>(args);
  uint32_t elapsed = 0;
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed));
  ctx->replies++;
  ctx->sumMs += elapsed;
  if (elapsed < ctx->minMs) ctx->minMs = elapsed;
  if (elapsed > ctx->maxMs) ctx->maxMs = elapsed;
}
static void shellPingTimeout(esp_ping_handle_t, void *) {}
static void shellPingEnd(esp_ping_handle_t hdl, void *args) {
  ShellPingContext *ctx = static_cast<ShellPingContext*>(args);
  uint32_t tx = 0;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &tx, sizeof(tx));
  ctx->transmitted = tx;
  ctx->done = true;
}
#endif

void ShellService::commandPing(const String &host, int count) {
  if (!host.length()) { push("usage: ping <host> [1..6]"); return; }
  if (WiFi.status() != WL_CONNECTED) { push("ping: WiFi disconnected"); return; }
  count = constrain(count, 1, 6);
  IPAddress ip;
  if (!WiFi.hostByName(host.c_str(), ip)) { push("ping: unknown host"); return; }
  pushWrapped(String("PING ") + host + " (" + ip.toString() + ")");
  lastNetworkActionMs = millis();

#if SYMBIAN_SHELL_HAS_ICMP
  ip_addr_t target;
  IP_ADDR4(&target, ip[0], ip[1], ip[2], ip[3]);
  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
  config.target_addr = target;
  config.count = count;
  config.interval_ms = 350;
  config.timeout_ms = 900;
  ShellPingContext context = {false, 0, 0xFFFFFFFFUL, 0, 0, 0};
  esp_ping_callbacks_t callbacks = {};
  callbacks.on_ping_success = shellPingSuccess;
  callbacks.on_ping_timeout = shellPingTimeout;
  callbacks.on_ping_end = shellPingEnd;
  callbacks.cb_args = &context;
  esp_ping_handle_t handle = nullptr;
  if (esp_ping_new_session(&config, &callbacks, &handle) != ESP_OK || !handle) { push("ping: session failed"); return; }
  esp_ping_start(handle);
  uint32_t deadline = millis() + (uint32_t)count * 1400UL + 1000UL;
  while (!context.done && (int32_t)(deadline - millis()) > 0) delay(20);
  esp_ping_stop(handle);
  esp_ping_delete_session(handle);
  const uint32_t tx = context.transmitted ? context.transmitted : (uint32_t)count;
  push(String(tx) + " packets, " + String(context.replies) + " replies");
  if (context.replies) {
    push(String("rtt min/avg/max ") + String(context.minMs) + "/" + String(context.sumMs / context.replies) + "/" + String(context.maxMs) + " ms");
  }
#else
  WiFiClient c;
  uint32_t started = millis();
  bool ok = c.connect(ip.toString().c_str(), 80, 2500);
  c.stop();
  push("ICMP unavailable; TCP probe used");
  push(String(ok ? "reachable " : "no TCP/80 response ") + String(millis() - started) + " ms");
#endif
}

void ShellService::commandWget(const String &url, const String &path) {
  if (!url.length()) { push("usage: wget https://host/file [path]"); return; }
  if (!url.startsWith("https://")) { push("wget: verified HTTPS required"); return; }
  if (WiFi.status() != WL_CONNECTED) { push("wget: WiFi disconnected"); return; }
  if (!storageService || !storageService->mounted()) { push("wget: microSD unavailable"); return; }
  String outPath = path;
  if (!outPath.length()) {
    int slash = url.lastIndexOf('/');
    outPath = slash >= 0 ? url.substring(slash + 1) : String("download.bin");
    int q = outPath.indexOf('?'); if (q >= 0) outPath = outPath.substring(0, q);
    if (!outPath.length()) outPath = "index.html";
  }
  outPath = normalizePath(outPath);
  if (outPath == "/" || outPath.indexOf("..") >= 0 || outPath.endsWith(".part")) {
    push("wget: invalid destination"); return;
  }
  if (storageService->exists(outPath)) { push("wget: destination exists"); return; }
  WiFiClientSecure client;
  String tlsError;
  if (!TrustedTls::configure(client, tlsError)) { pushWrapped(tlsError); return; }

  HTTPClient http;
  http.setConnectTimeout(9000);
  http.setTimeout(15000);
  // No invisible cross-scheme redirects. User can fetch the final HTTPS URL.
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(client, url.c_str())) { push("wget: cannot open HTTPS"); return; }
  http.addHeader("Accept-Encoding", "identity");
  int code = http.GET();
  if (code <= 0) { http.end(); push("wget: TLS verification/network failed"); return; }
  if (code < 200 || code >= 300) {
    http.end();
    push(code >= 300 && code < 400 ? "wget: redirect; supply final HTTPS URL" : String("wget: HTTP ") + code);
    return;
  }
  const int declaredBytes = http.getSize();
  const int MAX_BYTES = 4 * 1024 * 1024;
  if (declaredBytes <= 0) { http.end(); push("wget: Content-Length required"); return; }
  if (declaredBytes > MAX_BYTES) { http.end(); push("wget: 4 MB maximum"); return; }
  const String pending = outPath + ".part";
  storageService->fs().remove(pending);
  File out = storageService->fs().open(pending, FILE_WRITE);
  if (!out) { http.end(); push("wget: SD open failed"); return; }
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[512];
  int total = 0;
  uint32_t lastData = millis();
  uint32_t started = lastData;
  bool failed = false;
  while (http.connected() && (declaredBytes < 0 || total < declaredBytes)) {
    if ((uint32_t)(millis() - lastData) > 8000UL ||
        (uint32_t)(millis() - started) > 90000UL) { failed = true; break; }
    const int available = stream->available();
    if (!available) { delay(1); continue; }
    const size_t want = min((size_t)available, sizeof(buffer));
    const int got = stream->readBytes(buffer, want);
    if (got <= 0) continue;
    if (total > MAX_BYTES - got || out.write(buffer, got) != (size_t)got) { failed = true; break; }
    total += got; lastData = millis();
  }
  out.flush(); out.close(); http.end();
  File staged = storageService->fs().open(pending, FILE_READ);
  const bool stagedOkay = staged && staged.size() == (size_t)total;
  if (staged) staged.close();
  if (failed || total == 0 || (declaredBytes >= 0 && total != declaredBytes) ||
      !stagedOkay || !storageService->mounted() || storageService->exists(outPath) ||
      !storageService->fs().rename(pending, outPath)) {
    storageService->fs().remove(pending);
    push("wget: partial / SD error; no file installed"); return;
  }
  lastNetworkActionMs = millis();
  networkDownloadBytes += (uint32_t)total;
  push(String("HTTPS 200, ") + total + " bytes");
  pushWrapped(String("saved: ") + outPath);
}

void ShellService::commandTop() {
  push("SYSTEM MONITOR");
  push(String("uptime: ") + (systemService ? systemService->uptimeText() : String(millis() / 1000UL) + "s"));
  push(String("heap free: ") + String((unsigned long)ESP.getFreeHeap()) + " B");
  if (systemService) push(String("heap min:  ") + String((unsigned long)systemService->minFreeHeap()) + " B");
  push(String("psram: ") + String((unsigned long)ESP.getFreePsram()) + "/" + String((unsigned long)ESP.getPsramSize()));
  push(String("wifi: ") + (WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) + " dBm" : "offline"));
  push(String("file copy: ") + String((unsigned long)fileCopyBytes) + " B");
  push(String("net dl: ") + String((unsigned long)networkDownloadBytes) + " B");
}


void ShellService::commandLayout() {
  if (!storageService || !storageService->mounted()) { push("layout: microSD not mounted"); return; }
  push("SYSTEM SD LAYOUT");
  push("/System/Cache/Web");
  push("/System/Cache/Thumbs");
  push("/System/Themes");
  push("/System/Apps/{Installed,Inbox}");
  push("/System/{Downloads,Logs,Temp}");
  push("/Media/{Music,Pictures}");
  push("/Documents");
}

void ShellService::commandCache(const String &actionRaw) {
  if (!storageService || !storageService->mounted()) { push("cache: microSD not mounted"); return; }
  String action = actionRaw; action.toLowerCase();
  if (!action.length() || action == "status") {
    uint64_t bytes = storageService->directoryBytes(StoragePaths::CACHE_WEB, 64);
    push(String("web cache: ") + String((unsigned long)bytes) + " B");
    push("quota: 16 files / 512 KB");
    return;
  }
  if (action == "prune") {
    int n = storageService->pruneFlatDirectory(StoragePaths::CACHE_WEB, 16, 512UL * 1024UL);
    if (n >= 0) push(String("cache: pruned ") + n + " file(s)");
    else push("cache: prune unfinished; check SD");
    return;
  }
  if (action == "clear") {
    const int removed = storageService->clearFlatDirectory(StoragePaths::CACHE_WEB);
    if (removed < 0) push("cache: clear unfinished; check SD");
    else push(String("cache: cleared ") + removed + " file(s)");
    return;
  }
  push("usage: cache [status|prune|clear]");
}

void ShellService::commandHelp() {
  push("SYSTEM: help clear uname uptime free");
  push("layout cache df mount sd sddiag corediag top");
  push("FILES: ls cd pwd cat stat mkdir rm");
  push("rmdir cp mv touch write append hexdump");
  push("NET: wifi ifconfig ip nslookup ping");
  push("wget(HTTPS) tlsdiag netmon ps dmesg");
  push("reboot echo history");
}

void ShellService::execute(const String &input, NotificationService &notifications) {
  String full = trimCopy(input);
  if (!full.length()) return;
  remember(full);
  pushWrapped(String("$ ") + full);

  String argv[8];
  int argc = splitArgs(full, argv, 8);
  if (argc <= 0) return;
  String cmd = argv[0]; cmd.toLowerCase();
  String arg = argc > 1 ? joinArgs(argv, argc, 1) : String();

  if (cmd == "help" || cmd == "?") commandHelp();
  else if (cmd == "clear" || cmd == "cls") clear();
  else if (cmd == "uname") push("VQEAF-OS ESP32-S3 Xtensa LX7");
  else if (cmd == "version") push(VQEAF_OS_VERSION_TEXT);
  else if (cmd == "uptime") push(systemService ? systemService->uptimeText() : String(millis() / 1000UL) + "s");
  else if (cmd == "free") {
    push(String("heap free: ") + String((unsigned long)ESP.getFreeHeap()));
    push(String("psram free: ") + String((unsigned long)ESP.getFreePsram()));
    if (systemService) push(String("heap min: ") + String((unsigned long)systemService->minFreeHeap()));
  } else if (cmd == "top") commandTop();
  else if (cmd == "layout") commandLayout();
  else if (cmd == "cache") commandCache(argc > 1 ? argv[1] : String("status"));
  else if (cmd == "sd") {
    if (!storageService) push("sd: storage service unavailable");
    else {
      push(String("SD: ") + (storageService->mounted() ? "mounted" : "offline"));
      push(String("I/O failures: ") + storageService->ioErrors());
      push("Probe 8s / retry 5-15s when idle");
    }
  }
  else if (cmd == "corediag") {
    // Read-only field diagnosis: no key material, network credentials or
    // user files are printed. Distinguish SD, package-key and TLS failures.
    push(String("microSD: ") + (storageService && storageService->mounted() ? "OK" : "NOT MOUNTED"));
    if (storageService && storageService->mounted()) {
      const char *dirs[]={StoragePaths::THEMES,StoragePaths::APPS_INBOX,
                           StoragePaths::APPS_INSTALLED,StoragePaths::CACHE_WEB};
      for (const char *dir : dirs) {
        File entry=storageService->fs().open(dir,FILE_READ);
        const bool ok=entry && entry.isDirectory();
        if(entry)entry.close();
        push(String(ok?"OK ":"MISSING ")+dir);
      }
      const char *appExt[]={".qeapp"}, *skinExt[]={".vqeaf"};
      FsEntry found[1];
      const int nApps=storageService->scanMedia(StoragePaths::APPS_INBOX,appExt,1,found,1,0);
      const int nThemes=storageService->scanMedia(StoragePaths::THEMES,skinExt,1,found,1,0);
      push(String("Inbox QEAPP: ")+(nApps?"found":"none"));
      push(String("Themes: ")+(nThemes?"found":"none"));
    }
    char key[16];snprintf(key,sizeof key,"0x%08lX",(unsigned long)QEAPP_TRUST_KEY_ID);
    push(String("QEAPP trust key: ")+key);
    push(String("WiFi: ")+(WiFi.status()==WL_CONNECTED?"connected":"offline"));
    push(String("HTTPS clock: ")+(TrustedTls::timeValidAt(time(nullptr))?"READY":"WAIT NTP"));
    push("Signed apps: inspect via App Manager");
    push("TLS: tlsdiag valid after time sync");
  }
  else if (cmd == "sddiag") {
    if (!storageService) push("sddiag: storage service unavailable");
    else if (argc == 1 || argv[1] == "status") pushWrapped(BoardDiagnostics::sdStatus(*storageService));
    else if (argv[1] == "rw") {
      String info;
      const BoardDiagnostics::Result verdict = BoardDiagnostics::testSdReadWrite(*storageService, info);
      Serial.printf("[S3DIAG][SD] test=rw result=%s detail=%s\n", BoardDiagnostics::label(verdict), info.c_str());
      pushWrapped(String(BoardDiagnostics::label(verdict)) + ": " + info);
    } else push("usage: sddiag [status|rw]");
  }
  else if (cmd == "tlsdiag") {
    String host;
    bool expected = true;
    if (argc < 2 || argv[1] == "valid") host = "valid-isrgrootx1.letsencrypt.org";
    else if (argv[1] == "expired") { host = "expired.badssl.com"; expected = false; }
    else if (argv[1] == "wrong") { host = "wrong.host.badssl.com"; expected = false; }
    else if (argv[1] == "self") { host = "self-signed.badssl.com"; expected = false; }
    else if (argv[1] == "host" && argc > 2) host = argv[2];
    else { push("tlsdiag: valid|expired|wrong|self"); push("or tlsdiag host example.com"); host = ""; }
    if (host.length()) {
      String info;
      const BoardDiagnostics::Result verdict = BoardDiagnostics::testTls(host, expected, info);
      Serial.printf("[S3DIAG][TLS] verdict=%s detail=%s\n", BoardDiagnostics::label(verdict), info.c_str());
      pushWrapped(String(BoardDiagnostics::label(verdict)) + ": " + info);
    }
  }
  else if (cmd == "df") {
    if (!storageService || !storageService->mounted()) push("microSD: not mounted");
    else { push(String("microSD size: ") + String((unsigned long)storageService->cardSizeMB()) + " MB"); push(String("free: ") + String((unsigned long)(storageService->freeBytes()/1024ULL/1024ULL)) + " MB"); push("filesystem: SD_MMC / FAT"); }
  } else if (cmd == "mount") {
    push(storageService && storageService->mounted() ? "/sdcard on / type SD_MMC (rw)" : "mount: no microSD filesystem");
  } else if (cmd == "pwd") push(currentDir);
  else if (cmd == "cd") { if (!changeDir(argc > 1 ? argv[1] : "/")) push("cd: directory not found"); }
  else if (cmd == "ls") commandLs(argc > 1 ? argv[1] : String());
  else if (cmd == "cat") { if (argc < 2) push("usage: cat <file>"); else commandCat(argv[1]); }
  else if (cmd == "stat") { if (argc < 2) push("usage: stat <path>"); else commandStat(argv[1]); }
  else if (cmd == "mkdir") commandMkdir(argc > 1 ? argv[1] : String());
  else if (cmd == "rm") commandRm(argc > 1 ? argv[1] : String(), false);
  else if (cmd == "rmdir") commandRm(argc > 1 ? argv[1] : String(), true);
  else if (cmd == "touch") commandTouch(argc > 1 ? argv[1] : String());
  else if (cmd == "cp") commandCopy(argc > 1 ? argv[1] : String(), argc > 2 ? argv[2] : String(), false);
  else if (cmd == "mv") commandCopy(argc > 1 ? argv[1] : String(), argc > 2 ? argv[2] : String(), true);
  else if (cmd == "hexdump" || cmd == "xxd") commandHexdump(argc > 1 ? argv[1] : String(), argc > 2 ? (uint32_t)strtoul(argv[2].c_str(), nullptr, 0) : 0);
  else if (cmd == "write") commandWrite(argc > 1 ? argv[1] : String(), argc > 2 ? joinArgs(argv, argc, 2) : String(), false);
  else if (cmd == "append") commandWrite(argc > 1 ? argv[1] : String(), argc > 2 ? joinArgs(argv, argc, 2) : String(), true);
  else if (cmd == "date") {
    struct tm info;
    if (getLocalTime(&info, 10)) { char b[36]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &info); push(String(b)); }
    else push(String("uptime clock: ") + String(millis() / 1000UL) + "s");
  } else if (cmd == "wifi") {
    String mode = argc > 1 ? argv[1] : String("status");
    String modeLower = mode; modeLower.toLowerCase();
    if (modeLower == "status") commandIfconfig();
    else if (modeLower == "off") { if (radioService) radioService->externalOverride(); WiFi.disconnect(); WiFi.mode(WIFI_OFF); push("wifi: off"); notifications.push("Shell", "WiFi radio disabled"); }
    else if (modeLower == "on") { if (radioService) radioService->externalOverride(); WiFi.mode(WIFI_STA); push("wifi: station mode on"); notifications.push("Shell", "WiFi radio enabled"); }
    else if (modeLower == "scan") {
      if (radioService) radioService->externalOverride();
      WiFi.mode(WIFI_STA);
      int found = WiFi.scanNetworks(false, true);
      int limit = min(found, 8);
      for (int i = 0; i < limit; ++i) pushWrapped(String(WiFi.RSSI(i)) + "  " + WiFi.SSID(i));
      if (found > limit) push("... scan truncated");
      if (found <= 0) push("wifi: no networks found");
      WiFi.scanDelete(); lastNetworkActionMs = millis();
    } else {
      String ssid = argv[1];
      String pass = argc > 2 ? argv[2] : String();
      if (radioService) radioService->externalOverride();
      WiFi.mode(WIFI_STA);
      if (pass.length()) WiFi.begin(ssid.c_str(), pass.c_str()); else WiFi.begin(ssid.c_str());
      pushWrapped(String("wifi: associating with ") + ssid);
      push("run 'wifi status' to check");
      lastNetworkActionMs = millis();
    }
  } else if (cmd == "ifconfig") commandIfconfig();
  else if (cmd == "ip") { if (WiFi.status() == WL_CONNECTED) push(WiFi.localIP().toString()); else push("ip: no active WiFi lease"); }
  else if (cmd == "netmon" || cmd == "netstat") commandNetmon();
  else if (cmd == "nslookup" || cmd == "dns") commandNslookup(argc > 1 ? argv[1] : String());
  else if (cmd == "ping") commandPing(argc > 1 ? argv[1] : String(), argc > 2 ? argv[2].toInt() : 4);
  else if (cmd == "wget") commandWget(argc > 1 ? argv[1] : String(), argc > 2 ? argv[2] : String());
  else if (cmd == "ps") {
    push("PID SERVICE       STATE");
    push("  1 ui            running");
    push("  2 input         running");
    push(String("  3 wifi          ") + (WiFi.getMode() == WIFI_OFF ? "stopped" : "ready"));
    push(String("  4 recovery      ") + (systemService && systemService->safeMode() ? "safe" : "normal"));
  } else if (cmd == "dmesg") {
    const int n = notifications.count();
    if (!n) push("dmesg: no system events");
    for (int i = min(n, 8) - 1; i >= 0; --i) {
      const SystemNotification &ev = notifications.at(i);
      pushWrapped(String("[") + String((unsigned long)(ev.whenMs / 1000UL)) + "] " + ev.title + ": " + ev.body);
    }
  } else if (cmd == "safe") {
    String mode = argc > 1 ? argv[1] : String("status"); mode.toLowerCase();
    if (!systemService) push("safe: service unavailable");
    else if (mode == "status") push(systemService->safeMode() ? "safe mode: on" : "safe mode: off");
    else if (mode == "on") { systemService->setSafeMode(true); push("safe mode enabled; reboot advised"); }
    else if (mode == "off") { systemService->setSafeMode(false); push("safe mode disabled; reboot required"); }
    else push("usage: safe [status|on|off]");
  } else if (cmd == "reboot") { push("reboot requested"); rebootRequested = true; }
  else if (cmd == "echo") pushWrapped(arg);
  else if (cmd == "history") { for (int i = historyUsed - 1; i >= 0; --i) push(String(historyUsed - i) + "  " + history[i]); }
  else pushWrapped(String("sh: ") + cmd + ": command not found");
}
