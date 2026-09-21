#pragma once
#include <Arduino.h>
#include <SD.h>
#include "UITheme.h"

// ═══════════════════════════════════════════════════════════
//  Music Player - SD Card audio player for LegacyOS
//  Audio output: I2S DAC or PWM buzzer
//  Library: ESP8266Audio (supports MP3, WAV, FLAC)
//  Install: https://github.com/earlephilhower/ESP8266Audio
// ═══════════════════════════════════════════════════════════

// I2S pins (optional - connect I2S DAC like MAX98357A)
#define I2S_BCLK   -1   // Set your pins
#define I2S_LRC    -1
#define I2S_DOUT   -1

struct Track {
  String name;
  String path;
  size_t size;
  uint32_t duration; // Estimated seconds
};

namespace AppMusicPlayer {
  static std::vector<Track> playlist;
  static int    currentTrack = 0;
  static int    scroll = 0;
  static bool   playing = false;
  static bool   paused  = false;
  static int    volume  = 75;       // 0-100
  static uint32_t playStart = 0;
  static uint32_t elapsed   = 0;
  static int    visualizer[12] = {};
  static uint32_t vizUpdate = 0;
  
  void _scanMusic() {
    playlist.clear();
    if (!SD.begin()) return;
    
    const char* exts[] = {".mp3", ".wav", ".flac", ".ogg", nullptr};
    File dir = SD.open("/music");
    if (!dir) return;
    
    File f;
    while ((f = dir.openNextFile())) {
      String name = f.name();
      bool match = false;
      for (int i = 0; exts[i]; i++) {
        if (name.endsWith(exts[i])) { match = true; break; }
      }
      if (match) {
        Track t;
        t.name = name.substring(0, name.lastIndexOf('.'));
        t.path = "/music/" + name;
        t.size = f.size();
        t.duration = t.size / 16000; // Rough estimate for 128kbps MP3
        playlist.push_back(t);
      }
      f.close();
    }
    dir.close();
    
    if (playlist.empty()) {
      // Demo tracks
      playlist.push_back({"No music files", "", 0, 0});
      playlist.push_back({"Add .mp3 to /music/", "", 0, 0});
    }
  }
  
  void _updateVisualizer() {
    if (millis() - vizUpdate < 80) return;
    vizUpdate = millis();
    for (int i = 0; i < 12; i++) {
      if (playing && !paused) {
        int target = random(2, 20);
        visualizer[i] = (visualizer[i] * 3 + target) / 4;
      } else {
        visualizer[i] = max(1, visualizer[i] - 2);
      }
    }
  }
  
  String _formatTime(uint32_t sec) {
    char buf[8];
    snprintf(buf, 8, "%d:%02d", sec/60, sec%60);
    return String(buf);
  }
  
