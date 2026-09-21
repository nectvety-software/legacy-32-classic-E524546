#ifndef KEIRA_H
#define KEIRA_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

#ifndef BTN_UP
#define BTN_UP    40
#define BTN_DOWN  5
#define BTN_LEFT  4
#define BTN_RIGHT 45
#define BTN_OK    37
#define BTN_BACK  36
#endif

#ifndef C_BLACK
#define C_BLACK       0x0000
#define C_WHITE       0xFFFF
#define C_MIDNIGHTBL  0x18CE
#define C_PERSIANPLUM 0x70E3
#define C_DARKORANGE  0xFC60
#define C_YELLOW      0xFFE0
#define C_BLUE        0x001F
#define C_ORANGERED   0xFC00
#define C_GRAY33      0x18C3
#define C_GRAY44      0x2108
#define C_BORDERGRAY  0x5AEB
#define C_GREEN       0x07E0
#define C_CYAN        0x07FF
#define C_RED         0xF800
#define C_DIMGRAY     0x4228
#define C_BODYBG      0x230C
#define C_NEON_PINK   0xF81F
#define C_CORNER_CYAN 0x0BFF
#define C_LED_GREEN   0x06E0
#define C_GLOW_ORANGE 0xFD20
#endif

#ifndef HEADER_H
#define HEADER_H 44
#define FOOTER_H 22
#define SB_H HEADER_H
#endif

#ifndef W
#define W 240
#define H 240
#endif

// ===================== GLOBAL DECLARATIONS =====================
extern TFT_eSPI tft;
extern TFT_eSprite cv;
extern void dispPush();
extern void sndNav();
extern void sndOk();
extern void sndBack();
extern void sndBoot();

// ===================== CONTROLLER =====================
class Controller {
    static const int BTN_COUNT = 6;
    const uint8_t pins[BTN_COUNT] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_OK, BTN_BACK};
    struct Btn {
        uint32_t lastChange;
        bool pressed, justPressed, justReleased;
    } btns[BTN_COUNT];
    uint32_t lastDebounce[BTN_COUNT];
    static const uint32_t DEBOUNCE = 20, REPEAT_DELAY = 250, REPEAT_RATE = 80;
    uint32_t nextRepeat[BTN_COUNT];
    bool repeatActive[BTN_COUNT];
public:
    enum Button { UP = 0, DOWN, LEFT, RIGHT, OK, BACK };
    void begin();
    void update();
    bool pressed(Button b) { return btns[(int)b].justPressed; }
};
extern Controller ctl;

// ===================== ICON SYSTEM =====================
struct AppEntry { const char* name, *desc; int iconIdx; };
extern const int APP_COUNT;
extern AppEntry apps[];
extern TFT_eSprite* iconCache[];
extern void initIcons();

// ===================== MENU =====================
#define MENU_MAX_ITEMS 20
struct MenuItem {
    const char* title;
    uint16_t color;
    const char* postfix;
    int iconIdx;
    void (*callback)(int);
    int callbackData;
};
class Menu {
    const char* _title;
    MenuItem _items[MENU_MAX_ITEMS];
    int _count, _cursor, _scroll;
    bool _finished;
    int _activationButton;
    uint16_t _titleColor;
    uint32_t _lastMove;
    static const int ICON_W = 32, ITEM_H = 28, TITLE_H = 40, ITEMS_Y = 52, VISIBLE = 6, SCROLL_W = 8;
public:
    Menu();
    void setTitle(const char* t) { _title = t; }
    void setTitleColor(uint16_t c) { _titleColor = c; }
    void clear();
    int getCursor() { return _cursor; }
    int getCount() { return _count; }
    bool isFinished();
    void reset();
    void addItem(const char* title, uint16_t color = C_WHITE, const char* postfix = "",
                 int iconIdx = -1, void (*cb)(int) = nullptr, int cd = 0);
    void update();
    void draw(TFT_eSprite* gfx);
};

// ===================== STATUS BAR =====================
extern char timeStr[6];
void drawStatusBar(TFT_eSprite* gfx);

// ===================== ALERT =====================
class Alert {
    const char *_title, *_body;
    bool _active;
public:
    Alert() : _title(""), _body(""), _active(false) {}
    void show(const char* title, const char* body) { _title = title; _body = body; _active = true; }
    bool isActive() { return _active; }
    void dismiss() { _active = false; }
    void update();
    void draw(TFT_eSprite* gfx);
};
extern Alert alert;

// ===================== APP BASE =====================
class App {
public:
    virtual ~App() {}
    virtual const char* name() const = 0;
    virtual void create() {}
    virtual void update() {}
    virtual void draw(TFT_eSprite* gfx) {}
    virtual void destroy() {}
    virtual bool onBack() { return true; }
};

// ===================== SERVICE =====================
class Service {
public:
    virtual ~Service() {}
    virtual void setup() {}
    virtual void update() {}
};

// ===================== SERVICE MANAGER =====================
class ServiceManager {
    static const int MAX = 8;
    Service* services[MAX];
    int count;
public:
    ServiceManager() : count(0) {}
    void add(Service* s) { if (count < MAX) services[count++] = s; }
    void setupAll() { for (int i = 0; i < count; i++) services[i]->setup(); }
    void updateAll() { for (int i = 0; i < count; i++) services[i]->update(); }
};

// ===================== APP MANAGER =====================
class AppManager {
    App *_launcher, *_current, *_pending;
    bool _redrawNeeded;
public:
    AppManager(App* launcherApp);
    void switchTo(App* app) { _pending = app; }
    App* current() { return _current; }
    void update();
    void back();
    void draw(TFT_eSprite* gfx);
    bool needsRedraw() { bool r = _redrawNeeded; _redrawNeeded = false; return r; }
    void requestRedraw() { _redrawNeeded = true; }
};
extern AppManager appMgr;

// ===================== CONCRETE APPS =====================
class LauncherApp : public App {
    int _selected;
public:
    const char* name() const { return "Launcher"; }
    void create();
    void update();
    void draw(TFT_eSprite* gfx);
    bool onBack() { return false; }
};
extern LauncherApp launcherApp;

class GPIOApp : public App {
    int _scroll, _cursor;
    static const int PINS = 8;
    uint8_t _pins[PINS];
    const char* _labels[PINS];
public:
    GPIOApp();
    const char* name() const { return "GPIO Debugger"; }
    void create();
    void update();
    void draw(TFT_eSprite* gfx);
    bool onBack() { return true; }
};

class SysInfoApp : public App {
public:
    const char* name() const { return "System Info"; }
    void create();
    void update();
    void draw(TFT_eSprite* gfx);
    bool onBack() { return true; }
};

class AboutApp : public App {
public:
    const char* name() const { return "About"; }
    void create();
    void update();
    void draw(TFT_eSprite* gfx);
    bool onBack() { return true; }
};

class PlaceholderApp : public App {
    const char* _name;
public:
    PlaceholderApp(const char* n) : _name(n) {}
    const char* name() const { return _name; }
    void draw(TFT_eSprite* gfx);
    bool onBack() { return true; }
};

#endif // KEIRA_H
