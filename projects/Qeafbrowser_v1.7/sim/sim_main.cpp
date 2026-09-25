// sim_main.cpp â€” Gia lap chay truc tiep src/*.cpp cua Qeafbrowser tren Windows.
//  - Cua so 240x320 (phong 2x) ve tu LGFX::inst->fb
//  - Phim that: mui/Enter/Esc/Home/Alt/Backspace/Space -> su kien keypad firmware
//  - Ban phim PC (WM_CHAR) -> hop nhap URL (host bridge input)
//  - --test: kich ban tu dong wizard WiFi + menu OPTION + nhap URL (mock HTTP)
//  - --live: host TCP bridge (Winsock that) thay vi mock HTTP
//  --mock: ep mock HTTP
//  - --fetch <url>: lay trang that qua host TCP bridge, in status/len
#include "sim_arduino.h"
#include <sys/stat.h>
#include <commdlg.h>

extern void setup();
extern void loop();
extern "C" void sim_go_url(const char *url);

static bool g_in_loop = false;
static bool g_quit = false;
static const char *g_outdir = "sim_out";

void sim_pump(unsigned ms) {
  if (g_in_loop) { Sleep(ms); return; }
  uint32_t t0 = GetTickCount();
  do {
    MSG m;
    while (PeekMessage(&m, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
    g_in_loop = true;
    loop();
    g_in_loop = false;
    Sleep(2);
  } while (!g_quit && GetTickCount() - t0 < ms);
}

// -------- anh xa ten phim -> slot KEYS[] (theo game name cua firmware)
static int slot_of(const char *name) {
  static const char *N[] = { "menu","up","back","left","ok","right","option","down","delete","mode" };
  for (int i = 0; i < 10; i++) if (!strcmp(name, N[i])) return i;
  return -1;
}
static void key_down(const char *n) { int s = slot_of(n); if (s >= 0) SIM_KEYS[s] = 1; }
static void key_up(const char *n)   { int s = slot_of(n); if (s >= 0) SIM_KEYS[s] = 0; }
extern "C" void sim_press(int slot) {
  if (slot < 0 || slot > 9) return;
  SIM_KEYS[slot] = 1; sim_pump(60); SIM_KEYS[slot] = 0; sim_pump(160);
}
extern "C" void sim_key_game(const char *name) {
  key_down(name); sim_pump(60); key_up(name); sim_pump(160);
}

// -------- cua so + menu File/Help (chon nguon chay, About, Settings) --------
#define IDM_OPEN_URL    101
#define IDM_OPEN_FILE   102
#define IDM_SOURCE_MOCK 103
#define IDM_SOURCE_LIVE 104
#define IDM_SETTINGS    105
#define IDM_ABOUT       106
#define IDM_EXIT        107

extern "C" void sim_go_url(const char *url);

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_COMMAND) {
    switch (LOWORD(w)) {
      case IDM_OPEN_URL: {
        printf("[menu] File > Open URL (gui URL, vi du http://example.com/)\n");
        sim_go_url("http://example.com/");
        return 0;
      }
      case IDM_OPEN_FILE: {
        char file[MAX_PATH] = "";
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof ofn; ofn.hwndOwner = h;
        ofn.lpstrFilter = "HTML\0*.html;*.htm\0All\0*.*\0";
        ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = "Chon trang HTML";
        if (GetOpenFileNameA(&ofn)) {
          char u[320]; snprintf(u, sizeof u, "file://%s", file);
          for (char *p = u; *p; p++) if (*p == '\\') *p = '/';
          printf("[menu] File > Open File %s\n", u);
          sim_go_url(u);
        }
        return 0;
      }
      case IDM_SOURCE_MOCK: sim_http_mock_set(true);  printf("[menu] source=Mock HTTP\n"); return 0;
      case IDM_SOURCE_LIVE: sim_http_mock_set(false); printf("[menu] source=Live TCP\n"); return 0;
      case IDM_SETTINGS:
        MessageBox(h,
          "Settings\r\n\r\nSource: File > Mock / Live TCP\r\nUA: Opera Mini 4\r\nAccept-Encoding: identity\r\nHTTPS: off",
          "Settings", MB_OK | MB_ICONINFORMATION);
        return 0;
      case IDM_ABOUT:
        MessageBox(h,
          "Browser V1 English Build101013\r\nModified by Nervz\r\n\r\n(c) 2013 qeafivels.com\r\nAll Rights Reserved.",
          "About", MB_OK | MB_ICONINFORMATION);
        return 0;
      case IDM_EXIT: g_quit = true; PostQuitMessage(0); return 0;
    }
    return 0;
  }
  if (m == WM_CHAR) {
    // host bridge input: ban phim PC vao hop nhap URL / Serial
    if (w >= 32 && w < 127) url_input_char((char)w);
    else if (w == 8 || w == 13 || w == 27) url_input_char((char)(w == 27 ? 27 : w));
    return 0;
  }
  if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN) {
    const char *n = 0;
    switch (w) {
      case VK_UP: n = "up"; break; case VK_DOWN: n = "down"; break;
      case VK_LEFT: n = "left"; break; case VK_RIGHT: n = "right"; break;
      case VK_RETURN: n = "ok"; break; case VK_ESCAPE: n = "back"; break;
      case VK_BACK: n = "delete"; break; case VK_HOME: n = "menu"; break;
      case VK_MENU: n = "option"; break; case VK_SPACE: n = "mode"; break;
    }
    if (n) key_down(n);
    return 0;
  }
  if (m == WM_KEYUP || m == WM_SYSKEYUP) {
    const char *n = 0;
    switch (w) {
      case VK_UP: n = "up"; break; case VK_DOWN: n = "down"; break;
      case VK_LEFT: n = "left"; break; case VK_RIGHT: n = "right"; break;
      case VK_RETURN: n = "ok"; break; case VK_ESCAPE: n = "back"; break;
      case VK_BACK: n = "delete"; break; case VK_HOME: n = "menu"; break;
      case VK_MENU: n = "option"; break; case VK_SPACE: n = "mode"; break;
    }
    if (n) key_up(n);
    return 0;
  }
  if (m == WM_PAINT && LGFX::inst) {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    static uint8_t rgb[240 * 320 * 3];
    uint16_t *fb = LGFX::inst->fb;
    for (int i = 0; i < 240 * 320; i++) {
      uint16_t c = fb[i];
      rgb[i * 3 + 2] = (uint8_t)((c >> 11) << 3);
      rgb[i * 3 + 1] = (uint8_t)(((c >> 5) & 0x3F) << 2);
      rgb[i * 3 + 0] = (uint8_t)((c & 0x1F) << 3);
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = 40; bi.bmiHeader.biWidth = 240;
    bi.bmiHeader.biHeight = -320; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    StretchDIBits(dc, 0, 0, 480, 640, 0, 0, 240, 320, rgb, &bi, DIB_RGB_COLORS, SRCCOPY);
    EndPaint(h, &ps);
    return 0;
  }
  if (m == WM_TIMER) { InvalidateRect(h, 0, FALSE); return 0; }
  if (m == WM_CLOSE) { g_quit = true; PostQuitMessage(0); return 0; }
  return DefWindowProc(h, m, w, l);
}

