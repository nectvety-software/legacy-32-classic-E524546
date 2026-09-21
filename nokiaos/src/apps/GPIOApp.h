#ifndef GPIO_APP_H
#define GPIO_APP_H

#include "../core/AppManager.h"

class GPIOApp : public App {
public:
    const char* getName() override { return "GPIO"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;

private:
    void draw();
    void setGPIOMode(int pin, const char* mode);
    
    static const uint8_t GPIO_PINS[8];
    static const char* GPIO_NAMES[8];
    
    int selectedPin = 0;
    int pinModes[8] = {0};
    bool pinStates[8] = {false};
};

#endif
