#ifndef SDCARD_H
#define SDCARD_H

#include <SD.h>
#include <FS.h>
#include "app_registry.h"
#include "component/FileManager.h"

extern TFT_eSPI tft;
extern void drawLauncherContent();
extern SystemMode currentMode;

void initSD() {
    Serial.println("Initializing SD Card app...");
    
    if (!mountSDCard()) {
        Serial.println("SD Card not available");
        for (int i = 0; i < 3; i++) {
            delay(200);
            if (mountSDCard()) {
                Serial.println("SD Card initialized");
                break;
            }
        }
    }
    
    fileManager.begin();
    
    // Clear screen and draw File Manager UI
    tft.fillScreen(TFT_BLACK);
    fileManager.drawList();
}

void loopFileManager() {
    bool exit = false;
    fileManager.update(exit);
    
    if (exit) {
        currentMode = MODE_LAUNCHER;
        drawLauncherContent();
    }
}

void drawFileManager() {
    tft.fillScreen(TFT_BLACK);
    fileManager.drawList();
}

#endif