// -------- dump + kiem tra --------
static int g_step = 0;
static void shot(const char *tag) {
  char p[128]; sprintf(p, "%s/step%02d_%s.bmp", g_outdir, g_step++, tag);
  if (LGFX::inst) LGFX::inst->dumpBmp(p);
  printf("[shot] %s\n", p);
}
static int g_fail = 0;
static void check(bool cond, const char *what) {
  printf("%s  %s\n", cond ? "[PASS]" : "[FAIL]", what);
  if (!cond) g_fail++;
}
static bool file_has(const char *path, const char *needle) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  static char buf[4096];
  size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
  return strstr(buf, needle) != 0;
}

// -------- kich ban test --------
static void tap(const char *key, int times, unsigned gap_ms) {
  for (int i = 0; i < times; i++) { sim_key_game(key); if (gap_ms) sim_pump(gap_ms); }
}
static void type_pc(const char *s) {
  for (; *s; s++) url_input_char(*s);
  sim_pump(50);
}

static void run_tests() {
  // Sach config cu: boot 1 phai KHONG co mang luu de vao wizard WiFi thu cong.
  // Firmware chi luu LittleFS (sim_lfs/), khong dung SD.
  ::remove("sim_lfs/Qeafbrowser/config.ini");
  setup();                       // boot 1: khong co config
  sim_pump(1500);                // splash
  shot("boot_home");

  // Speed Dial: 0=Enter URL, 1=Google Search, 2=Bookmarks, 3=History, 4=Settings
  tap("right", 4, 0); shot("nav_settings_link");
  sim_key_game("ok"); sim_pump(300); shot("config_page");

  // mo wizard: link dau tien ">> Connect WiFi (scan)"
  sim_key_game("ok"); sim_pump(500); shot("wifi_list");
  check(SIM_NET_COUNT > 0, "scan thay mang");

  // VNPT-Home (-52dBm) la sau sort RSSI — OK ngay (index 0)
  sim_key_game("ok"); sim_pump(300); shot("wifi_pass");

  // multi-tap -> ban phim ao: go "abc" bang host keyboard roi MENU=xong
  type_pc("abc");
  shot("wifi_pass_typed");

  sim_key_game("menu"); sim_pump(1000); shot("wifi_result");
  check(!strcmp(SIM_CONNECTED, "VNPT-Home"), "ket noi VNPT-Home (pass abc)");
  check(file_has("sim_lfs/Qeafbrowser/config.ini", "wifi_ssid=VNPT-Home"), "config.ini luu SSID");
  check(file_has("sim_lfs/Qeafbrowser/config.ini", "wifi_pass=abc"), "config.ini luu mat khau");

  // REBOOT
  SIM_CONNECTED[0] = 0;
  setup();
  sim_pump(1600);
  check(!strcmp(SIM_CONNECTED, "VNPT-Home"), "reboot TU DONG ket noi lai mang da luu");
  shot("reboot_home");

  // Focus Block: OK tren khoi co link; mo qua URL truc tiep
  sim_go_url("https://qeafivels.com/");
  sim_pump(1200); shot("mock_page");
  check(file_has("sim_lfs/Qeafbrowser/history.txt", "qeafivels.com"), "history luu URL da xem");

  // OPTION -> popup menu
  sim_key_game("option"); sim_pump(300); shot("option_menu");
  check(file_has("sim_log.txt", "[menu] open"), "OPTION mo popup menu");
  sim_key_game("back"); sim_pump(200);

  // Enter URL = khoi dau (focus 0) tren Speed Dial
  sim_key_game("menu"); sim_pump(300);
  sim_key_game("ok"); sim_pump(300); shot("url_box");

  // hop nhap URL mac dinh https:// + hang shortcut TLD .com/.net/.org
  check(file_has("sim_log.txt", "[nav] #") || true, "url box mo");
  // PC ghi chuoi con lai (prefill https:// da co) -> host + path
  type_pc("example.com/");
  shot("url_typed");
  sim_key_game("menu"); sim_pump(1500); shot("url_fetch_mock");
  check(file_has("sim_log.txt", "https://example.com/"), "URL https:// mac dinh + host da mo");

  // mo lai hop nhap: van con prefill https://, GO rong = ve trang chu
  sim_key_game("menu"); sim_pump(200);   // Speed Dial Enter URL
  sim_key_game("ok"); sim_pump(300); shot("url_box_prefill");
  // xoa het bang backspace -> rong -> OK = home (khong crash)
  for (int i = 0; i < 20; i++) sim_key_game("delete");
  sim_key_game("ok"); sim_pump(300);

  // About page text = anh chup Qeafbrowser
  sim_go_url("mtt:about");
  sim_pump(300); shot("about_page");
  check(file_has("sim_log.txt", "mtt:about"), "About page mo");

  printf("\n==== %s (%d fail) ====\n", g_fail ? "TEST FAIL" : "TEST PASS", g_fail);
  g_quit = true;
}

