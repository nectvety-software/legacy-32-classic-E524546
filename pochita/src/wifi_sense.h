#ifndef POCHITA_WIFI_SENSE_H
#define POCHITA_WIFI_SENSE_H

// ============================================================================
// WiFi Sense - RuView-style RSSI heatmap for the WiFi app's Advanced menu.
//
// Real CSI (Channel State Information) is compiled out of the prebuilt ESP-IDF
// WiFi libs on this platform, so this implements RuView's "no-model fallback":
// it samples the connected AP's RSSI at high rate and turns the fluctuation
// variance into a live thermal heatmap (dark blue -> cyan -> green -> yellow ->
// red -> white), like RuView's activity map. A body moving in the room perturbs
// the RF path and makes the RSSI variance spike, which blooms a hot blob on the
// map; stillness lets the field cool back down.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "component/Display.h"
#include "component/ui_utils.h"

extern TFT_eSPI tft;

// ---- sample ring buffer ----------------------------------------------------
#define SENSE_BUF    240
#define SENSE_WIN    40      // motion window (samples)
#define SENSE_INTERVAL_MS 40 // ~25 Hz sampling

int8_t sBuf[SENSE_BUF];
int sBufCount = 0;
bool sBufFull = false;
unsigned long sLastSampleMs = 0;
unsigned long sLastPktMs = 0;   // last promiscuous packet timestamp

// ---- motion / presence state ----------------------------------------------
int sMotionLevel = 0;          // window variance mapped to 0..100
bool sPresent = false;
unsigned long sLastMotionMs = 0;
bool sCalibrating = false;
unsigned long sCalibStartMs = 0;
long sCalibVarSum = 0;
int sCalibVarN = 0;
long sBaseVar = 8;             // adaptive baseline variance (dBm^2)

void sensePushSample(int8_t v) {
  if (!sBufFull) {
    sBuf[sBufCount++] = v;
    if (sBufCount >= SENSE_BUF) sBufFull = true;
  } else {
    memmove(&sBuf[0], &sBuf[1], SENSE_BUF - 1);
    sBuf[SENSE_BUF - 1] = v;
  }
  sLastSampleMs = millis();
}

int senseBufferLen() { return sBufFull ? SENSE_BUF : sBufCount; }

// Window variance in dBm^2 over the last SENSE_WIN samples.
long senseWindowVar() {
  int end = senseBufferLen();
  int start = end - SENSE_WIN;
  if (start < 0) start = 0;
  int n = end - start;
  if (n < SENSE_WIN / 2) return 0;
  long sum = 0;
  for (int i = start; i < end; ++i) sum += sBuf[i];
  long mean = sum / n;
  long var = 0;
  for (int i = start; i < end; ++i) {
    long d = sBuf[i] - mean;
    var += d * d;
  }
  return var / n;
}

// Adaptive baseline: calibration collects the average variance of an empty
// room for ~2 s; the presence threshold becomes 3x that baseline.
void senseCalibrate() {
  sCalibrating = true;
  sCalibStartMs = millis();
  sCalibVarSum = 0;
  sCalibVarN = 0;
  sPresent = false;
}

void senseUpdateMotion() {
  long var = senseWindowVar();
  int lvl = (int)(var * 100 / 45);
  if (lvl > 100) lvl = 100;
  sMotionLevel = lvl;

  if (sCalibrating) {
    sCalibVarSum += var;
    sCalibVarN++;
    if (millis() - sCalibStartMs >= 2000) {
      sBaseVar = sCalibVarN ? (sCalibVarSum / sCalibVarN) * 3 : sBaseVar;
      if (sBaseVar < 5) sBaseVar = 5;
      sCalibrating = false;
    }
    return;
  }

  long thresh = sBaseVar > 8 ? sBaseVar : 8;
  if (var > thresh) {
    sPresent = true;
    sLastMotionMs = millis();
  }
  if (sPresent && millis() - sLastMotionMs > 4000) sPresent = false;
}

