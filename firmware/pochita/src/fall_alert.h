#ifndef POCHITA_FALL_ALERT_H
#define POCHITA_FALL_ALERT_H

// ============================================================================
// Fall Alert - RF fall detection for the WiFi app's Advanced menu.
//
// Adapts the idea from bakhtiyorjondadajonov/fall-detection-vison to the
// radio-only sensors this device has (no camera): ESPectre watches the Wi-Fi
// CSI subcarrier energy, WiFi Sense watches the per-packet RSSI variance.
// Both feed one motion level; a state machine detects the fall signature
//
//     NORMAL -> (motion burst) -> FALLING -> (still afterwards) -> FALLEN
//
// and on FALLEN the device broadcasts a BLE advertisement ("POCHITA-FALL",
// manufacturer payload "FALL") that any nearby phone / ESP32 scanner can pick
// up, shows a persistent red alert with a 30 s timer, and - if an AI model is
// currently loaded - generates a short analysis message.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include "component/Display.h"
#include "component/ui_utils.h"
#include "espectre.h"
#include "wifi_sense.h"
#include "ai_chat.h"

extern TFT_eSPI tft;
extern RouterState routerState;
extern int routerSel;
extern void drawRouterApp();

// ---- parameters -------------------------------------------------------------
constexpr int FALL_BURST_MS   = 900;   // sustained motion = impact/fall
constexpr int FALL_STILL_MS   = 2500;  // stillness after burst = fallen
constexpr int FALL_RESET_S    = 30;    // persistent alert duration
constexpr int FALL_SAMPLE_MS  = 40;    // RSSI sample interval (~25 Hz)
constexpr int FALL_UPDATE_MS  = 60;

enum FallPhase { FALL_NORMAL, FALL_FALLING, FALL_STILL, FALL_FALLEN };

// ---- state ------------------------------------------------------------------
FallPhase fallPhase = FALL_NORMAL;
unsigned long fallBurstStartMs = 0;   // when the motion burst began
unsigned long fallStillStartMs = 0;   // when stillness began (after burst)
unsigned long fallFallenMs = 0;       // when the alert was raised
int fallAiTried = 0;                  // 0 = not yet, 1 = generating, 2 = done
String fallAiText = "";

// ---- BLE broadcast ----------------------------------------------------------
static BLEAdvertising* fallAdvert = nullptr;
static bool fallBleInit = false;
static bool fallAdvertising = false;

void fallBleStart() {
  if (fallAdvertising) return;
  if (!fallBleInit) {
    BLEDevice::init("POCHITA-FALL");
    fallBleInit = true;
  }
  fallAdvert = BLEDevice::getAdvertising();
  fallAdvert->stop();
  BLEAdvertisementData adv;
  adv.setName("POCHITA-FALL");
  adv.setManufacturerData(std::string("\xEF\xFE", 2) + std::string("FALL"));
  fallAdvert->setAdvertisementData(adv);
  fallAdvert->setScanResponseData(adv);
  fallAdvert->start();
  fallAdvertising = true;
  Serial.println("[FALL] BLE alert advertising");
}

void fallBleStop() {
  if (fallAdvertising) {
    if (fallAdvert) fallAdvert->stop();
    fallAdvertising = false;
  }
}

// ---- motion fusion ----------------------------------------------------------
// espMotion/espScore come from the ESPectre CSI (or its RSSI fallback)
// pipeline; sMotionLevel/sPresent come from the WiFi Sense buffer fed by the
// per-packet RSSI values sampled below.
bool fallMotionNow() {
  if (espMotion) return true;
  if (sPresent) return true;
  if (espScore > 0.55f) return true;
  if (sMotionLevel > 55) return true;
  return false;
}

// ---- AI message -------------------------------------------------------------
static void fallAiSink(const char* text, void* ctx) {
  (void)ctx;
  if (text && *text) fallAiText += text;
}