  void render(AppContext& ctx) {
    _updateVisualizer();
    
    auto* spr = ctx.display->getContent();
    spr->fillSprite(CLR_BG);
    
    // Gradient background
    for (int y = 0; y < 80; y++) {
      uint16_t c = spr->alphaBlend(y*3, CLR_BG, CLR_APP_PURPLE);
      spr->drawFastHLine(0, y, 240, c);
    }
    
    // Title
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(MC_DATUM);
    spr->drawString("Music Player", 120, 10);
    
    // Album art placeholder (72x72 centered)
    spr->fillRoundRect(84, 18, 72, 72, 10, CLR_APP_PURPLE);
    spr->setTextSize(3); spr->setTextDatum(MC_DATUM);
    spr->drawString(playing ? "♪" : "♫", 120, 52);
    
    // Track info
    String trackName = playlist.empty() ? "No tracks" :
      (currentTrack < (int)playlist.size() ? playlist[currentTrack].name : "---");
    if (trackName.length() > 20) trackName = trackName.substring(0, 20) + "..";
    
    spr->setTextColor(CLR_WHITE); spr->setTextSize(1); spr->setTextDatum(MC_DATUM);
    spr->drawString(trackName, 120, 97);
    spr->setTextColor(CLR_GRAY2);
    char trackNum[16];
    snprintf(trackNum, 16, "%d / %d", currentTrack+1, (int)playlist.size());
    spr->drawString(trackNum, 120, 109);
    
    // Progress bar
    uint32_t dur = playlist.empty() ? 0 :
      (currentTrack < (int)playlist.size() ? playlist[currentTrack].duration : 0);
    if (playing && !paused) elapsed = (millis() - playStart) / 1000;
    
    spr->fillRect(5, 120, 230, 3, CLR_GRAY3);
    if (dur > 0) {
      int px = min(230, (int)(230 * elapsed / dur));
      spr->fillRect(5, 120, px, 3, CLR_PRIMARY);
      spr->fillCircle(5 + px, 121, 4, CLR_WHITE);
    }
    
    spr->setTextColor(CLR_GRAY2);
    spr->setTextDatum(ML_DATUM);
    spr->drawString(_formatTime(elapsed), 6, 128);
    spr->setTextDatum(MR_DATUM);
    spr->drawString(dur > 0 ? _formatTime(dur) : "--:--", 234, 128);
    
    // Visualizer bars
    for (int i = 0; i < 12; i++) {
      int bx = 10 + i * 19;
      int bh = max(2, visualizer[i]);
      uint16_t bc = playing && !paused ? 
        spr->color565(0, 180, 255 - i*15) : CLR_GRAY3;
      spr->fillRect(bx, 158 - bh, 14, bh, bc);
    }
    
    // Controls
    int cy = 170;
    // Prev
    spr->fillTriangle(28, cy+10, 28, cy+30, 15, cy+20, CLR_GRAY1);
    spr->fillRect(10, cy+10, 4, 20, CLR_GRAY1);
    // Play/Pause
    if (playing && !paused) {
      spr->fillRect(100, cy+8, 10, 24, CLR_WHITE);
      spr->fillRect(116, cy+8, 10, 24, CLR_WHITE);
    } else {
      spr->fillTriangle(100, cy+8, 100, cy+32, 128, cy+20, CLR_WHITE);
    }
    // Next
    spr->fillTriangle(172, cy+10, 172, cy+30, 185, cy+20, CLR_GRAY1);
    spr->fillRect(186, cy+10, 4, 20, CLR_GRAY1);
    // Stop
    spr->fillRect(205, cy+12, 18, 18, CLR_ACCENT);
    
    // Volume
    spr->setTextColor(CLR_GRAY2); spr->setTextSize(1); spr->setTextDatum(ML_DATUM);
    spr->drawString("Vol", 5, 212);
    UITheme::drawProgressBar(spr, 28, 209, 170, 8, volume, CLR_PRIMARY, CLR_GRAY3);
    char vbuf[6]; snprintf(vbuf, 6, "%d%%", volume);
    spr->setTextColor(CLR_GRAY1); spr->drawString(vbuf, 202, 212);
    
    // Playlist (2 rows preview)
    spr->fillRect(0, 225, 240, 1, CLR_GRAY3);
    for (int i = 0; i < 4 && (i + scroll) < (int)playlist.size(); i++) {
      int idx = i + scroll;
      bool cur = (idx == currentTrack);
      int y = 228 + i * 18;
      if (cur) {
        spr->fillRect(0, y-1, 240, 17, CLR_BG2);
        spr->setTextColor(CLR_PRIMARY);
      } else {
        spr->setTextColor(CLR_GRAY2);
      }
      spr->setTextDatum(ML_DATUM);
      spr->drawString((cur ? "▶ " : "  ") + playlist[idx].name, 4, y + 6);
    }
    
    spr->fillRect(0, 298, 240, 2, CLR_BG2);
    spr->setTextColor(CLR_GRAY3); spr->setTextDatum(MC_DATUM);
    spr->drawString("L/R:Track  A:Play  B/+VOL:-VOL", 120, 296);
    
    ctx.display->pushContent();
  }
  
  void start(AppContext& ctx) {
    playing = paused = false;
    volume = 75; elapsed = 0;
    currentTrack = scroll = 0;
    memset(visualizer, 1, sizeof(visualizer));
    _scanMusic();
    
    // Init I2S audio if pins configured
    if (I2S_BCLK >= 0) {
      // AudioOutputI2S setup would go here with ESP8266Audio
      Serial.println(F("[Music] I2S audio init - install ESP8266Audio lib"));
    } else {
      Serial.println(F("[Music] No I2S pins - set I2S_BCLK/LRC/DOUT"));
    }
  }
  
  void loop(AppContext& ctx) {
    render(ctx);
    
    if (ctx.input->left())   {
      if (currentTrack > 0) { currentTrack--; playing = false; elapsed = 0; }
    }
    if (ctx.input->right())  {
      if (currentTrack < (int)playlist.size()-1) { currentTrack++; playing = false; elapsed = 0; }
    }
    if (ctx.input->up())     { if (volume < 100) volume += 5; }
    if (ctx.input->down())   { if (volume > 0) volume -= 5; }
    if (ctx.input->start()) {
      if (!playing) {
        playing = true; paused = false;
        playStart = millis() - elapsed * 1000;
        // AudioFileSourceSD + AudioGeneratorMP3::begin() here
      } else {
        paused = !paused;
      }
    }
    if (ctx.input->b()) {
      playing = false; paused = false; elapsed = 0;
    }
    if (ctx.input->option())   { _scanMusic(); }
    if (ctx.input->b())      { ctx.os->setState(OSState::HOME_SCREEN); }
  }
}
