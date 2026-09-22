#ifndef POCHITA_ESPECTRE_H
#define POCHITA_ESPECTRE_H

// ============================================================================
// ESPectre - Wi-Fi CSI motion detector for the WiFi app's Advanced menu.
//
// A moving body perturbs the multipath channel between the router and the
// device, which shows up in the Wi-Fi CSI (Channel State Information)
// amplitudes. ESPectre (francescopace/espectre) watches those amplitudes to
// sense presence. This builds on the same idea using the ESP-IDF CSI API
// shipped in this SDK (CONFIG_ESP32_WIFI_CSI_ENABLED=1): the average energy of
// the subcarrier complex samples is tracked per received frame, calibrated
// against a quiet-room baseline, and any sustained deviation is reported as
// MOTION with a live score, an FPS counter and a scrolling energy chart.
//
// Requires an active STA connection (CSI needs Wi-Fi to be started). If no CSI
// frames arrive (some radios/channels), it degrades to the per-packet RSSI
// variance used by WiFi Sense and shows "RSSI" as the mode.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <math.h>
#include "component/Display.h"
#include "component/ui_utils.h"

extern TFT_eSPI tft;
extern RouterState routerState;
extern int routerSel;
extern void drawRouterApp();

// ---- shared state (produced in the Wi-Fi task, consumed in loop()) ----------
constexpr int ESP_BUF = 240;          // energy ring capacity
constexpr int ESP_WIN = 40;           // frames used for the motion window mean
constexpr int ESP_CALIB_MS = 8000;    // quiet-room calibration time
constexpr float ESP_MOTION_K = 2.5f;  // threshold = baseline + K * baseline std

volatile float espEnergy[ESP_BUF];
volatile int espWrite = 0;      // next ring slot
volatile int espTotal = 0;      // total frames pushed
volatile bool espHasCsi = false;
volatile unsigned long espLastFrameMs = 0;
volatile int espFrameTick = 0;  // for the FPS counter

// detection / calibration (loop task)
bool espRssiMode = false;       // CSI produced nothing -> RSSI fallback
bool espCalib = true;
unsigned long espCalibStartMs = 0;
float espAcc = 0, espAcc2 = 0;
long espAccN = 0;
float espBase = 0, espBaseStd = 0, espThresh = 0;
float espScore = 0;
bool espMotion = false;
int espMotionTicks = 0;
int espFps = 0;
unsigned long espFpsMs = 0;

int espBufSize() { return espTotal > ESP_BUF ? ESP_BUF : espTotal; }

void espectrePushEnergy(float e) {
  espEnergy[espWrite] = e;
  espWrite = (espWrite + 1) % ESP_BUF;
  espTotal++;
  espFrameTick++;
  espLastFrameMs = millis();
  if (espCalib) {
    espAcc += e;
    espAcc2 += e * e;
    espAccN++;
  }
}

// ---- Wi-Fi task callbacks (keep these short, no TFT drawing) ----------------
void espectreCsiCb(void *ctx, wifi_csi_info_t *info) {
  if (!info || !info->buf || info->len < 2) return;
  espHasCsi = true;
  int8_t *p = info->buf;
  int start = info->first_word_invalid ? 4 : 0;
  long sum2 = 0;
  int used = 0;
  for (int i = start; i + 1 < (int)info->len; i += 2) {
    sum2 += (long)p[i] * p[i] + (long)p[i + 1] * p[i + 1];
    used++;
  }
  if (used <= 0) return;
  espectrePushEnergy((float)sum2 / (float)used);
}

void espectreRssiCb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (!buf) return;
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  int8_t rssi = pkt->rx_ctrl.rssi;
  if (rssi < -100) rssi = -100;
  if (rssi > -10) rssi = -10;
  espectrePushEnergy(100.0f + rssi);
}

// ---- detection --------------------------------------------------------------
void espectreBeginCalib() {
  espCalib = true;
  espCalibStartMs = millis();
  espAcc = 0;
  espAcc2 = 0;
  espAccN = 0;
  espScore = 0;
  espMotion = false;
  espMotionTicks = 0;
  espFpsMs = millis();
}