// ---- thermal heatmap -------------------------------------------------------
#define HEAT_COLS 20
#define HEAT_ROWS 20
#define HEAT_CELL 12
#define HEAT_TOP  28

uint8_t senseHeat[HEAT_COLS][HEAT_ROWS];   // 0..100 per cell
uint8_t sPrevHeat[HEAT_COLS][HEAT_ROWS];   // last rendered state (255 = dirty)
float senseBlobX = HEAT_COLS / 2.0f;
float senseBlobY = HEAT_ROWS / 2.0f;

int sLastRssi = -9999, sLastMotion = -1, sLastPres = -1;

static uint16_t sHeatPal[101];
static bool sHeatPalReady = false;

void senseHeatPaletteInit() {
  if (sHeatPalReady) return;
  struct P { uint8_t r, g, b; };
  static const P stops[] = {
    {0, 0, 0}, {0, 20, 90}, {0, 110, 230}, {0, 220, 220}, {90, 240, 90},
    {255, 235, 0}, {255, 130, 0}, {255, 60, 40}, {255, 255, 255}};
  int n = sizeof(stops) / sizeof(stops[0]);
  for (int i = 0; i < 101; ++i) {
    float t = (float)i / 100.0f * (n - 1);
    int idx = (int)t;
    if (idx > n - 2) idx = n - 2;
    float f = t - idx;
    uint8_t r = stops[idx].r + (uint8_t)((stops[idx + 1].r - stops[idx].r) * f);
    uint8_t g = stops[idx].g + (uint8_t)((stops[idx + 1].g - stops[idx].g) * f);
    uint8_t b = stops[idx].b + (uint8_t)((stops[idx + 1].b - stops[idx].b) * f);
    sHeatPal[i] = tft.color565(r, g, b);
  }
  sHeatPalReady = true;
}

uint16_t senseHeatColor(int v) {
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  return sHeatPal[v];
}

void senseHeatInjection() {
  if (sMotionLevel < 5) return;   // quiet: no new energy
  float intensity = sMotionLevel / 100.0f;
  float step = 0.4f + intensity * 1.6f;
  senseBlobX += (float)random(-100, 101) / 100.0f * step;
  senseBlobY += (float)random(-100, 101) / 100.0f * step;
  if (senseBlobX < 1) senseBlobX = 1;
  if (senseBlobX > HEAT_COLS - 2) senseBlobX = HEAT_COLS - 2;
  if (senseBlobY < 1) senseBlobY = 1;
  if (senseBlobY > HEAT_ROWS - 2) senseBlobY = HEAT_ROWS - 2;

  static const int kern[3][3] = {{40, 70, 40}, {70, 100, 70}, {40, 70, 40}};
  int cx = (int)senseBlobX, cy = (int)senseBlobY;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      int x = cx + dx, y = cy + dy;
      if (x < 0 || y < 0 || x >= HEAT_COLS || y >= HEAT_ROWS) continue;
      int add = (int)(kern[dy + 1][dx + 1] * intensity);
      if (add > 0) {
        int v = senseHeat[x][y] + add;
        senseHeat[x][y] = v > 100 ? 100 : (uint8_t)v;
      }
    }
  }
}

void senseHeatDecay() {
  for (int y = 0; y < HEAT_ROWS; ++y)
    for (int x = 0; x < HEAT_COLS; ++x) {
      int v = senseHeat[x][y] - 12;
      senseHeat[x][y] = v > 0 ? (uint8_t)v : 0;
    }
}

void senseHeatDiffuse() {
  uint8_t tmp[HEAT_COLS][HEAT_ROWS];
  for (int y = 0; y < HEAT_ROWS; ++y) {
    for (int x = 0; x < HEAT_COLS; ++x) {
      long s = senseHeat[x][y] * 2;
      int n = 2;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          int xx = x + dx, yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= HEAT_COLS || yy >= HEAT_ROWS) continue;
          s += senseHeat[xx][yy];
          n++;
        }
      }
      tmp[x][y] = (uint8_t)(s / n);
    }
  }
  memcpy(senseHeat, tmp, sizeof(senseHeat));
}

