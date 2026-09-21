#ifndef BLUETOOTH_APP_H
#define BLUETOOTH_APP_H

#include "../core/AppManager.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

class BluetoothApp : public App {
public:
    const char* getName() override { return "Bluetooth"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;

private:
    void draw();
    enum State { STATE_MAIN, STATE_SCANNING, STATE_CONNECTED };
    State state = STATE_MAIN;
    
    bool bleServerRunning = false;
    int selectedMenu = 0;
};

#endif
