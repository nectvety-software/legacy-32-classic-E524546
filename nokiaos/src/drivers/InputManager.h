#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <stdint.h>
#include <functional>

class InputManager {
public:
    static InputManager& getInstance();
    
    void begin();
    void update();
    
    bool isKeyPressed(uint8_t key);
    bool isKeyJustPressed(uint8_t key);
    bool isKeyJustReleased(uint8_t key);
    
    bool isUp() { return isKeyJustPressed(KEY_UP); }
    bool isDown() { return isKeyJustPressed(KEY_DOWN); }
    bool isLeft() { return isKeyJustPressed(KEY_LEFT); }
    bool isRight() { return isKeyJustPressed(KEY_RIGHT); }
    bool isMenu() { return isKeyJustPressed(KEY_MENU); }
    bool isOption() { return isKeyJustPressed(KEY_OPTION); }
    bool isSelect() { return isKeyJustPressed(KEY_SELECT); }
    bool isStart() { return isKeyJustPressed(KEY_START); }
    bool isA() { return isKeyJustPressed(KEY_A); }
    bool isB() { return isKeyJustPressed(KEY_B); }

    // Vai tro OS theo docs/system_prompt_phan_cung.md:
    // START=OK, A=Back, MENU=Home, B=Delete, OPTION=Context.
    bool isOK() { return isStart(); }
    bool isBack() { return isA(); }
    bool isHome() { return isMenu(); }
    bool isDelete() { return isB(); }
    bool isContext() { return isOption(); }
    // SELECT giu >600ms: doi che do Game/T9.
    bool isKeyHeldFor(uint8_t key, uint32_t ms);
    
    bool isAnyKey();
    
    void setRepeatEnabled(bool enabled) { repeatEnabled = enabled; }
    
    using KeyCallback = std::function<void(uint8_t)>;
    void onKeyPress(KeyCallback callback) { keyPressCallback = callback; }

private:
    InputManager() : lastKeyState(0), currentKeyState(0), 
                     lastChangeTime(0), repeatEnabled(false) {}
    
    static InputManager instance;
    
    uint32_t lastKeyState;
    uint32_t currentKeyState;
    uint32_t lastChangeTime;
    bool repeatEnabled;
    
    KeyCallback keyPressCallback;
    
    static const uint8_t KEY_COUNT = 10;
    static const uint32_t KEY_PINS[KEY_COUNT];
    static const uint32_t KEY_MASK[KEY_COUNT];
    uint32_t pressStartMs[KEY_COUNT];

    uint8_t keyIndex(uint8_t pin);
};

#endif
