#ifndef SETTINGS_APP_H
#define SETTINGS_APP_H

#include "../core/AppManager.h"

class SettingsApp : public App {
public:
    const char* getName() override { return "Settings"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;

private:
    void draw();
    int selectedItem = 0;
    uint8_t brightness = 128;
};

#endif
