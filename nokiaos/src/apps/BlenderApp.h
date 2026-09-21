#ifndef BLENDER_APP_H
#define BLENDER_APP_H
#include "../core/AppManager.h"

class BlenderApp : public App {
public:
    const char* getName() override { return "Blender"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;
private:
    void draw();
    float rotation = 0;
    int selected = 0;
};

#endif
