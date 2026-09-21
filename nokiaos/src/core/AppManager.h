#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include "SystemConfig.h"
#include "UIRenderer.h"

class App {
public:
    virtual ~App() {}
    virtual const char* getName() = 0;
    virtual void begin() {}
    virtual void update() {}
    virtual void end() {}
    virtual bool onKey(uint8_t key) { return false; }
};

class AppManager {
public:
    static AppManager& getInstance();
    
    void registerApp(App* app, AppID id);
    void launchApp(AppID id);
    void closeCurrentApp();
    AppID getCurrentApp() { return currentApp; }
    
    void update();
    void handleInput();
    
    void goBack();
    
private:
    AppManager() : currentApp(APP_NONE), currentIndex(0) {}
    
    static AppManager instance;
    
    App* apps[MAX_APPS];
    AppID appIds[MAX_APPS];
    uint8_t appCount;
    
    AppID currentApp;
    uint8_t currentIndex;
    
    App* getAppById(AppID id);
};

#endif
