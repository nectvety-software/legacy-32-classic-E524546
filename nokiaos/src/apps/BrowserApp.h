#ifndef BROWSER_APP_H
#define BROWSER_APP_H
#include "../core/AppManager.h"
#include <WiFi.h>

class BrowserApp : public App {
public:
    const char* getName() override { return "Browser"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;
private:
    void draw();
    char url[64] = "192.168.1.1";
    int selected = 0;
};

#endif
