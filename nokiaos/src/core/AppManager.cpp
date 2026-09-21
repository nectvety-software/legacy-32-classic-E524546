#include "AppManager.h"
#include <Arduino.h>

AppManager AppManager::instance;

AppManager& AppManager::getInstance() {
    return instance;
}

void AppManager::registerApp(App* app, AppID id) {
    if (appCount < MAX_APPS) {
        apps[appCount] = app;
        appIds[appCount] = id;
        appCount++;
    }
}

App* AppManager::getAppById(AppID id) {
    for (uint8_t i = 0; i < appCount; i++) {
        if (appIds[i] == id) {
            return apps[i];
        }
    }
    return nullptr;
}

void AppManager::launchApp(AppID id) {
    if (currentApp != APP_NONE) {
        App* app = getAppById(currentApp);
        if (app) app->end();
    }
    
    currentApp = id;
    App* app = getAppById(id);
    if (app) {
        app->begin();
    }
}

void AppManager::closeCurrentApp() {
    if (currentApp != APP_NONE) {
        App* app = getAppById(currentApp);
        if (app) app->end();
        currentApp = APP_NONE;
    }
}

void AppManager::update() {
    App* app = getAppById(currentApp);
    if (app) {
        app->update();
    }
}

void AppManager::handleInput() {
    App* app = getAppById(currentApp);
    if (app) {
        InputManager& input = InputManager::getInstance();
        input.update();

        // SELECT giu >600ms: doi che do Game/T9 (theo system_prompt_phan_cung.md).
        static bool t9mode = false;
        static uint32_t lastModeToggle = 0;
        if (input.isKeyHeldFor(KEY_SELECT, 600) &&
            millis() - lastModeToggle > 1000) {
            lastModeToggle = millis();
            t9mode = !t9mode;
        }
        (void)t9mode;
                
        uint32_t keyState = 0;
        if (input.isUp()) keyState = KEY_UP;
        else if (input.isDown()) keyState = KEY_DOWN;
        else if (input.isLeft()) keyState = KEY_LEFT;
        else if (input.isRight()) keyState = KEY_RIGHT;
        else if (input.isStart()) keyState = KEY_START;   // OK (phim giua)
        else if (input.isA()) keyState = KEY_A;           // Back
        else if (input.isMenu()) keyState = KEY_MENU;     // Home
        else if (input.isB()) keyState = KEY_B;           // Delete
        else if (input.isOption()) keyState = KEY_OPTION; // Context
        
        if (keyState && !app->onKey(keyState)) {
            if (keyState == KEY_A || keyState == KEY_MENU) {
                goBack();
            }
        }
    }
}

void AppManager::goBack() {
    closeCurrentApp();
}
