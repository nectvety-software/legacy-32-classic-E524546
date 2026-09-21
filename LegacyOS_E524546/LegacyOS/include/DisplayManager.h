#pragma once
#include <TFT_eSPI.h>
#include "HardwareConfig.h"
#include "UITheme.h"

// ═══════════════════════════════════════════════════════════
//  DisplayManager - ST7789 240x320 with Sprite Buffering
// ═══════════════════════════════════════════════════════════

class DisplayManager {
public:
  TFT_eSPI    tft;
  TFT_eSprite statusSprite;   // 240x20 status bar
  TFT_eSprite contentSprite;  // 240x300 content area
  
  DisplayManager() : tft(), statusSprite(&tft), contentSprite(&tft) {}
  
  void init() {
    HW::initBacklight();
    
    tft.init();
    tft.setRotation(0);       // Portrait 240x320
    tft.fillScreen(CLR_BG);
    tft.setTextColor(CLR_WHITE, CLR_BG);
    
    // Create sprites in PSRAM
    statusSprite.setColorDepth(16);
    statusSprite.createSprite(SCREEN_WIDTH, STATUS_BAR_H);
    
    contentSprite.setColorDepth(16);
    contentSprite.createSprite(SCREEN_WIDTH, CONTENT_H);
    
    Serial.println(F("[Display] ST7789 240x320 initialized"));
    Serial.printf("[Display] PSRAM sprites: %d bytes\n",
      SCREEN_WIDTH * STATUS_BAR_H * 2 + SCREEN_WIDTH * CONTENT_H * 2);
  }
  
  void setBacklight(uint8_t v) { HW::setBacklight(v); }
  
  // Clear content area sprite
  void clearContent(uint16_t color = CLR_BG) {
    contentSprite.fillSprite(color);
  }
  
  // Clear status bar sprite
  void clearStatus(uint16_t color = CLR_STATUS_BG) {
    statusSprite.fillSprite(color);
  }
  
  // Push status sprite to display
  void pushStatus() {
    statusSprite.pushSprite(0, 0);
  }
  
  // Push content sprite to display
  void pushContent() {
    contentSprite.pushSprite(0, STATUS_BAR_H);
  }
  
  // Push both
  void flip() {
    statusSprite.pushSprite(0, 0);
    contentSprite.pushSprite(0, STATUS_BAR_H);
  }
  
  // Direct draw helpers (on content sprite)
  TFT_eSprite* getContent() { return &contentSprite; }
  TFT_eSprite* getStatus()  { return &statusSprite; }
  TFT_eSPI*    getTFT()     { return &tft; }
  
  // Transition effects
  void fadeIn(uint8_t steps = 8) {
    for (int i = 0; i <= BACKLIGHT_MAX; i += BACKLIGHT_MAX/steps) {
      HW::setBacklight(i);
      delay(20);
    }
    HW::setBacklight(BACKLIGHT_MAX);
  }
  
  void fadeOut(uint8_t steps = 8) {
    for (int i = BACKLIGHT_MAX; i >= 0; i -= BACKLIGHT_MAX/steps) {
      HW::setBacklight(i);
      delay(20);
    }
    HW::setBacklight(0);
  }
  
  void slideTransition(uint16_t bgColor, bool fromRight = true) {
    // Quick slide effect
    TFT_eSprite slide(&tft);
    slide.setColorDepth(16);
    slide.createSprite(SCREEN_WIDTH, CONTENT_H);
    slide.fillSprite(bgColor);
    
    int dir = fromRight ? 1 : -1;
    for (int x = SCREEN_WIDTH * dir; abs(x) > 2; x = x * 6 / 7) {
      contentSprite.pushSprite(x, STATUS_BAR_H);
      slide.pushSprite(x - SCREEN_WIDTH * dir, STATUS_BAR_H);
      delay(8);
    }
    contentSprite.pushSprite(0, STATUS_BAR_H);
    slide.deleteSprite();
  }
  
  // Boot splash
  void showBootSplash() {
    tft.fillScreen(CLR_BLACK);
    
    // Animated logo
    for (int r = 0; r <= 30; r += 2) {
      tft.drawCircle(120, 130, r, CLR_PRIMARY);
      delay(20);
    }
    tft.fillCircle(120, 130, 30, CLR_PRIMARY);
    
    // OS name
    tft.setTextColor(CLR_WHITE);
    tft.setTextSize(3);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Legacy", 120, 180);
    
    tft.setTextSize(2);
    tft.setTextColor(CLR_PRIMARY);
    tft.drawString("OS v1.0", 120, 205);
    
    tft.setTextSize(1);
    tft.setTextColor(CLR_GRAY2);
    tft.drawString("E524546 Classic", 120, 225);
    tft.drawString("ESP32-S3 N16R8", 120, 237);
  }
  
  // Loading bar
  void showBootProgress(int percent, const char* msg) {
    tft.fillRect(20, 270, 200, 8, CLR_GRAY3);
    tft.fillRect(20, 270, 200 * percent / 100, 8, CLR_PRIMARY);
    tft.fillRect(20, 285, 200, 10, CLR_BLACK);
    tft.setTextColor(CLR_GRAY1);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(msg, 120, 290);
  }
  
  // Notification toast at bottom
  void showToast(const String& msg, uint16_t duration = 2000) {
    TFT_eSprite toast(&tft);
    toast.setColorDepth(16);
    toast.createSprite(220, 28);
    toast.fillRoundRect(0, 0, 220, 28, 8, CLR_GRAY3);
    toast.setTextColor(CLR_WHITE);
    toast.setTextSize(1);
    toast.setTextDatum(ML_DATUM);
    toast.drawString(msg, 8, 14);
    
    // Slide up
    for (int y = SCREEN_HEIGHT; y > SCREEN_HEIGHT - 40; y -= 3) {
      toast.pushSprite(10, y);
      delay(10);
    }
    delay(duration);
    // Slide down
    for (int y = SCREEN_HEIGHT - 40; y < SCREEN_HEIGHT; y += 3) {
      tft.fillRect(10, y, 220, 28, CLR_BG);
      toast.pushSprite(10, y);
      delay(10);
    }
    tft.fillRect(10, SCREEN_HEIGHT - 40, 220, 40, CLR_BG);
    toast.deleteSprite();
  }
  
  // Dim screen after inactivity
  void dimScreen() { HW::setBacklight(BACKLIGHT_DIM); }
  void wakeScreen() { HW::setBacklight(BACKLIGHT_MAX); }
  
  int width()  { return SCREEN_WIDTH; }
  int height() { return SCREEN_HEIGHT; }
};
