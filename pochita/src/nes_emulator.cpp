// nes_emulator.cpp
//
// PochitaOS OSD (operating-system-dependent) bridge for the vendored nofrendo
// NES core in lib/nofrendo. It implements every osd_* entry point the core
// expects and a viddriver_t, mapping:
//   - video  -> LovyanGFX (global `tft`), 8bpp indexed -> RGB565, blit per frame
//   - input  -> ButtonManager (NES pad + MENU to pause + OPTION to quit)
//   - audio  -> stubbed (no sound this iteration)
//   - memory -> ROM image loaded from SD into PSRAM
//   - timing -> esp_timer periodic tick at NES_REFRESH_RATE (60 Hz)
//
// nofrendo is GPL-2.0; this bridge is distributed under the same terms.

#include <Arduino.h>
#include <SD.h>
#include <stdio.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

// Pulls in Config.h -> Display.h (TFT_eSPI), BoardPins.h (KEY_*), tft, buttonManager.
#include "component/ButtonManager.h"
#include "nes_emulator.h"

// nofrendo portable core (C). Wrapped so the C++ compiler emits/consumes C ABI.
extern "C" {
#include <noftypes.h>
#include <nofrendo.h>
#include <nofconfig.h>
#include <event.h>
#include <log.h>
#include <gui.h>
#include <bitmap.h>
#include <vid_drv.h>
#include <osd.h>
#include <nes.h>
#include <nes_pal.h>
#include <nesinput.h>
}

// memguard.h (included via noftypes.h) redefines malloc/free/strdup as macros
// that route into nofrendo's own allocator. Undo them so the allocations below
// use the real heap / PSRAM allocators.
#ifdef malloc
#undef malloc
#endif
#ifdef free
#undef free
#endif
#ifdef strdup
#undef strdup
#endif

// ----------------------------------------------------------------------------
// Geometry
// ----------------------------------------------------------------------------
#define NES_W 256
#define NES_H NES_SCREEN_HEIGHT /* 240 - full rendered height (PPU draws 0..239) */

// ----------------------------------------------------------------------------
// Bridge state
// ----------------------------------------------------------------------------
static uint8_t  *g_romData = nullptr; // .nes image in PSRAM (owned here)
static uint16_t *g_frame = nullptr;   // RGB565 scratch frame in PSRAM
static uint16_t  g_palette[256];      // NES palette -> RGB565
static int       g_x0 = 0, g_y0 = 0;  // blit origin on the (rotated) panel
static bitmap_t *g_lockBmp = nullptr; // wrapper returned by lock_write()

static bool      g_prevBtn[8] = {false, false, false, false,
                                 false, false, false, false};
static bool      g_optPrev = false;
static bool      g_menuPrev = false;

// ---- background gamepad poller (retro-go style) ----
// The core only reads input once per *rendered* frame, so with slow software
// emulation quick taps were missed by the per-frame buttonManager debounce
// (game felt unresponsive). Poll the buttons on a dedicated task every 10 ms
// with a shift-register debounce, fully decoupled from emulation speed.
static const int s_nesPins[10] = {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_OPTION, KEY_A, KEY_B,
    KEY_START, KEY_MENU, KEY_SELECT,
};
static volatile uint8_t s_btnDebounced[10] = {0};
static uint8_t          s_btnHist[10] = {0};
static volatile bool    s_inputRun = false;

