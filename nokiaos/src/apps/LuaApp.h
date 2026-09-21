#ifndef LUA_APP_H
#define LUA_APP_H
#include "../core/AppManager.h"

class LuaApp : public App {
public:
    const char* getName() override { return "Lua"; }
    void begin() override;
    void update() override;
    bool onKey(uint8_t key) override;
private:
    void draw();
    char code[512] = "print('Hello!')";
    bool running = false;
};

#endif
