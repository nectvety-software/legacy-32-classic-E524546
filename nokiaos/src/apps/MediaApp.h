#ifndef MEDIA_APP_H
#define MEDIA_APP_H

#include "../core/AppManager.h"
#include <SD.h>

class MediaApp : public App {
public:
    const char* getName() override { return "Media"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;
    void end() override;

private:
    void draw();
    bool isPlaying = false;
    int selectedFile = 0;
    int fileCount = 0;
    File root;
};

#endif