static void nes_input_task(void *arg) {
  (void)arg;
  for (int i = 0; i < 10; i++) {
    s_btnHist[i] = 0;
    s_btnDebounced[i] = 0;
  }
  while (s_inputRun) {
    for (int i = 0; i < 10; i++) {
      bool pressed = (digitalRead(s_nesPins[i]) == LOW);  // active low
      uint8_t hist = (uint8_t)((s_btnHist[i] << 1) | (pressed ? 1u : 0u));
      s_btnHist[i] = hist;
      if ((hist & 0x07) == 0x07)          // 3 stable reads pressed
        s_btnDebounced[i] = 1;
      else if ((hist & 0x03) == 0x00)     // 2 stable reads released
        s_btnDebounced[i] = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  vTaskDelete(NULL);
}

static esp_timer_handle_t g_timer = nullptr;
static void (*g_timerFunc)(void) = nullptr;

// ============================================================================
// Video driver
// ============================================================================
extern "C" {

static int nes_vid_init(int width, int height) {
  (void)width;
  (void)height;
  return 0;
}

static void nes_vid_shutdown(void) {}

static int nes_vid_setmode(int width, int height) {
  (void)width;
  (void)height;
  return 0;
}

// NES 6-bit color -> RGB565.
static void nes_vid_setpalette(rgb_t *pal) {
  for (int i = 0; i < 256; i++) {
    g_palette[i] = (uint16_t)(((pal[i].r >> 3) << 11) |
                              ((pal[i].g >> 2) << 5) |
                              (pal[i].b >> 3));
  }
}

static void nes_vid_clear(uint8 color) { (void)color; }

// Only invoked by vid_findmode() during init (dimensions probe / clear). Back
// it with the RGB565 scratch buffer, which is large enough for an 8bpp frame.
static bitmap_t *nes_vid_lockwrite(void) {
  g_lockBmp = bmp_createhw((uint8 *)g_frame, NES_W, NES_H, NES_W);
  return g_lockBmp;
}

static void nes_vid_freewrite(int num_dirties, rect_t *dirty_rects) {
  (void)num_dirties;
  (void)dirty_rects;
  if (g_lockBmp) bmp_destroy(&g_lockBmp);
}

// Receives the real 256x224 8bpp primary buffer each frame.
static void nes_vid_customblit(bitmap_t *bmp, int num_dirties,
                               rect_t *dirty_rects) {
  (void)num_dirties;
  (void)dirty_rects;
  if (!g_frame || !bmp) return;

  const int h = (bmp->height < NES_H) ? bmp->height : NES_H;
  const int w = (bmp->width < NES_W) ? bmp->width : NES_W;
  for (int y = 0; y < h; y++) {
    const uint8_t *src = bmp->line[y];
    uint16_t *dst = g_frame + (size_t)y * NES_W;
    for (int x = 0; x < w; x++) dst[x] = g_palette[src[x]];
  }
  tft.pushImage(g_x0, g_y0, NES_W, NES_H, g_frame);
}

static viddriver_t pochitaDriver = {
    "PochitaOS LovyanGFX",  // name
    nes_vid_init,           // init
    nes_vid_shutdown,       // shutdown
    nes_vid_setmode,        // set_mode
    nes_vid_setpalette,     // set_palette
    nes_vid_clear,          // clear
    nes_vid_lockwrite,      // lock_write
    nes_vid_freewrite,      // free_write
    nes_vid_customblit,     // custom_blit
    false                   // invalidate
};

void osd_getvideoinfo(vidinfo_t *info) {
  info->default_width = NES_W;
  info->default_height = NES_H;
  info->driver = &pochitaDriver;
}

// ============================================================================
// Audio (stubbed - no output this iteration)
// ============================================================================
static void (*s_audioCb)(void *, int) = nullptr;

void osd_setsound(void (*playfunc)(void *buffer, int size)) {
  s_audioCb = playfunc;
}

void osd_getsoundinfo(sndinfo_t *info) {
  info->sample_rate = 22050;
  info->bps = 16;
}

// ============================================================================
// Timer - drive the core tick counter at `frequency` Hz.
// ============================================================================
static void nes_timer_trampoline(void *arg) {
  (void)arg;
  if (g_timerFunc) g_timerFunc();
}

int osd_installtimer(int frequency, void *func, int funcsize, void *counter,
                     int countersize) {
  (void)funcsize;
  (void)counter;
  (void)countersize;

  g_timerFunc = (void (*)(void))func;

  if (g_timer) {
    esp_timer_stop(g_timer);
    esp_timer_delete(g_timer);
    g_timer = nullptr;
  }

  const esp_timer_create_args_t args = {
      .callback = &nes_timer_trampoline,
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "nofrendo60",
      .skip_unhandled_events = true,
  };
  if (esp_timer_create(&args, &g_timer) != ESP_OK) return -1;
  esp_timer_start_periodic(g_timer, 1000000UL / (frequency > 0 ? frequency : 60));
  return 0;
}

// ============================================================================
// Input
// ============================================================================

// In-game pause menu (MENU button). Blocks the frame loop until the player
// resumes or quits, so the game is frozen while the overlay is up.
static int s_pauseSel = 0;

static void nesDrawPauseOverlay() {
  const int cw = 220, ch = 106;
  const int cx = (tft.width() - cw) / 2;
  const int cy = tft.height() - ch - 10;
  tft.fillRoundRect(cx, cy, cw, ch, 8, SymbianUI::BG);
  tft.drawRoundRect(cx, cy, cw, ch, 8, SymbianUI::ACCENT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SymbianUI::ACCENT, SymbianUI::BG);
  tft.drawString("PAUSED", tft.width() / 2, cy + 16, 2);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString("Arrows: move", tft.width() / 2, cy + 36, 1);
  tft.drawString("A/B: action   START/SEL: sys", tft.width() / 2, cy + 50, 1);
  const char *opts[] = {"Resume", "Exit game"};
  for (int i = 0; i < 2; i++) {
    int y = cy + 64 + i * 19;
    bool sel = (i == s_pauseSel);
    tft.fillRoundRect(cx + 16, y, cw - 32, 17, 4,
                      sel ? SymbianUI::ACCENT : SymbianUI::BG);
    tft.drawRoundRect(cx + 16, y, cw - 32, 17, 4,
                      sel ? SymbianUI::ACCENT : SymbianUI::DIVIDER);
    tft.setTextColor(sel ? SymbianUI::ON_ACCENT : SymbianUI::FG,
                     sel ? SymbianUI::ACCENT : SymbianUI::BG);
    tft.drawString(opts[i], tft.width() / 2, y + 9, 2);
  }
}

static void nesPauseMenu() {
  s_pauseSel = 0;
  nesDrawPauseOverlay();
  while (true) {
    buttonManager.update();
    if (buttonManager.isJustPressed(KEY_UP) ||
        buttonManager.isJustPressed(KEY_DOWN) ||
        buttonManager.isJustPressed(KEY_LEFT) ||
        buttonManager.isJustPressed(KEY_RIGHT)) {
      s_pauseSel = 1 - s_pauseSel;
      nesDrawPauseOverlay();
    }
    if (buttonManager.isJustPressed(KEY_START)) {
      if (s_pauseSel == 1) {
        event_t q = event_get(event_quit);
        if (q) q(INP_STATE_MAKE);
      }
      return;
    }
    if (buttonManager.isJustPressed(KEY_A)) {
      return;  // resume
    }
    if (buttonManager.isJustPressed(KEY_OPTION)) {
      event_t q = event_get(event_quit);
      if (q) q(INP_STATE_MAKE);
      return;
    }
    delay(15);
  }
}

void osd_getinput(void) {
  // OPTION quits the emulator and returns to the OS.
  if (s_btnDebounced[8] && !g_optPrev) {
    g_optPrev = true;
    event_t q = event_get(event_quit);
    if (q) q(INP_STATE_MAKE);
    return;
  }
  if (!s_btnDebounced[8]) g_optPrev = false;

  // MENU pauses the game (frozen) with a Resume/Exit overlay.
  if (s_btnDebounced[9] && !g_menuPrev) {
    g_menuPrev = true;
    nesPauseMenu();
    return;
  }
  if (!s_btnDebounced[9]) g_menuPrev = false;

  static const int kEv[8] = {event_joypad1_up,     event_joypad1_down,
                             event_joypad1_left,   event_joypad1_right,
                             event_joypad1_a,      event_joypad1_b,
                             event_joypad1_start,  event_joypad1_select};

  for (int i = 0; i < 8; i++) {
    bool now = s_btnDebounced[i];
    if (now != g_prevBtn[i]) {
      event_t e = event_get(kEv[i]);
      if (e) e(now ? INP_STATE_MAKE : INP_STATE_BREAK);
      g_prevBtn[i] = now;
    }
  }
}

void osd_getmouse(int *x, int *y, int *button) {
  (void)x;
  (void)y;
  (void)button;
}

// ============================================================================
// Init / shutdown / misc
// ============================================================================
int osd_init(void) {
  for (int i = 0; i < 8; i++) g_prevBtn[i] = false;
  g_optPrev = false;
  g_menuPrev = false;
  return 0;
}

void osd_shutdown(void) { s_audioCb = nullptr; }

static char s_configName[] = "na";

int osd_main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  config.filename = s_configName;
  // We already know this is a NES ROM, so skip autodetection (which would try
  // to fopen the path) and go straight to the NES machine.
  return main_loop("rom", system_nes);
}

void osd_fullname(char *fullname, const char *shortname) {
  strncpy(fullname, shortname, PATH_MAX);
}

char *osd_newextension(char *string, char *ext) {
  (void)ext;
  return string;
}

int osd_makesnapname(char *filename, int len) {
  (void)filename;
  (void)len;
  return -1;
}

// The core asks the OSD for the raw ROM bytes rather than reading the file.
char *osd_getromdata(void) { return (char *)g_romData; }

}  // extern "C"

// ============================================================================
// Public entry point
// ============================================================================
static void nes_error_splash(const char *msg) {
  tft.fillScreen(0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_RED);
  tft.drawString("NES", tft.width() / 2, tft.height() / 2 - 20, 2);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2 + 10, 1);
  delay(1800);
}