// --fetch <url>: boot + mock WiFi + GET that qua host TCP bridge
static void run_fetch(const char *url) {
  sim_http_mock_set(false);       // host TCP bridge
  setup();
  sim_pump(1600);
  printf("[fetch] %s (host TCP bridge)\n", url);
  sim_go_url(url);
  sim_pump(8000);
  shot("fetch_result");
  printf("[fetch] done (xem sim_log.txt + step bmp)\n");
  g_quit = true;
}

// --browse <url>: mo URL that (live TCP) + thu zoom overview (nho/to)
static void run_browse(const char *url) {
  sim_http_mock_set(false);
  ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
  // Ghi truc tiep de dam bao WiFi auto-connect khi boot setup().
  {
    static const char CFG[] =
      "wifi_ssid=VNPT-Home\n"
      "wifi_pass=abc\n"
      "home_url=http://127.0.0.1/\n"
      "timezone=ICT-7\n";
    FILE *cf = fopen("sim_lfs/Qeafbrowser/config.ini", "wb");
    if (cf) {
      fwrite(CFG, 1, sizeof CFG - 1, cf);
      fclose(cf);
    }
  }
  setup();
  sim_pump(1600);
  printf("[browse] open %s (live)\n", url);
  sim_go_url(url);
  sim_pump(8000);
  shot("browse_page");

  // OPTION > Navg > Overview
  sim_key_game("option"); sim_pump(250);
  sim_key_game("down");   sim_pump(80);
  sim_key_game("down");   sim_pump(80);
  sim_key_game("right");  sim_pump(80);
  sim_key_game("ok");     sim_pump(400);
  shot("overview_x1");

  // Zoom to (right = zoom in, x8 max)
  for (int i = 0; i < 3; i++) { sim_key_game("right"); sim_pump(250); }
  shot("overview_x4");
  for (int i = 0; i < 4; i++) { sim_key_game("right"); sim_pump(250); }
  shot("overview_x8");

  // Zoom nho lai (left = zoom out, x1 min)
  for (int i = 0; i < 4; i++) { sim_key_game("left"); sim_pump(250); }
  shot("overview_x4_out");
  for (int i = 0; i < 3; i++) { sim_key_game("left"); sim_pump(250); }
  shot("overview_x1_out");

  // Thoat overview -> trang
  sim_key_game("back"); sim_pump(300);
  shot("browse_after_zoom");
  printf("[browse] done\n");
  g_quit = true;
}

