#ifndef WIFI_APP_H
#define WIFI_APP_H

#include "../core/AppManager.h"
#include <WiFi.h>

class WiFiApp : public App {
public:
    const char* getName() override { return "WiFi"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;

private:
    void draw();
    void scanNetworks();
    
    enum State { STATE_LIST, STATE_CONNECTING, STATE_CONNECTED, STATE_SCANNING };
    State state = STATE_LIST;
    
    static const uint8_t MAX_NETWORKS = 20;
    String networks[MAX_NETWORKS];
    int8_t networkCount = 0;
    int8_t selectedNetwork = -1;
    int8_t scrollOffset = 0;
    
    bool connecting = false;
    uint32_t connectStartTime = 0;
};

#endif
