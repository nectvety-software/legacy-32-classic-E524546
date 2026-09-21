#include "FileManagerApp.h"
#include "../gui/UIRenderer.h"
#include "../gui/Theme.h"

extern TFT_eSPI tft;

void FileManagerApp::begin() {
    sdAvailable = SD.begin(SD_CS);
    strcpy(currentPath, "/");
    selectedIndex = 0;
    scrollOffset = 0;
    visibleItems = (SCREEN_HEIGHT - TITLE_HEIGHT - STATUS_HEIGHT - 30) / 35;
    
    if (sdAvailable) {
        root = SD.open(currentPath);
    }
    draw();
}

void FileManagerApp::end() {
    if (root) root.close();
    if (currentFile) currentFile.close();
}

void FileManagerApp::update() {
    if (!sdAvailable) return;
    
    InputManager& input = InputManager::getInstance();
    input.update();
    
    if (input.isUp() && selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
        draw();
    }
    else if (input.isDown() && selectedIndex < getFileCount() - 1) {
        selectedIndex++;
        if (selectedIndex >= scrollOffset + visibleItems) {
            scrollOffset = selectedIndex - visibleItems + 1;
        }
        draw();
    }
    else if (input.isStart()) {
        openSelectedFile();
    }
}

bool FileManagerApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        if (strcmp(currentPath, "/") != 0) {
            goUp();
        } else {
            AppManager::getInstance().goBack();
        }
        return true;
    }
    return false;
}

void FileManagerApp::openSelectedFile() {
    root.rewindDirectory();
    File entry;
    int idx = 0;
    
    while ((entry = root.openNextFile())) {
        if (idx == selectedIndex) {
            if (entry.isDirectory()) {
                strcpy(currentPath, entry.name());
                selectedIndex = 0;
                scrollOffset = 0;
            }
            entry.close();
            break;
        }
        idx++;
        entry.close();
    }
    draw();
}

void FileManagerApp::goUp() {
    if (strcmp(currentPath, "/") != 0) {
        char* lastSlash = strrchr(currentPath, '/');
        if (lastSlash) {
            *lastSlash = '\0';
        }
        if (strlen(currentPath) == 0) {
            strcpy(currentPath, "/");
        }
        selectedIndex = 0;
        scrollOffset = 0;
        draw();
    }
}

int FileManagerApp::getFileCount() {
    root.rewindDirectory();
    int count = 0;
    while (root.openNextFile()) count++;
    root.rewindDirectory();
    return count;
}

void FileManagerApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    ui.drawTitleBar(currentPath);
    
    if (!sdAvailable) {
        ui.drawTextCentered("SD Card Error", SCREEN_HEIGHT / 2 - 20, COLOR_ERROR);
        return;
    }
    
    root.rewindDirectory();
    File entry;
    int displayIdx = 0;
    
    while ((entry = root.openNextFile()) && displayIdx < visibleItems + scrollOffset) {
        if (displayIdx >= scrollOffset) {
            int visIdx = displayIdx - scrollOffset;
            bool isSelected = (displayIdx == selectedIndex);
            ui.drawListItem(visIdx, entry.name(), entry.isDirectory() ? "[DIR]" : "[FILE]", isSelected);
        }
        displayIdx++;
        entry.close();
    }
    
    ui.drawStatusBar();
    char info[32];
    snprintf(info, sizeof(info), "Items: %d", getFileCount());
    ui.drawText(info, 5, SCREEN_HEIGHT - STATUS_HEIGHT + 3, COLOR_TEXT_DIM);
}
