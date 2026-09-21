#ifndef RETRO_GO_H
#define RETRO_GO_H

#include "component/Config.h"
#include <SPI.h>
#include <SD.h>

enum EmulatorType {
    EMU_NES, EMU_SNES, EMU_GB, EMU_GBC, EMU_GBA,
    EMU_SMS, EMU_SG1000, EMU_MD, EMU_GG, EMU_COLECO,
    EMU_PCENGINE, EMU_ATARI_LYNX, EMU_DOOM, EMU_UNKNOWN
};

struct EmulatorInfo {
    const char* name;
    const char* ext;
    bool available;
};

static EmulatorInfo emulators[] = {
    {"NES", ".nes", false}, {"SNES", ".snes", false}, {"GameBoy", ".gb", false},
    {"GameBoy Color", ".gbc", false}, {"GameBoy Advance", ".gba", false},
    {"Master System", ".sms", false}, {"SG-1000", ".sg1000", false},
    {"Mega Drive", ".md", false}, {"Game Gear", ".gg", false},
    {"ColecoVision", ".col", false}, {"PC Engine", ".pce", false},
    {"Atari Lynx", ".lynx", false}, {"DOOM", ".wad", false}
};

inline EmulatorType getEmulatorType(int ft) {
    if (ft >= 10 && ft <= 22) return (EmulatorType)(ft - 10);
    return EMU_UNKNOWN;
}

inline const char* getEmulatorName(EmulatorType emu) {
    return emulators[emu].name;
}

inline bool isEmulatorAvailable(EmulatorType emu) {
    return emulators[emu].available;
}

inline void checkAvailableEmulators() {
    File root = SD.open("/retro");
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        String name = file.name(); name.toLowerCase();
        if (name.endsWith(".nes")) emulators[EMU_NES].available = true;
        else if (name.endsWith(".snes") || name.endsWith(".sfc")) emulators[EMU_SNES].available = true;
        else if (name.endsWith(".gb")) emulators[EMU_GB].available = true;
        else if (name.endsWith(".gbc")) emulators[EMU_GBC].available = true;
        else if (name.endsWith(".gba")) emulators[EMU_GBA].available = true;
        else if (name.endsWith(".sms")) emulators[EMU_SMS].available = true;
        else if (name.endsWith(".md") || name.endsWith(".gen")) emulators[EMU_MD].available = true;
        else if (name.endsWith(".gg")) emulators[EMU_GG].available = true;
        else if (name.endsWith(".col")) emulators[EMU_COLECO].available = true;
        else if (name.endsWith(".pce")) emulators[EMU_PCENGINE].available = true;
        else if (name.endsWith(".lynx")) emulators[EMU_ATARI_LYNX].available = true;
        else if (name.endsWith(".wad")) emulators[EMU_DOOM].available = true;
        file.close(); file = root.openNextFile();
    }
    root.close();
}

inline void runRetroGoEmulator(String romPath, int fileType) {
    EmulatorType emuType = getEmulatorType(fileType);
    
    tft.fillScreen(COLOR_BLACK);
    
    // Status Bar - Nokia Symbian style
    tft.fillRect(0, 0, 240, 20, 0x8410);
    tft.drawLine(0, 19, 240, 19, 0x4208);
    tft.setTextColor(0x0000);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("RETRO-GO", 5, 10, 1);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN);
    tft.drawString(getEmulatorName(emuType), UiLayout::CENTER_X, 50, 2);
    
    tft.setTextColor(TFT_WHITE);
    tft.drawString(UiLayout::ellipsize(romPath, 30), UiLayout::CENTER_X, 75, 1);
    
    if (isEmulatorAvailable(emuType)) {
        tft.setTextColor(TFT_GREEN);
        tft.drawString("Ready!", UiLayout::CENTER_X, 105, 1);
        
        tft.setTextColor(0x2145);
        tft.drawString("Loading...", UiLayout::CENTER_X, 125, 1);
        
        int barX = 40; int barY = 160; int barW = 160; int barH = 15;
        tft.drawRect(barX, barY, barW, barH, 0xFBE0);
        
        for (int i = 0; i <= 100; i += 5) {
            int w = ::map(i, 0, 100, 0, barW - 4);
            tft.fillRect(barX + 2, barY + 2, w, barH - 4, TFT_GREEN);
            delay(20);
        }
        
        tft.fillScreen(COLOR_BLACK);
        tft.setTextColor(TFT_RED);
        tft.drawString("GAME STARTED!", UiLayout::CENTER_X, 100, 2);
        tft.setTextColor(TFT_SILVER);
        tft.drawString("Press B to exit", UiLayout::CENTER_X, 140, 1);
        
        while (true) {
            buttonManager.update();
            if (buttonManager.isJustPressed(KEY_A)) {
                break;
            }
            delay(10);
        }
    } else {
        tft.setTextColor(TFT_RED);
        tft.drawString("Emulator Not Found!", UiLayout::CENTER_X, 110, 2);
        
        tft.setTextColor(TFT_SILVER);
        tft.drawString("Install Retro-Go", UiLayout::CENTER_X, 135, 1);
        tft.drawString("to play this game", UiLayout::CENTER_X, 150, 1);
        
        tft.setTextColor(0x2145);
        tft.drawString("retro-go.github.io", UiLayout::CENTER_X, 180, 1);
        
        delay(3000);
    }
    
    fileManager.drawList();
}

#endif
