#ifndef FILE_MANAGER_APP_H
#define FILE_MANAGER_APP_H

#include "../core/AppManager.h"
#include <SD.h>

class FileManagerApp : public App {
public:
    const char* getName() override { return "Files"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;
    void end() override;

private:
    void draw();
    bool sdAvailable = false;
    
    File root;
    File currentFile;
    char currentPath[256];
    char selectedFile[64];
    int selectedIndex = 0;
    int scrollOffset = 0;
    int visibleItems = 0;
};

#endif
