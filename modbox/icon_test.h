// ========================================
// ICON TEST FUNCTION
// ========================================

#include <SPIFFS.h>
#include <FS.h>

void testIconDisplay() {
    Serial.println("Testing icon display...");

    // Clear screen
    gfx->fillScreen(COLOR_BG);

    // Test if icons are loaded
    IconManager& iconMgr = IconManager::getInstance();

    if (!iconMgr.hasIcons()) {
        gfx->setTextColor(COLOR_RED);
        gfx->setCursor(10, 50);
        gfx->print("ERROR: Icons not loaded!");
        gfx->setCursor(10, 80);
        gfx->print("Check SPIFFS file: /assets/epd_bitmap_.bin");
        return;
    }

    // Display all icons in a grid
    int iconsPerRow = 3;
    int iconSpacing = 70;
    int startX = 10;
    int startY = 10;

    const char* iconNames[] = {"WiFi", "BLE", "Web", "SD", "Paint", "Terminal", "Script", "Retro", "Setup"};
    const uint16_t* iconPointers[] = {
        epd_bitmap_wifi, epd_bitmap_ble, epd_bitmap_web, epd_bitmap_sd,
        epd_bitmap_paint, epd_bitmap_terminal, epd_bitmap_script,
        epd_bitmap_retro, epd_bitmap_setup
    };

    for (int i = 0; i < 9; i++) {
        int row = i / iconsPerRow;
        int col = i % iconsPerRow;

        int x = startX + col * iconSpacing;
        int y = startY + row * iconSpacing;

        // Draw icon
        if (iconPointers[i] && iconPointers[i][0] != 0) {
            // Direct pixel drawing as fallback
            for (int py = 0; py < 48; py++) {
                for (int px = 0; px < 48; px++) {
                    uint16_t color = iconPointers[i][py * 48 + px];
                    if (color != 0x0000) {
                        gfx->drawPixel(x + px, y + py, color);
                    }
                }
            }

            // Draw label
            gfx->setTextColor(COLOR_WHITE);
            gfx->setTextSize(1);
            gfx->setCursor(x, y + 52);
            gfx->print(iconNames[i]);
        } else {
            // Draw error indicator
            gfx->setTextColor(COLOR_RED);
            gfx->setCursor(x, y + 20);
            gfx->print("NULL");
        }
    }

    // Status info
    gfx->setTextColor(COLOR_GREEN);
    gfx->setCursor(10, 280);
    gfx->print("Icon Test - Press MENU to exit");
}

// ========================================
// SPIFFS FILE CHECK FUNCTION
// ========================================

void checkSPIFFSFiles() {
    Serial.println("Checking SPIFFS files...");

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS init failed!");
        return;
    }

    Serial.println("SPIFFS contents:");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();

    while (file) {
        Serial.print("FILE: ");
        Serial.print(file.name());
        Serial.print(" SIZE: ");
        Serial.println(file.size());
        file = root.openNextFile();
    }

    // Check specific file
    if (SPIFFS.exists("/assets/epd_bitmap_.bin")) {
        Serial.println("Found /assets/epd_bitmap_.bin");
        File iconFile = SPIFFS.open("/assets/epd_bitmap_.bin", "r");
        if (iconFile) {
            Serial.print("File size: ");
            Serial.println(iconFile.size());
            iconFile.close();
        }
    } else {
        Serial.println("NOT FOUND: /assets/epd_bitmap_.bin");
    }

    if (SPIFFS.exists("/epd_bitmap_.bin")) {
        Serial.println("Found /epd_bitmap_.bin");
    } else {
        Serial.println("NOT FOUND: /epd_bitmap_.bin");
    }
}