#ifndef ABOUT_APP_H
#define ABOUT_APP_H
#include "../core/AppManager.h"

class AboutApp : public App {
public:
    const char* getName() override { return "About"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;
private:
    void draw();
};

#endif