void fallAiRun() {
  fallAiTried = 1;
  if (!aiModelLoaded()) {
    fallAiText = "AI offline: load a model in the terminal (ai load) first.";
    fallAiTried = 2;
    return;
  }
  const char* prompt =
    "A fall was detected by the radio sensors (CSI + RSSI motion). "
    "Write a very short alert message in Vietnamese (under 80 chars) "
    "for a caregiver telling them a person may have fallen.";
  fallAiText = "";
  aiGenerate(prompt, 48, fallAiSink, nullptr);
  if (fallAiText.length() == 0) fallAiText = "Fall alert - check on the person.";
  fallAiTried = 2;
}

// ---- state machine ----------------------------------------------------------
void fallUpdateState() {
  unsigned long now = millis();
  bool motion = fallMotionNow();

  switch (fallPhase) {
    case FALL_NORMAL:
      if (motion) {
        fallPhase = FALL_FALLING;
        fallBurstStartMs = now;
        fallStillStartMs = 0;
      }
      break;
    case FALL_FALLING:
      if (motion) {
        if (now - fallBurstStartMs >= FALL_BURST_MS) {
          // A prolonged burst is a fall-like impact: wait for stillness.
          fallPhase = FALL_STILL;
          fallStillStartMs = now;
        }
      } else {
        // Brief blip only - back to normal.
        fallPhase = FALL_NORMAL;
      }
      break;
    case FALL_STILL:
      if (!motion) {
        if (now - fallStillStartMs >= FALL_STILL_MS) {
          fallPhase = FALL_FALLEN;
          fallFallenMs = now;
          fallAiTried = 0;
          fallAiText = "";
          fallBleStart();
          Serial.println("[FALL] FALLEN - alert raised");
        }
      } else {
        // Person moved again - no fall.
        fallPhase = FALL_NORMAL;
      }
      break;
    case FALL_FALLEN:
      if (motion) {
        // Person got up / was rescued - clear the alert.
        fallPhase = FALL_NORMAL;
        fallBleStop();
        Serial.println("[FALL] cleared - motion resumed");
      } else if (now - fallFallenMs >= (unsigned long)(FALL_RESET_S * 1000UL)) {
        fallPhase = FALL_NORMAL;
        fallBleStop();
        Serial.println("[FALL] alert auto-reset after 30s");
      }
      break;
  }
}

// ---- UI ---------------------------------------------------------------------
void fallDrawChrome() {
  SymbianUI::drawChrome("Fall Alert", "Calib", "Back");
}

