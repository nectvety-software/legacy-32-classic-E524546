#include "InputManager.h"
#include "SystemConfig.h"
#include <Arduino.h>

InputManager InputManager::instance;

const uint32_t InputManager::KEY_PINS[KEY_COUNT] = {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_MENU, KEY_OPTION, KEY_SELECT, KEY_START,
    KEY_A, KEY_B
};

const uint32_t InputManager::KEY_MASK[KEY_COUNT] = {
    (1u << 0), (1u << 1), (1u << 2), (1u << 3), (1u << 4),
    (1u << 5), (1u << 6), (1u << 7), (1u << 8), (1u << 9)
};

InputManager& InputManager::getInstance() {
    return instance;
}

void InputManager::begin() {
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        pinMode(KEY_PINS[i], INPUT_PULLUP);
        pressStartMs[i] = 0;
    }
    lastKeyState = 0x3FF;
    currentKeyState = 0x3FF;
}

uint8_t InputManager::keyIndex(uint8_t pin) {
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        if (KEY_PINS[i] == pin) return i;
    }
    return 0xFF;
}

void InputManager::update() {
    lastKeyState = currentKeyState;
    currentKeyState = 0;
    uint32_t now = millis();

    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        if (digitalRead(KEY_PINS[i]) == LOW) {
            currentKeyState |= KEY_MASK[i];
            if (pressStartMs[i] == 0) pressStartMs[i] = now;
        } else {
            pressStartMs[i] = 0;
        }
    }
    
    if (isAnyKey() && keyPressCallback) {
        for (uint8_t i = 0; i < KEY_COUNT; i++) {
            if ((currentKeyState & KEY_MASK[i]) && !(lastKeyState & KEY_MASK[i])) {
                keyPressCallback(KEY_PINS[i]);
                break;
            }
        }
    }
}

bool InputManager::isKeyPressed(uint8_t key) {
    uint8_t i = keyIndex(key);
    if (i >= KEY_COUNT) return false;
    return (currentKeyState & KEY_MASK[i]);
}

bool InputManager::isKeyJustPressed(uint8_t key) {
    uint8_t i = keyIndex(key);
    if (i >= KEY_COUNT) return false;
    return ((currentKeyState & KEY_MASK[i]) && !(lastKeyState & KEY_MASK[i]));
}

bool InputManager::isKeyJustReleased(uint8_t key) {
    uint8_t i = keyIndex(key);
    if (i >= KEY_COUNT) return false;
    return (!(currentKeyState & KEY_MASK[i]) && (lastKeyState & KEY_MASK[i]));
}

// Giu phim (tinh bang ms). Dung cho SELECT giu >600ms doi che do Game/T9.
bool InputManager::isKeyHeldFor(uint8_t key, uint32_t ms) {
    uint8_t i = keyIndex(key);
    if (i >= KEY_COUNT) return false;
    if (!(currentKeyState & KEY_MASK[i])) return false;
    return (millis() - pressStartMs[i] >= ms);
}

bool InputManager::isAnyKey() {
    return (currentKeyState != lastKeyState);
}