void espectreUpdate() {
  int sz = espBufSize();
  if (sz < ESP_WIN) return;

  float wm = 0;
  {
    int w = espWrite;
    float sum = 0;
    for (int i = 0; i < ESP_WIN; ++i) {
      int idx = (w - ESP_WIN + i + ESP_BUF * 2) % ESP_BUF;
      sum += espEnergy[idx];
    }
    wm = sum / ESP_WIN;
  }

  if (espCalib) {
    if (millis() - espCalibStartMs >= ESP_CALIB_MS && espAccN > 50) {
      float mean = espAcc / (float)espAccN;
      float var = espAcc2 / (float)espAccN - mean * mean;
      if (var < 0) var = 0;
      espBase = mean;
      espBaseStd = sqrtf(var);
      espThresh = mean + max(ESP_MOTION_K * sqrtf(var), mean * 0.02f);
      espCalib = false;
    }
    return;
  }

  float dev = fabsf(wm - espBase);
  float span = espThresh - espBase;
  if (span <= 0) span = 1.0f;
  espScore = dev / span;
  if (espScore > 1.0f) espScore = 1.0f;

  if (wm > espThresh) {
    espMotionTicks++;
    if (espMotionTicks >= 2) espMotion = true;
  } else {
    espMotionTicks--;
    if (espMotionTicks <= 0) {
      espMotionTicks = 0;
      espMotion = false;
    }
  }
}

void espectreCountFps() {
  if (millis() - espFpsMs >= 1000) {
    espFps = espFrameTick;
    espFrameTick = 0;
    espFpsMs += 1000;
  }
}

// ---- chart (PSRAM sprite, pushed whole to avoid flicker) --------------------
constexpr int ESP_CHART_W = 224;
constexpr int ESP_CHART_H = 172;
constexpr int ESP_CHART_X = 8;
constexpr int ESP_CHART_Y = 118;
constexpr int ESP_BARS = 112;

TFT_eSprite *espChartSpr = nullptr;

void espectreEnsureChart() {
  if (espChartSpr) return;
  espChartSpr = new TFT_eSprite(&tft);
  if (!espChartSpr || !espChartSpr->createSprite(ESP_CHART_W, ESP_CHART_H)) {
    if (espChartSpr) {
      delete espChartSpr;
      espChartSpr = nullptr;
    }
  }
}

void espectreRenderChart() {
  espectreEnsureChart();
  if (!espChartSpr) return;
  espChartSpr->fillSprite(SymbianUI::BG);

  int sz = espBufSize();
  if (sz >= 2) {
    int D = sz / ESP_BARS;
    if (D < 1) D = 1;
    int n = sz / D;
    if (n > ESP_BARS) n = ESP_BARS;
    int w = espWrite;
    float colVals[ESP_BARS];
    float vmin = 1e9f, vmax = -1e9f;
    for (int c = 0; c < n; ++c) {
      int idx = (w - (n - c) * D + ESP_BUF * 2) % ESP_BUF;
      float e = espEnergy[idx];
      colVals[c] = e;
      if (e < vmin) vmin = e;
      if (e > vmax) vmax = e;
    }
    float span = vmax - vmin;
    if (span < 1e-3f) span = 1e-3f;
    for (int c = 0; c < n; ++c) {
      int bh = (int)((colVals[c] - vmin) / span * ESP_CHART_H);
      if (bh < 1) bh = 1;
      if (bh > ESP_CHART_H) bh = ESP_CHART_H;
      espChartSpr->fillRect(c * 2, ESP_CHART_H - bh, 2, bh, SymbianUI::ACCENT);
    }
    if (!espCalib) {
      int thY = ESP_CHART_H - (int)((espThresh - vmin) / span * ESP_CHART_H);
      if (thY < 0) thY = 0;
      if (thY > ESP_CHART_H - 1) thY = ESP_CHART_H - 1;
      espChartSpr->drawFastHLine(0, thY, ESP_CHART_W, SymbianUI::BAD);
    }
  }
  espChartSpr->pushSprite(ESP_CHART_X, ESP_CHART_Y);
}

