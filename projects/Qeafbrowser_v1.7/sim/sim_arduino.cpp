// sim_arduino.cpp — Trien khai shim cho PC.
#include "sim_arduino.h"
#include <sys/stat.h>
#include <vector>
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

SimSerial Serial;
SimWiFi WiFi;
LGFX *LGFX::inst = nullptr;
SimFS SD_MMC{ "sim_sd" };
SimFS LittleFS{ "sim_lfs" };
SimNet SIM_NETS[] = {
  { "Zzz-No-Service", -95, true },
  { "VNPT-Home",      -52, false },
  { "Viettel-4G",     -67, false },
  { "FPT-Wifi",       -73, false },
};
int SIM_NET_COUNT = 4;
char SIM_CONNECTED[64] = "";
char SIM_EXPECT_PASS[64] = "abc";
bool SIM_HTTP_MOCK = true;
int SIM_KEYS[10] = { 0,0,0,0,0,0,0,0,0,0 };   // 1 = pressed (LOW)

static bool sim_read_file(const char *path, std::string &out) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  out.resize(n);
  fread(&out[0], 1, n, f);
  fclose(f);
  return true;
}

bool sim_http_mock() { return SIM_HTTP_MOCK; }
void sim_http_mock_set(bool on) { SIM_HTTP_MOCK = on; }

int sim_digitalRead(int pin) {
  static const int map_pin[] = { 18, 7, 15, 45, 17, 6, 8, 46, 5, 16 };
  for (int i = 0; i < 10; i++) if (map_pin[i] == pin) return SIM_KEYS[i] ? 0 : 1;
  return 1;
}