// The nofrendo core (6502 + PPU + mappers) uses far more stack than the
// Arduino loop() task provides (~8 KB), which panics/reboots the device.
// Run it in a dedicated task with a large stack instead.
static volatile bool s_emuDone = false;

static void nofrendo_task(void *arg) {
  (void)arg;
  nofrendo_main(0, NULL);
  s_emuDone = true;
  vTaskDelete(NULL);
}

bool runNesEmulator(const String &romPath) {
  // ---- load ROM from SD into PSRAM ----
  File f = SD.open(romPath.c_str(), FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    nes_error_splash("ROM open failed");
    return false;
  }

  size_t sz = f.size();
  if (sz < 16 || sz > (4UL * 1024UL * 1024UL)) {
    f.close();
    nes_error_splash("Bad ROM size");
    return false;
  }

  g_romData = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
  if (!g_romData) g_romData = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_8BIT);
  if (!g_romData) {
    f.close();
    nes_error_splash("Out of memory (ROM)");
    return false;
  }

  size_t rd = 0;
  while (rd < sz) {
    size_t chunk = (sz - rd) > 4096 ? 4096 : (sz - rd);
    int n = f.read(g_romData + rd, chunk);
    if (n <= 0) break;
    rd += (size_t)n;
  }
  f.close();
  if (rd != sz) {
    heap_caps_free(g_romData);
    g_romData = nullptr;
    nes_error_splash("ROM read error");
    return false;
  }

  // ---- RGB565 scratch frame ----
  const size_t frameBytes = (size_t)NES_W * NES_H * sizeof(uint16_t);
  g_frame = (uint16_t *)heap_caps_malloc(frameBytes, MALLOC_CAP_SPIRAM);
  if (!g_frame) g_frame = (uint16_t *)heap_caps_malloc(frameBytes, MALLOC_CAP_8BIT);
  if (!g_frame) {
    heap_caps_free(g_romData);
    g_romData = nullptr;
    nes_error_splash("Out of memory (VRAM)");
    return false;
  }

  // ---- display: portrait (rotation 0, same as the OS/.bin/image preview) ----
  // NES frame is 256 wide but the portrait panel is only 240 wide, so the
  // origin is negative on X and pushImage crops 8px off each side (centered).
  uint8_t oldRot = tft.getRotation();
  tft.setRotation(0);  // 240x320
  tft.setSwapBytes(true);
  tft.fillScreen(0);
  g_x0 = (tft.width() - NES_W) / 2;    // (240-256)/2 = -8 -> crop 8px each side
  g_y0 = (tft.height() - NES_H) / 2;   // (320-240)/2 = 40
  if (g_y0 < 0) g_y0 = 0;

  // The core's main loop blocks and busy-waits between 60 Hz ticks; keep the
  // task/idle watchdogs quiet for the duration of the session.
  disableCore0WDT();
  disableCore1WDT();
  disableLoopWDT();

  // ---- run (blocks until OPTION is pressed) ----
  // Run the core in a dedicated task with a big stack (the loop() task's
  // ~8 KB is not enough for the 6502/PPU call chain). Pin to core 1 so it
  // does not fight WiFi/BT on core 0. Wait here until it exits.
  // A background task polls the buttons every 10 ms so input stays fresh no
  // matter how slowly the emulation renders (see osd_getinput).
  s_inputRun = true;
  TaskHandle_t inputTask = nullptr;
  xTaskCreatePinnedToCore(nes_input_task, "nesinput", 4096, NULL, 4,
                          &inputTask, 0);
  s_emuDone = false;
  TaskHandle_t emuTask = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(nofrendo_task, "nofrendo", 24576,
                                          NULL, 5, &emuTask, 1);
  if (ok != pdPASS) {
    enableLoopWDT();
    enableCore0WDT();
    enableCore1WDT();
    if (g_frame) { heap_caps_free(g_frame); g_frame = nullptr; }
    if (g_romData) { heap_caps_free(g_romData); g_romData = nullptr; }
    tft.setSwapBytes(false);
    tft.setRotation(oldRot);
    tft.fillScreen(0);
    nes_error_splash("Task create failed");
    return false;
  }
  while (!s_emuDone) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  s_inputRun = false;
  vTaskDelay(pdMS_TO_TICKS(40));  // let nofrendo + input tasks self-delete

  // ---- restore watchdogs ----
  enableLoopWDT();
  enableCore0WDT();
  enableCore1WDT();

  // ---- tear down core resources main_loop() leaves allocated on return ----
  if (g_timer) {
    esp_timer_stop(g_timer);
    esp_timer_delete(g_timer);
    g_timer = nullptr;
  }
  vid_shutdown();
  gui_shutdown();
  log_shutdown();

  if (g_frame) {
    heap_caps_free(g_frame);
    g_frame = nullptr;
  }
  if (g_romData) {
    heap_caps_free(g_romData);
    g_romData = nullptr;
  }

  // ---- restore display for the OS UI ----
  tft.setSwapBytes(false);
  tft.setRotation(oldRot);
  tft.fillScreen(0);
  return true;
}