void fallRender() {
  if (fallPhase == FALL_FALLEN) {
    // ---- full red alert overlay ----
    tft.fillScreen(SymbianUI::BAD);
    tft.setTextColor(TFT_WHITE, SymbianUI::BAD);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("!!! FALL DETECTED !!!", 120, 60, 2);
    long secs = (millis() - fallFallenMs) / 1000;
    if (secs < 0) secs = 0;
    tft.drawString(String(secs) + " s FALLEN", 120, 96, 2);
    tft.drawString("BLE alert broadcasting", 120, 130, 1);
    if (fallAiTried == 1) {
      tft.drawString("AI analyzing...", 120, 158, 1);
    } else {
      String t = fallAiText;
      if (t.length() > 30) t = t.substring(0, 30);
      tft.drawString(UiLayout::ellipsize(t, 26), 120, 158, 1);
    }
    tft.drawString("A to acknowledge", 120, 210, 1);
    tft.drawString("B to exit", 120, 232, 1);
    return;
  }

  fallDrawChrome();
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.drawString("RF fall detection (CSI + RSSI)", 8, 30, 1);

  const char* phaseName = "NORMAL";
  uint16_t phaseColor = SymbianUI::GOOD;
  if (fallPhase == FALL_FALLING) { phaseName = "FALLING"; phaseColor = SymbianUI::WARN; }
  else if (fallPhase == FALL_STILL) { phaseName = "STILL?"; phaseColor = SymbianUI::WARN; }

  tft.setTextColor(phaseColor, SymbianUI::BG);
  tft.drawString(phaseName, 8, 52, 2);

  // motion level bar
  int level = sMotionLevel;
  if (espScore > 0) level = max(level, (int)(espScore * 100));
  if (espMotion) level = max(level, 70);
  tft.fillRect(8, 78, 224, 8, SymbianUI::MUTED);
  int bw = level * 224 / 100;
  if (bw > 224) bw = 224;
  tft.fillRect(8, 78, bw, 8, level > 55 ? SymbianUI::BAD : SymbianUI::ACCENT);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString("Motion " + String(level) + "%", 8, 94, 1);

  // sensor status
  tft.setTextColor(SymbianUI::FG, SymbianUI::BG);
  tft.drawString(espRssiMode ? "Sensor: RSSI" : "Sensor: CSI", 8, 116, 2);
  tft.setTextColor(SymbianUI::DIM, SymbianUI::BG);
  tft.drawString("Frames " + String(espTotal) + "  |  RSSI var " + String(sMotionLevel) + "%",
                 8, 136, 1);

  SymbianUI::drawSectionLabel(170, "Detection flow");
  SymbianUI::drawInfoLine(200, "1. Motion burst", String(FALL_BURST_MS / 1000) + "." + String(FALL_BURST_MS % 1000 / 100) + "s");
  SymbianUI::drawInfoLine(222, "2. Still after burst", String(FALL_STILL_MS / 1000) + "s");
  SymbianUI::drawInfoLine(244, "3. Alert", String(FALL_RESET_S) + "s persistent");
  SymbianUI::drawInfoLine(266, "BLE", "Broadcast FALL");
}

// ---- lifecycle --------------------------------------------------------------
void fallTeardown() {
  fallBleStop();
  espectreTeardown();
  fallPhase = FALL_NORMAL;
  fallAiText = "";
  fallAiTried = 0;
}

bool fallStart() {
  fallTeardown();
  if (!espectreStart()) return false;
  senseReset();
  senseCalibrate();
  fallPhase = FALL_NORMAL;
  fallRender();
  return true;
}

void fallLoop() {
  unsigned long now = millis();
  static unsigned long lastSample = 0, lastUpd = 0, lastRender = 0;

  // Feed the WiFi Sense buffer with live station RSSI (promiscuous is owned
  // by ESPectre's RSSI fallback, so poll here like the heatmap does).
  if (WiFi.status() == WL_CONNECTED && now - lastSample >= FALL_SAMPLE_MS) {
    lastSample = now;
    if (millis() - sLastPktMs > 250) {
      int8_t v = (int8_t)WiFi.RSSI();
      if (v < -100) v = -100;
      if (v > -10) v = -10;
      sensePushSample(v);
    }
  }

  if (now - lastUpd >= FALL_UPDATE_MS) {
    lastUpd = now;
    espectreUpdate();
    espectreCountFps();
    senseUpdateMotion();
    fallUpdateState();
  }

  // One-shot AI generation when the alert fires (blocking, capped tokens).
  if (fallPhase == FALL_FALLEN && fallAiTried == 0) fallAiRun();

  if (now - lastRender >= 120) {
    lastRender = now;
    fallRender();
  }

  if (isBackPressed()) {
    fallTeardown();
    routerState = ROUTER_ADVANCED;
    routerSel = 0;
    drawRouterApp();
    delay(200);
    return;
  }
  if (isSelectPressed()) {
    if (fallPhase == FALL_FALLEN) {
      fallPhase = FALL_NORMAL;
      fallBleStop();
      Serial.println("[FALL] alert acknowledged");
      fallRender();
    }
    delay(200);
    return;
  }
  if (digitalRead(KEY_OPTION) == LOW) {
    espectreBeginCalib();
    senseCalibrate();
    fallPhase = FALL_NORMAL;
    fallAiText = "";
    fallAiTried = 0;
    delay(200);
    return;
  }
  delay(8);
}

#endif