// ---------------- WiFiClient ----------------
static std::string g_req;
bool WiFiClient::connect(const char *host, int port, int) {
  rx.clear(); rxp = 0; g_req.clear(); ok = false;
  if (SIM_HTTP_MOCK) {
    ok = true;    // phan ung sinh o lan doc dau
    return true;
  }
  s = ::socket(AF_INET, SOCK_STREAM, 0);
  if (s == INVALID_SOCKET) return false;
  struct addrinfo hints = {}, *res = nullptr;
  hints.ai_family = AF_INET;
  char ps[8]; sprintf(ps, "%d", port);
  if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) { closesocket(s); s = INVALID_SOCKET; return false; }
  if (::connect(s, res->ai_addr, (int)res->ai_addrlen) < 0) { freeaddrinfo(res); closesocket(s); s = INVALID_SOCKET; return false; }
  freeaddrinfo(res);
  ok = true;
  return true;
}
void WiFiClient::print(const char *s) {
  if (!s || !s[0]) return;
  if (SIM_HTTP_MOCK) { g_req += s; return; }
  if (ok && this->s != INVALID_SOCKET) ::send(this->s, s, (int)strlen(s), 0);
}
int WiFiClient::available() {
  if (SIM_HTTP_MOCK) {
    if (rx.empty() && !g_req.empty()) {
      // Mock web that hon de test keypad focus / HTML / relative URL.
      const bool keypad = g_req.find("Host: keypad.test") != std::string::npos;
      const bool keypad_long = keypad && g_req.find("GET /long.html ") != std::string::npos;
      const bool qf_root = g_req.find("Host: qeafivels.com") != std::string::npos;
      const bool qf_www = g_req.find("Host: www.qeafivels.com") != std::string::npos;
      const bool qf_hero = g_req.find("hero-banner.jpg") != std::string::npos;
      const bool qf_products = g_req.find("products-audio.png") != std::string::npos;
      const bool article = g_req.find("GET /article.html ") != std::string::npos;
      const bool feed_rss = g_req.find("GET /rss.xml ") != std::string::npos;
      const bool feed_atom = g_req.find("GET /atom.xml ") != std::string::npos;
      const bool feed_host = g_req.find("Host: feed.test") != std::string::npos;
      const bool vn_host = g_req.find("Host: vn.test") != std::string::npos;
      const bool image_host = g_req.find("Host: image.test") != std::string::npos;
      const char *ctype = "text/vnd.wap.wml";
      const char *body = nullptr;
      if (image_host) {
        ctype = "text/html; charset=utf-8";
        body =
          "<!doctype html><html><head><title>Image source test</title></head><body>"
          "<img src=\"data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///ywAAAAAAQABAAACAUwAOw==\" data-src=\"/real.jpg?x=1&amp;y=2\" alt=\"Lazy image\"/>"
          "<img srcset=\"/small.webp 1x, /small.jpg 2x\" alt=\"Srcset image\"/>"
          "<img src=\"/vector.svg\" alt=\"Vector fallback\"/>"
          "</body></html>";
      } else if (vn_host) {
        ctype = "text/html; charset=utf-8";
        // XHTML-MP tieng Viet: numeric entity (dung codepoint Unicode dung)
        // ế=7871 ớ=7899 ệ=7879 ụ=7909 ộ=7897 ấ=7845 ả=7843 ạ=7841 ọ=7885 ơ=417 đ=273 á=225 ó=243 ữ=7919
        body =
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          "<!DOCTYPE html PUBLIC \"-//WAPFORUM//DTD XHTML Mobile 1.2//EN\" "
          "\"http://www.openmobilealliance.org/tech/DTD/xhtml-mobile12.dtd\">"
          "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
          "<head><title>Th&#7871; gi&#7899;i ti&#7871;ng Vi&#7879;t</title>"
          "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
          "</head><body>"
          "<h1>Danh m&#7909;c</h1>"
          "<p>Trang n&#224;y c&#243; ch&#7919; ti&#7871;ng Vi&#7879;t c&#243; d&#7845;u.</p>"
          "<ul>"
          "<li><a href=\"/vn/1.html\">M&#7909;c m&#7897;t</a></li>"
          "<li><a href=\"/vn/2.html\">M&#7909;c hai</a></li>"
          "<li><a href=\"/vn/3.html\">M&#7909;c b&#225;</a></li>"
          "</ul>"
          "<small>C&#7843;m &#417;n b&#7841;n &#273;&#7885;c!</small>"
          "</body></html>";
      } else if (feed_host && feed_rss) {
        ctype = "application/rss+xml";
        // Giong BBC: CDATA title/description + media:thumbnail self-closing
        body =
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          "<rss version=\"2.0\" xmlns:media=\"http://search.yahoo.com/mrss/\">"
          "<channel>"
          "<title><![CDATA[Qeaf Test Feed]]></title>"
          "<link>http://feed.test/</link>"
          "<description><![CDATA[Text-only news]]></description>"
          "<item>"
          "<title><![CDATA[First Story Headline]]></title>"
          "<description><![CDATA[Body of first story for readers.]]></description>"
          "<link>http://feed.test/a1?x=1&amp;y=2</link>"
          "<guid isPermaLink=\"false\">http://feed.test/a1#1</guid>"
          "<pubDate>Thu, 24 Sep 2026 10:00:00 GMT</pubDate>"
          "<media:thumbnail width=\"240\" height=\"135\" url=\"http://feed.test/t1.jpg\"/>"
          "</item>"
          "<item>"
          "<title><![CDATA[Second Story Headline]]></title>"
          "<link>http://feed.test/a2</link>"
          "<description><![CDATA[Body of second story.]]></description>"
          "<media:thumbnail width=\"240\" height=\"135\" url=\"http://feed.test/t2.jpg\"/>"
          "</item>"
          "<item>"
          "<title><![CDATA[Third Story Headline]]></title>"
          "<link>http://feed.test/a3</link>"
          "</item>"
          "</channel></rss>";
      } else if (feed_host && feed_atom) {
        ctype = "application/atom+xml";
        body =
          "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
          "<feed xmlns=\"http://www.w3.org/2005/Atom\">"
          "<title>Atom Test</title>"
          "<entry><title>Atom Entry One</title><link href=\"http://feed.test/e1\"/>"
          "<summary>Summary one.</summary></entry>"
          "<entry><title>Atom Entry Two</title><link href=\"http://feed.test/e2\"/>"
          "<summary>Summary two.</summary></entry>"
          "</feed>";
      } else if (feed_host) {
        const bool vn_page = g_req.find("GET /vn.html ") != std::string::npos;
        if (vn_page) {
          ctype = "text/html; charset=utf-8";
          // XHTML-MP tieng Viet: numeric entity &#7871; ế, &#225; á, ...
          body =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<!DOCTYPE html PUBLIC \"-//WAPFORUM//DTD XHTML Mobile 1.2//EN\" "
            "\"http://www.openmobilealliance.org/tech/DTD/xhtml-mobile12.dtd\">"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
            "<head><title>Th&#7871; gi&#7889;i ti&#7871;ng Vi&#7871;t</title>"
            "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />"
            "</head><body>"
            "<h1>Danh m&#7909;ch</h1>"
            "<p>Trang n&#225;y c&#243; ch&#7919; ti&#7871;ng Vi&#7871;t c&#243; d&#7845;u.</p>"
            "<ul>"
            "<li><a href=\"/vn/1.html\">M&#7909;c m&#7893;t</a></li>"
            "<li><a href=\"/vn/2.html\">M&#7909;c hai</a></li>"
            "<li><a href=\"/vn/3.html\">M&#7909;c b&#225;</a></li>"
            "</ul>"
            "<small>C&#7843;m &#373;&#7899;n b&#7841;n &#273;&#7885;c!</small>"
            "</body></html>";
        } else {
          body = "<wml><card title=\"Mock\"><p>No vn page</p></card></wml>";
        }
      } else if (qf_root) {
        const char *redir = "HTTP/1.1 301 Moved Permanently\r\nLocation: https://www.qeafivels.com/\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        rx.assign(redir);
        return (int)rx.size() - (int)rxp;
      } else if (qf_www) {
        ctype = "text/html; charset=utf-8";
        // Semantic test fixture. The public page title was verified live; this fixture exercises
        // the exact ESP parser/renderer without pretending the simulator has public DNS/TLS.
        body =
          "<!doctype html><html><head>"
          "<title>Qeafivels Software — Audio Tools, AI Agents, Hugging Face &amp; 2D Games</title>"
          "<style>body{font-family:sans-serif}</style><script>window.__app=1;</script>"
          "</head><body>"
          "<header><h1>Qeafivels Software</h1></header>"
          "<p>Audio Tools, AI Agents, Hugging Face &amp; 2D Games</p>"
          "<img src=\"/hero-banner.jpg\" alt=\"Qeafivels hero banner\"/>"
          "<nav><a href=\"/?category=audio\">Audio Tools</a> <a href=\"/?category=agents\">AI Agents</a> "
          "<a href=\"/?category=hf\">Hugging Face</a> <a href=\"/?category=games\">2D Games</a></nav>"
          "<section><h2>Qeafivels</h2><p>Small-screen Opera Mini 4 style view with Small Screen Rendering, Desktop overview and keypad zoom.</p>"
          "<img src=\"/products-audio.png\" alt=\"Featured product strip\"/>"
          "<p>Showing 10 products in all.</p></section>"
          "<footer><small>https://www.qeafivels.com/</small></footer>"
          "</body></html>";
      } else if (keypad && article) {
        ctype = "text/html; charset=utf-8";
        body =
          "<!doctype html><html><head><title>Article Opened</title></head><body>"
          "<h1>Article opened by OK</h1>"
          "<p>This page proves the center key opened the focused link.</p>"
          "<p><a href=\"/\">Back to keypad test home</a></p>"
          "</body></html>";
      } else if (keypad_long) {
        // Fixture cho regression cuon pixel/inertia: WML nen firmware KHONG tu bat
        // chuot ao (khong co <meta viewport>), va du dai de cuon > mot man hinh.
        ctype = "text/vnd.wap.wml";
        static std::string longpage;
        if (longpage.empty()) {
          longpage = "<wml><card title=\"Long scroll test\"><p>";
          char line[96];
          for (int i = 1; i <= 24; i++) {
            snprintf(line, sizeof line, "<a href=\"#\">Content block number %d with a long label</a><br/>", i);
            longpage += line;
          }
          longpage += "</p></card></wml>";
        }
        body = longpage.c_str();
      } else if (keypad) {
        ctype = "text/html; charset=utf-8";
        body =
          "<!doctype html><html><head><title>Keypad Web Test</title></head><body>"
          "<h1>Qeafbrowser keypad web test</h1>"
          "<p>Small-screen browser navigation for Java and Symbian style phones.</p>"
          "<p><a href=\"/article.html\">Open a long featured article link that wraps across several screen lines</a></p>"
          "<p>This is a normal paragraph block. The blue frame should hug the text without repainting it.</p>"
          "<p><a href=\"/downloads.html\">Downloads and files</a></p>"
          "<p><a href=\"https://example.com/\">External web site</a></p>"
          "<small>Use D-Pad Up Down Left Right and press OK to activate.</small>"
          "</body></html>";
      } else {
        body =
          "<wml><card title=\"Mock Server\"><p>Chao mung den Mock Server!<br/>"
          "<a href=\"mtt:start\">Ve trang chu</a><br/>"
          "<a href=\"/page2.wml\">Trang 2</a></p></card></wml>";
      }
      char hdr[320];
      int hn = sprintf(hdr, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                       ctype, (int)strlen(body));
      rx.assign(hdr, hn); rx += body;
    }
    return (int)rx.size() - (int)rxp;
  }
  // host TCP bridge (Winsock that)
  if (rxp < rx.size()) return (int)rx.size() - (int)rxp;
  if (!ok || s == INVALID_SOCKET) return 0;
  fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
  timeval tv = { 0, 200000 };
  if (select((int)s + 1, &fds, 0, 0, &tv) > 0) {
    char buf[2048];
    int r = recv(s, buf, sizeof buf, 0);
    if (r > 0) rx.append(buf, r);
    else ok = false;   // 0 = peer close, <0 = error
  }
  return (int)rx.size() - (int)rxp;
}
char WiFiClient::read() {
  if (rxp >= rx.size()) return -1;
  return (char)rx[rxp++];
}
void WiFiClient::stop() {
  if (s != INVALID_SOCKET) { closesocket(s); s = INVALID_SOCKET; }
  ok = false; rx.clear(); rxp = 0;
}

