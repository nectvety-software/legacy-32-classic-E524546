#pragma once
#include <Arduino.h>
#include <FS.h>
#include <TFT_eSPI.h>

class ImageViewerService {
public:
  bool drawFile(TFT_eSPI &tft, fs::FS &fs, const String &path,
                int x, int y, int w, int h, String &error);

private:
  bool drawBmp(TFT_eSPI &tft, fs::FS &fs, const String &path,
               int x, int y, int w, int h, String &error);
  bool drawJpeg(TFT_eSPI &tft, fs::FS &fs, const String &path,
                int x, int y, int w, int h, String &error);
  bool drawPng(TFT_eSPI &tft, fs::FS &fs, const String &path,
               int x, int y, int w, int h, String &error);
};