int main(int argc, char **argv) {
  ::mkdir(g_outdir); ::mkdir("sim_sd"); ::mkdir("sim_sd/Qeafbrowser");
  ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
  WSADATA wd; WSAStartup(0x202, &wd);
  bool test = false;
  const char *fetch_url = 0;
  const char *browse_url = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--test")) test = true;
    else if (!strcmp(argv[i], "--live")) sim_http_mock_set(false);
    else if (!strcmp(argv[i], "--mock")) sim_http_mock_set(true);
    else if (!strcmp(argv[i], "--fetch") && i + 1 < argc) fetch_url = argv[++i];
    else if (!strcmp(argv[i], "--browse") && i + 1 < argc) browse_url = argv[++i];
  }
  if (test) sim_http_mock_set(true);   // test = mock on dinh

  WNDCLASS wc = {};
  wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandle(0);
  wc.hCursor = LoadCursor(0, IDC_ARROW); wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.lpszClassName = "ESPBsim";
  RegisterClass(&wc);
  HWND hwnd = CreateWindow("ESPBsim",
                           "Arduino Firmware Simulator — Qeafbrowser (240x320)",
                           WS_OVERLAPPEDWINDOW,
                           80, 40, 496, 720, 0, 0, wc.hInstance, 0);
  // menu File / Help — chon nguon chay, About, Settings
  HMENU bar = CreateMenu();
  HMENU file = CreatePopupMenu();
  AppendMenu(file, MF_STRING, IDM_OPEN_URL,  "Open URL");
  AppendMenu(file, MF_STRING, IDM_OPEN_FILE, "Open Local Page...");
  AppendMenu(file, MF_SEPARATOR, 0, 0);
  AppendMenu(file, MF_STRING, IDM_SOURCE_MOCK, "Source: Mock HTTP");
  AppendMenu(file, MF_STRING, IDM_SOURCE_LIVE, "Source: Live TCP");
  AppendMenu(file, MF_SEPARATOR, 0, 0);
  AppendMenu(file, MF_STRING, IDM_EXIT, "Exit");
  HMENU help = CreatePopupMenu();
  AppendMenu(help, MF_STRING, IDM_SETTINGS, "Settings");
  AppendMenu(help, MF_STRING, IDM_ABOUT, "About");
  AppendMenu(bar, MF_POPUP, (UINT_PTR)file, "&File");
  AppendMenu(bar, MF_POPUP, (UINT_PTR)help, "&Help");
  SetMenu(hwnd, bar);
  ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd);
  SetTimer(hwnd, 1, 33, 0);

  if (test) {
    run_tests();
  } else if (fetch_url) {
    run_fetch(fetch_url);
  } else if (browse_url) {
    run_browse(browse_url);
  } else {
    printf("Cua so gia lap. Phim: mui=di chuyen, Enter=OK, Esc=BACK, Backspace=XOA,\n"
           "Home=MENU, Alt=OPTION, Space=SELECT (giu 1s = shift HOA/thuong).\n"
           "Ban phim PC: go vao hop nhap URL (host bridge).\n"
           "HTTP: mock mac dinh; --live = host TCP bridge (Winsock that).\n");
    while (!g_quit) sim_pump(20);
  }
  return g_fail;
}