// ---------------- SimFS ----------------
bool SimFS::mkdir(const char *p) {
  std::string f = std::string(root) + p;
#ifdef _WIN32
  ::mkdir(f.c_str());
#else
  ::mkdir(f.c_str(), 0755);
#endif
  return true;
}
SimFile SimFS::open(const char *p, const char *mode) {
  std::string f = std::string(root) + p;
  if (strchr(p, '/')) {   // dam bao thu muc me
    std::string dir = root; dir += "/";
    for (const char *c = p; *c; c++) { dir += *c; if (*c == '/') {
#ifdef _WIN32
      ::mkdir(dir.c_str());
#else
      ::mkdir(dir.c_str(), 0755);
#endif
    } }
  }
  SimFile sf; sf.f = fopen(f.c_str(), mode);
  return sf;
}

// ---------------- LGFX ----------------
void LGFX::drawString(const char *s, int x, int y) {
  // font 5x7 don gian de screenshot doc duoc (sim only)
  static const unsigned char F[96][5] = {
    {0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},
    {0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},{0,0x1C,0x22,0x41,0},
    {0,0x41,0x22,0x1C,0},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},{0,0x50,0x30,0,0},
    {0x08,0x08,0x08,0x08,0x08},{0,0x60,0x60,0,0},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{0x08,0x14,0x22,0x41,0},
    {0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
    {0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
    {0,0x7F,0x41,0x41,0},{0x02,0x04,0x08,0x10,0x20},{0,0x41,0x41,0x7F,0},
    {0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},{0,0x01,0x02,0x04,0},
    {0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},
    {0x0C,0x52,0x52,0x52,0x3E},{0x7F,0x08,0x04,0x04,0x78},{0,0x44,0x7D,0x40,0},
    {0x20,0x40,0x44,0x3D,0},{0x7F,0x10,0x28,0x44,0},{0,0x41,0x7F,0x40,0},
    {0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},
    {0x48,0x54,0x54,0x54,0x20},{0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},
    {0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0,0x08,0x36,0x41,0},
    {0,0,0x7F,0,0},{0,0x41,0x36,0x08,0},{0x08,0x04,0x08,0x10,0x08},
  };
  int sx = x;
  for (int i = 0; s[i]; ) {
    unsigned char c = (unsigned char)s[i];
    // UTF-8 decode: 1 codepoint = 1 slot (hien '?' neu khong co glyph ASCII)
    int len = 1;
    unsigned int cp = c;
    if (c >= 0xC0) {
      if (c < 0xE0) { len = 2; cp = c & 0x1F; }
      else if (c < 0xF0) { len = 3; cp = c & 0x0F; }
      else { len = 4; cp = c & 0x07; }
      for (int k = 1; k < len && s[i + k]; k++) {
        unsigned char cc = (unsigned char)s[i + k];
        if ((cc & 0xC0) != 0x80) { len = k; break; }
        cp = (cp << 6) | (cc & 0x3F);
      }
    }
    i += len;
    if (cp == ' ') { sx += charW(); continue; }
    unsigned char show = (cp >= 32 && cp <= 127) ? (unsigned char)cp : '?';
    const unsigned char *g = F[show - 32];
    int scale = (font >= 4) ? 2 : 1;
    for (int col = 0; col < 5; col++) {
      unsigned char bits = g[col];
      for (int row = 0; row < 7; row++) {
        if (bits & (1 << row)) {
          if (scale == 1) drawPixel(sx + col, y + row, fg);
          else fillRect(sx + col * 2, y + row * 2, 2, 2, fg);
        }
      }
    }
    sx += charW();
  }
}
void LGFX::dumpBmp(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) return;
  int w = 240, h = 320, row = w * 3;
  unsigned char hdr[54] = {};
  hdr[0] = 'B'; hdr[1] = 'M';
  int size = 54 + row * h;
  memcpy(hdr + 2, &size, 4); hdr[10] = 54;
  hdr[14] = 40; memcpy(hdr + 18, &w, 4); memcpy(hdr + 22, &h, 4);
  hdr[26] = 1; hdr[28] = 24;
  fwrite(hdr, 1, 54, f);
  std::vector<unsigned char> line(row);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      uint16_t c = fb[y * w + x];
      int r = (c >> 11) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
      line[x * 3 + 0] = (unsigned char)b; line[x * 3 + 1] = (unsigned char)g; line[x * 3 + 2] = (unsigned char)r;
    }
    fwrite(line.data(), 1, row, f);
  }
  fclose(f);
}