// ---- screen -----------------------------------------------------------------
void espectreDraw() {
  SymbianUI::drawChrome("ESPectre", "Calib", "Back");
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.drawString("Wi-Fi CSI motion sensing", 8, 30, 1);

  // chart frame
  tft.drawRect(ESP_CHART_X - 1, ESP_CHART_Y - 1, ESP_CHART_W + 2,
               ESP_CHART_H + 2, SymbianUI::DIVIDER);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString("channel energy (scrolling)", ESP_CHART_X, ESP_CHART_Y - 8, 1);
}

void espectreRender() {
  // status text
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.fillRect(8, 42, 224, 18, SymbianUI::BG);
  if (espCalib) {
    tft.setTextColor(SymbianUI::WARN, SymbianUI::BG);
    tft.drawString("CALIBRATING", 8, 50, 2);
  } else {
    tft.setTextColor(espMotion ? SymbianUI::BAD : SymbianUI::GOOD, SymbianUI::BG);
    tft.drawString(espMotion ? "MOTION" : "IDLE", 8, 50, 2);
  }

  // score / calibration bar
  tft.fillRect(8, 64, 224, 8, SymbianUI::MUTED);
  int barW = espCalib ? (int)((millis() - espCalibStartMs) * 224 / ESP_CALIB_MS)
                      : (int)(espScore * 224);
  if (barW > 224) barW = 224;
  if (barW < 0) barW = 0;
  tft.fillRect(8, 64, barW, 8,
               espCalib ? SymbianUI::WARN
                        : (espMotion ? SymbianUI::BAD : SymbianUI::ACCENT));

  // info lines
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  String score = espCalib ? String("Calibrate ") + String(ESP_CALIB_MS / 1000) + "s"
                          : String("Score ") + String((int)(espScore * 100)) + "%";
  tft.drawString(score, 8, 84, 2);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString("Frames " + String(espTotal) + "  |  " + String(espFps) + " Hz",
                 8, 102, 1);
  tft.drawString(String(espRssiMode ? "Mode RSSI" : "Mode CSI") +
                     "  |  Thresh " + String((long)espThresh),
                 8, 116, 1);
  (void)espBase;
  (void)espBaseStd;
}

// ---- lifecycle --------------------------------------------------------------
void espectreTeardown() {
  esp_wifi_set_csi(false);
  esp_wifi_set_csi_rx_cb(NULL, nullptr);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(NULL);
  if (espChartSpr) {
    delete espChartSpr;
    espChartSpr = nullptr;
  }
  espWrite = 0;
  espTotal = 0;
  espHasCsi = false;
  espRssiMode = false;
  espCalib = true;
  espMotion = false;
}

// Returns false when the Wi-Fi stack rejected CSI (e.g. not started).
bool espectreStart() {
  espectreTeardown();
  wifi_csi_config_t cfg = {true, true, true, true, false, false, 0};
  if (esp_wifi_set_csi_config(&cfg) != ESP_OK) return false;
  esp_wifi_set_csi_rx_cb(&espectreCsiCb, nullptr);
  if (esp_wifi_set_csi(true) != ESP_OK) {
    esp_wifi_set_csi_rx_cb(NULL, nullptr);
    return false;
  }
  espectreBeginCalib();
  espectreDraw();
  espectreRender();
  return true;
}

void espectreLoop() {
  unsigned long now = millis();
  static unsigned long lastUpd = 0, lastChart = 0;

  // No CSI frames after 3s -> degrade to the RSSI promiscuous fallback.
  if (!espRssiMode && espTotal == 0 && now - espCalibStartMs >= 3000) {
    esp_wifi_set_csi(false);
    esp_wifi_set_csi_rx_cb(NULL, nullptr);
    esp_wifi_set_promiscuous_rx_cb(&espectreRssiCb);
    esp_wifi_set_promiscuous(true);
    espRssiMode = true;
    espectreBeginCalib();
  }

  if (now - lastUpd >= 60) {
    lastUpd = now;
    espectreUpdate();
    espectreCountFps();
  }
  if (now - lastChart >= 120) {
    lastChart = now;
    espectreRender();
    espectreRenderChart();
  }

  if (isBackPressed()) {
    espectreTeardown();
    routerState = ROUTER_ADVANCED;
    routerSel = 0;
    drawRouterApp();
    delay(200);
    return;
  }
  if (digitalRead(KEY_OPTION) == LOW) {
    espectreBeginCalib();
    delay(200);
    return;
  }
  delay(8);
}

#endif