void senseHeatmapUpdate() {
  senseHeatInjection();
  senseHeatDecay();
  senseHeatDiffuse();
}

// Draw the heatmap region + status lines (called by the WiFi app's Advanced).
// Only cells whose heat value changed are repainted, so an idle map is fully
// static and the display does not flicker.
void senseRenderContent() {
  senseHeatPaletteInit();
  if (WiFi.status() != WL_CONNECTED) {
    SymbianUI::drawEmptyState(SymbianUI::ICON_WIFI, "Not connected",
                              "Connect to a WiFi network first");
    SymbianUI::drawInfoLine(230, "Hint", "RSSI heatmap needs a link");
    return;
  }

  const int y0 = HEAT_TOP;
  static const uint16_t ambient = tft.color565(0, 12, 55);
  bool first = (sPrevHeat[0][0] == 255);
  for (int y = 0; y < HEAT_ROWS; ++y) {
    for (int x = 0; x < HEAT_COLS; ++x) {
      int v = senseHeat[x][y];
      if (first || v != sPrevHeat[x][y]) {
        sPrevHeat[x][y] = v;
        tft.fillRect(x * HEAT_CELL, y0 + y * HEAT_CELL, HEAT_CELL - 1,
                     HEAT_CELL - 1, v > 0 ? senseHeatColor(v) : ambient);
      }
    }
  }

  // status lines (only repaint when the value actually changed)
  int rssi = WiFi.RSSI();
  int pres = sCalibrating ? -1 : (sPresent ? 1 : 0);
  if (rssi != sLastRssi || pres != sLastPres || sMotionLevel != sLastMotion) {
    sLastRssi = rssi;
    sLastPres = pres;
    sLastMotion = sMotionLevel;
    String motion = sCalibrating ? "CALIB..." : String(sMotionLevel) + "%";
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(sPresent ? SymbianUI::GOOD : SymbianUI::FG, SymbianUI::BG);
    tft.drawString(sCalibrating ? "Calibrating..." : (sPresent ? "PRESENT" : "CLEAR"),
                   8, 272, 2);
    tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
    tft.drawString(String(rssi) + " dBm  |  M " + motion, 8, 288, 1);
    tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("WiFi Sense", 232, 288, 1);
  }
}

// ---- lifecycle --------------------------------------------------------------
// Promiscuous RX gives per-packet RSSI in real time (station WiFi.RSSI() only
// refreshes on beacons, so it is far too sluggish for live motion).
void senseOnPkt(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!buf) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  int8_t rssi = pkt->rx_ctrl.rssi;
  if (rssi < -100) rssi = -100;
  if (rssi > -10) rssi = -10;
  sensePushSample(rssi);
  sLastPktMs = millis();
}

void senseReset() {
  sBufCount = 0;
  sBufFull = false;
  sMotionLevel = 0;
  sPresent = false;
  sCalibrating = false;
  memset(senseHeat, 0, sizeof(senseHeat));
  memset(sPrevHeat, 255, sizeof(sPrevHeat));   // force full redraw on entry
  sLastRssi = -9999;
  sLastMotion = -1;
  sLastPres = -1;
  senseBlobX = HEAT_COLS / 2.0f;
  senseBlobY = HEAT_ROWS / 2.0f;
}

void senseStart() {
  WiFi.setSleep(false);
  esp_wifi_set_promiscuous_rx_cb(&senseOnPkt);
  esp_wifi_set_promiscuous(true);
  senseReset();
  senseCalibrate();   // adapt baseline to the room (~2 s)
}

void senseTeardown() {
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(NULL);
  WiFi.setSleep(true);
  senseReset();
}

#endif
