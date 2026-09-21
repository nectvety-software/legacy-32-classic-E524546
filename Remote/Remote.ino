#include <BleKeyboard.h>
#include <BleMouse.h>
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// ==================== CẤU HÌNH PSRAM ====================
#include "esp_heap_caps.h"

// ==================== ĐỊNH NGHĨA CHÂN ====================
#define KEY_UP      7
#define KEY_DOWN    46
#define KEY_LEFT    45
#define KEY_RIGHT   6
#define KEY_MENU    18
#define KEY_OPTION  8
#define KEY_SELECT  16
#define KEY_START   17
#define KEY_A       15
#define KEY_B       5

// Restore TFT Pins (for ESP32-S3)
#define TFT_LED_K   39
#define TFT_DC      47
#define TFT_CS      14
#define TFT_SCL     48
#define TFT_SDA     12
#define TFT_RST     3

// ==================== MÀU SẮC THEME ====================
#define COLOR_BG            0x0000  // Đen
#define COLOR_PANEL         0x0841  // Xám đen panel
#define COLOR_ACCENT        0x07FF  // Cyan
#define COLOR_SECONDARY     0xF81F  // Magenta (Hồng)
#define COLOR_DANGER        0xF800  // Đỏ
#define COLOR_SUCCESS       0x07E0  // Xanh lá
#define COLOR_YELLOW        0xFFE0  // Vàng
#define COLOR_TEXT          0xFFFF  // Trắng
#define COLOR_TEXT_DIM      0x7BEF  // Xám mờ
#define COLOR_BG_LIGHT      0x1082  // Xám xanh cho grid

// ==================== BIẾN TRẠNG THÁI & ENUM ====================
enum AppMode { REMOTE_MODE, MOUSE_MODE, KEYBOARD_MODE, SETTINGS_MODE };

// ==================== KHỞI TẠO ====================
BleKeyboard bleKeyboard("ESP32-S3 Box", "Espressif", 100);
BleMouse bleMouse;
TFT_eSPI tft = TFT_eSPI();

// ==================== CẤU TRÚC NÚT VỚI ANIMATION ====================
struct ButtonState {
    uint8_t pin;
    bool lastState;
    bool currentState;
    bool isPressed;
    bool wasPressed;
    const char* command;
    const char* label;
    uint16_t color;
    uint16_t glowColor;
    float scale;           // Scale hiệu ứng (1.0 = normal)
    float targetScale;     // Scale mục tiêu
    float glowIntensity;   // Độ sáng glow (0-1)
    float targetGlow;
    int x, y, w, h;        // Vị trí trên màn hình
    uint8_t shape;         // 0=round, 1=circle, 2=d-pad
    unsigned long lastDebounceTime;
    unsigned long pressTime;
};

ButtonState buttons[] = {
    // D-Pad (Cyan)
    {KEY_UP,    HIGH, HIGH, false, false, "UP",    "▲", COLOR_ACCENT,    0x03FF, 1.0, 1.0, 0, 0, 120, 80,  55, 55, 2, 0, 0},
    {KEY_DOWN,  HIGH, HIGH, false, false, "DOWN",  "▼", COLOR_ACCENT,    0x03FF, 1.0, 1.0, 0, 0, 120, 180, 55, 55, 2, 0, 0},
    {KEY_LEFT,  HIGH, HIGH, false, false, "LEFT",  "◀", COLOR_ACCENT,    0x03FF, 1.0, 1.0, 0, 0, 65,  130, 55, 55, 2, 0, 0},
    {KEY_RIGHT, HIGH, HIGH, false, false, "RIGHT", "▶", COLOR_ACCENT,    0x03FF, 1.0, 1.0, 0, 0, 175, 130, 55, 55, 2, 0, 0},
    
    // Action buttons
    {KEY_A,     HIGH, HIGH, false, false, "A",     "A", COLOR_DANGER,    0xF800, 1.0, 1.0, 0, 0, 175, 240, 70, 70, 1, 0, 0}, // Đỏ lớn
    {KEY_B,     HIGH, HIGH, false, false, "B",     "B", COLOR_SECONDARY, 0xF81F, 1.0, 1.0, 0, 0, 65,  240, 50, 50, 1, 0, 0}, // Hồng nhỏ
    
    // Bottom toolbar
    {KEY_MENU,  HIGH, HIGH, false, false, "MENU",  "☰", COLOR_YELLOW,    0xFD20, 1.0, 1.0, 0, 0, 45,  300, 65, 30, 0, 0, 0},
    {KEY_START, HIGH, HIGH, false, false, "OK",    "OK", COLOR_SUCCESS,   0x07E0, 1.0, 1.0, 0, 0, 120, 300, 65, 30, 0, 0, 0},
    {KEY_SELECT, HIGH, HIGH, false, false, "MODE",  "MODE", COLOR_SECONDARY, 0xF81F, 1.0, 1.0, 0, 0, 195, 300, 65, 30, 0, 0, 0},
    
    // Settings icon top-right
    {KEY_OPTION,HIGH, HIGH, false, false, "OPT",   "⚙", COLOR_ACCENT,    0x07FF, 1.0, 1.0, 0, 0, 160, 20,  25, 25, 1, 0, 0}
};

const int NUM_BUTTONS = sizeof(buttons) / sizeof(buttons[0]);
const unsigned long DEBOUNCE_DELAY = 30; // Giảm xuống 30ms cho responsive hơn

// ==================== BIẾN TRẠNG THÁI ====================
AppMode currentMode = REMOTE_MODE;

bool isConnected = false;
String lastCommand = "";
int commandCount = 123;
unsigned long lastActivity = 0;
uint8_t brightnessLevel = 180;
float mouseX = 120, mouseY = 160;
float mouseSpeed = 150.0f;
int gridX = 0, gridY = 0; // Keyboard selection

// Cài đặt menu
struct SettingItem {
    const char* label;
    const char* value;
    uint16_t color;
};
SettingItem settings[] = {
    {"Brightness", "80%", COLOR_ACCENT},
    {"Sound", "ON", COLOR_SUCCESS},
    {"Vibration", "OFF", COLOR_TEXT_DIM},
    {"Sleep Timer", "30s", COLOR_YELLOW},
    {"BT Power", "High", COLOR_ACCENT},
    {"About", "v1.0", COLOR_TEXT_DIM}
};
int selectedSetting = 0;

// Animation variables
float globalPulse = 0;  // Hiệu ứng pulse toàn cục
unsigned long lastFrame = 0;
const int FPS = 60;
const int FRAME_TIME = 1000 / FPS;

// Particle system cho hiệu ứng
struct Particle {
    float x, y;
    float vx, vy;
    float life;
    uint16_t color;
    bool active;
};
Particle particles[20];

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    initPSRAM();
    initPins();
    initTFT();
    initBluetooth();
    initParticles();
    
    showBootAnimation();
    
    lastActivity = millis();
    lastFrame = millis();
}

void initPSRAM() {
    if (psramFound()) {
        Serial.printf("PSRAM: %dMB\n", ESP.getPsramSize() / 1024 / 1024);
    }
}

void initPins() {
    pinMode(TFT_LED_K, OUTPUT);
    analogWrite(TFT_LED_K, brightnessLevel);
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttons[i].pin, INPUT_PULLUP);
    }
}

void initTFT() {
    SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS);
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COLOR_BG);
    tft.setTextFont(4);
    analogWrite(TFT_LED_K, brightnessLevel);
}

void initBluetooth() {
    bleKeyboard.begin();
    bleMouse.begin();
    Serial.println("BLE Keyboard/Mouse: ESP32-S3 Box");
}

void initParticles() {
    for (int i = 0; i < 20; i++) {
        particles[i].active = false;
    }
}

// ==================== BOOT ANIMATION ====================
void showBootAnimation() {
    // Hiệu ứng khởi động cyberpunk style
    tft.fillScreen(COLOR_BG);
    
    // Vẽ grid
    for (int i = 0; i < 240; i += 20) {
        tft.drawLine(i, 0, i, 320, COLOR_BG_LIGHT);
    }
    for (int i = 0; i < 320; i += 20) {
        tft.drawLine(0, i, 240, i, COLOR_BG_LIGHT);
    }
    
    // Animation text
    const char* texts[] = {"INITIALIZING", "LOADING PSRAM", "CONNECTING BT", "READY"};
    uint16_t colors[] = {COLOR_ACCENT, COLOR_SECONDARY, COLOR_YELLOW, COLOR_SUCCESS};
    
    for (int step = 0; step < 4; step++) {
        tft.fillRect(20, 140, 200, 40, COLOR_BG);
        tft.setTextColor(colors[step], COLOR_BG);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(texts[step], 120, 160, 4);
        
        // Premium progress glow
        for(int x=40; x<200; x++) {
            uint16_t c = tft.alphaBlend((x-40), colors[step], COLOR_BG);
            tft.drawFastVLine(x, 200, 4, c);
        }
        delay(300);
    }
    
    delay(200);
    tft.fillScreen(COLOR_BG);
}

// ==================== MAIN LOOP ====================
void loop() {
    unsigned long currentTime = millis();
    
    // Giới hạn FPS
    if (currentTime - lastFrame < FRAME_TIME) {
        delay(1);
        return;
    }
    float deltaTime = (currentTime - lastFrame) / 1000.0f;
    lastFrame = currentTime;
    
    // Cập nhật animation toàn cục
    globalPulse += deltaTime * 2;
    
    // Kiểm tra Bluetooth
    bool newBtState = bleKeyboard.isConnected();
    if (newBtState != isConnected) {
        isConnected = newBtState;
        createParticles(120, 20, isConnected ? COLOR_SUCCESS : COLOR_DANGER, 10);
    }
    
    // Xử lý nút bấm
    for (int i = 0; i < NUM_BUTTONS; i++) {
        processButton(buttons[i], i, deltaTime);
    }
    
    // Cập nhật particles
    updateParticles(deltaTime);
    
    // Nhận Bluetooth (không dùng cho HID)
    
    // Render frame
    renderFrame(deltaTime);
    
    // Auto sleep
    handleAutoSleep();
}

// ==================== XỬ LÝ NÚT VỚI ANIMATION ====================
void processButton(ButtonState &btn, int index, float deltaTime) {
    bool reading = digitalRead(btn.pin);
    
    if (reading != btn.lastState) {
        btn.lastDebounceTime = millis();
    }
    
    if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != btn.currentState) {
            btn.currentState = reading;
        }
    }
    btn.lastState = reading;
    
    bool isPressed = (btn.currentState == LOW);
    
    if (isPressed && !btn.wasPressed) {
        btn.isPressed = true;
        btn.pressTime = millis();
        btn.targetScale = 0.85f;  // Elastic Scale
        btn.targetGlow = 1.0f;
        
        createParticles(btn.x + btn.w/2, btn.y + btn.h/2, btn.glowColor, 5);
        lastActivity = millis();
        commandCount++;
        
        // Mode Management
        if (index == 9) { // OPT button
            currentMode = (AppMode)((currentMode + 1) % 4);
            tft.fillScreen(COLOR_BG);
            return;
        }

        // HID Logic
        if (currentMode == REMOTE_MODE) {
            if (index == 0) bleKeyboard.write(KEY_UP_ARROW);
            if (index == 1) bleKeyboard.write(KEY_DOWN_ARROW);
            if (index == 2) bleKeyboard.write(KEY_LEFT_ARROW);
            if (index == 3) bleKeyboard.write(KEY_RIGHT_ARROW);
            if (index == 4) bleKeyboard.write(KEY_RETURN);
            if (index == 5) bleKeyboard.write(KEY_BACKSPACE);
            if (index == 7) bleKeyboard.write(KEY_RETURN);
            if (index == 8) bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
        } 
        else if (currentMode == MOUSE_MODE) {
            if (index == 4) bleMouse.click(MOUSE_LEFT);
            if (index == 5) bleMouse.click(MOUSE_RIGHT);
            if (index == 7) bleMouse.click(MOUSE_MIDDLE);
        }
        else if (currentMode == KEYBOARD_MODE) {
            if (index == 0) gridY = (gridY > 0) ? gridY - 1 : 4;
            if (index == 1) gridY = (gridY < 4) ? gridY + 1 : 0;
            if (index == 2) gridX = (gridX > 0) ? gridX - 1 : 5;
            if (index == 3) gridX = (gridX < 5) ? gridX + 1 : 0;
            
            if (index == 7) { // OK -> Type key
                const char* keys[] = {"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z"," ","\b","\n","?"};
                bleKeyboard.print(keys[gridY * 6 + gridX]);
            }
            if (index == 5) { currentMode = REMOTE_MODE; tft.fillScreen(COLOR_BG); }
        }
        else if (currentMode == SETTINGS_MODE) {
            if (index == 0) selectedSetting = (selectedSetting > 0) ? selectedSetting - 1 : 5;
            if (index == 1) selectedSetting = (selectedSetting < 5) ? selectedSetting + 1 : 0;
            if (index == 5) { currentMode = REMOTE_MODE; tft.fillScreen(COLOR_BG); }
        }
        
        flashScreenEdge(btn.color);
    }
    else if (!isPressed && btn.wasPressed) {
        btn.isPressed = false;
        btn.targetScale = 1.0f;
        btn.targetGlow = 0.0f;
    }
    
    // Continuous Mouse Movement
    if (currentMode == MOUSE_MODE && isPressed) {
        float moveStep = mouseSpeed * deltaTime;
        if (index == 0) { mouseY -= moveStep; bleMouse.move(0, -2); }
        if (index == 1) { mouseY += moveStep; bleMouse.move(0, 2); }
        if (index == 2) { mouseX -= moveStep; bleMouse.move(-2, 0); }
        if (index == 3) { mouseX += moveStep; bleMouse.move(2, 0); }
        
        // Constrain cursor
        mouseX = constrain(mouseX, 10, 230);
        mouseY = constrain(mouseY, 50, 310);
    }
    
    // Animation lerp
    btn.scale += (btn.targetScale - btn.scale) * 15.0f * deltaTime;
    btn.glowIntensity += (btn.targetGlow - btn.glowIntensity) * 10.0f * deltaTime;
    btn.wasPressed = isPressed;
}

// ==================== PARTICLE SYSTEM ====================
void createParticles(float x, float y, uint16_t color, int count) {
    for (int i = 0; i < count; i++) {
        for (int p = 0; p < 20; p++) {
            if (!particles[p].active) {
                particles[p].x = x;
                particles[p].y = y;
                float angle = random(0, 360) * PI / 180;
                float speed = random(20, 80) / 10.0f;
                particles[p].vx = cos(angle) * speed;
                particles[p].vy = sin(angle) * speed;
                particles[p].life = 1.0f;
                particles[p].color = color;
                particles[p].active = true;
                break;
            }
        }
    }
}

void updateParticles(float deltaTime) {
    for (int i = 0; i < 20; i++) {
        if (particles[i].active) {
            particles[i].x += particles[i].vx * deltaTime * 60;
            particles[i].y += particles[i].vy * deltaTime * 60;
            particles[i].vy += 0.5f; // Gravity
            particles[i].life -= deltaTime * 2;
            
            if (particles[i].life <= 0) {
                particles[i].active = false;
            }
        }
    }
}

// ==================== RENDER FRAME ====================
void renderFrame(float deltaTime) {
    if (currentMode == REMOTE_MODE) {
        renderRemoteScreen(deltaTime);
    } else if (currentMode == MOUSE_MODE) {
        renderMouseScreen(deltaTime);
    } else if (currentMode == KEYBOARD_MODE) {
        renderKeyboardScreen(deltaTime);
    } else if (currentMode == SETTINGS_MODE) {
        renderSettingsScreen(deltaTime);
    }
}

void renderRemoteScreen(float deltaTime) {
    static bool firstDraw = true;
    if (firstDraw) {
        drawBackground();
        firstDraw = false;
    }
    
    drawHeader();
    drawStatusBar();
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
        drawAnimatedButton(buttons[i]);
    }
    
    drawParticles();
}

void renderMouseScreen(float deltaTime) {
    drawBackground();
    drawHeader();
    drawStatusBar();
    
    // Mouse specific UI
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("AIR MOUSE", 120, 60, 4);
    
    // Draw Cursor
    tft.fillCircle(mouseX, mouseY, 4, COLOR_ACCENT);
    tft.drawCircle(mouseX, mouseY, 8, COLOR_ACCENT);
    
    // Draw Buttons (smaller for mouse mode)
    for (int i = 0; i < NUM_BUTTONS; i++) {
        drawAnimatedButton(buttons[i]);
    }
}

void renderKeyboardScreen(float deltaTime) {
    drawBackground();
    drawHeader();
    
    tft.setTextColor(COLOR_YELLOW, COLOR_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("VIRTUAL KEYBOARD", 120, 50, 2);
    
    const char* keys[] = {
        "A","B","C","D","E","F",
        "G","H","I","J","K","L",
        "M","N","O","P","Q","R",
        "S","T","U","V","W","X",
        "Y","Z","SP","BS","ENT","?"
    };
    
    for (int i = 0; i < 30; i++) {
        int r = i / 6;
        int c = i % 6;
        int kx = 30 + c * 36;
        int ky = 90 + r * 36;
        
        bool selected = (r == gridY && c == gridX);
        tft.fillRoundRect(kx-16, ky-16, 32, 32, 4, selected ? COLOR_ACCENT : COLOR_PANEL);
        tft.setTextColor(selected ? COLOR_BG : COLOR_TEXT, COLOR_BG);
        tft.drawString(keys[i], kx, ky, 2);
    }
    
    drawStatusBar();
}

void renderSettingsScreen(float deltaTime) {
    tft.fillScreen(COLOR_BG);
    
    // Title
    tft.setTextColor(COLOR_YELLOW, COLOR_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("⚙ SETTINGS", 120, 40, 4);
    tft.drawFastHLine(20, 65, 200, COLOR_PANEL);
    
    // Menu Items
    for (int i = 0; i < 6; i++) {
        int y = 90 + i * 35;
        bool selected = (i == selectedSetting);
        
        if (selected) {
            tft.fillRoundRect(15, y - 5, 210, 30, 4, COLOR_PANEL);
            tft.drawRoundRect(15, y - 5, 210, 30, 4, COLOR_ACCENT);
        }
        
        tft.setTextColor(selected ? COLOR_TEXT : COLOR_TEXT_DIM, COLOR_BG);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(settings[i].label, 25, y, 2);
        
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(settings[i].color, COLOR_BG);
        tft.drawString(settings[i].value, 215, y, 2);
    }
    
    // Footer
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("▲▼ Navigate | OK Select | B Back", 120, 310, 1);
}

void drawBackground() {
    tft.fillScreen(COLOR_BG);
    
    // Ambient panels
    tft.fillRoundRect(10, 50, 220, 240, 15, COLOR_PANEL);
    
    // Subtle grid in panel
    for (int x = 20; x < 230; x += 30) {
        tft.drawFastVLine(x, 60, 220, 0x0841); // Very dark grey
    }
    for (int y = 60; y < 280; y += 30) {
        tft.drawFastHLine(20, y, 200, 0x0841);
    }
}

void drawAnimatedButton(ButtonState &btn) {
    int cx = btn.x;
    int cy = btn.y;
    int w = btn.w * btn.scale;
    int h = btn.h * btn.scale;
    int x = cx - w / 2;
    int y = cy - h / 2;
    
    // Neon Glow (Layered Alpha Blending)
    if (btn.glowIntensity > 0.05f) {
        for (int i = 3; i > 0; i--) {
            int gw = w + i * 12 * btn.glowIntensity;
            int gh = h + i * 12 * btn.glowIntensity;
            uint16_t glowCol = tft.alphaBlend(40 / i * btn.glowIntensity, btn.glowColor, COLOR_BG);
            if (btn.shape == 1) tft.fillCircle(cx, cy, (gw + gh) / 4, glowCol);
            else tft.fillRoundRect(cx - gw / 2, cy - gh / 2, gw, gh, 8, glowCol);
        }
    }
    
    uint16_t baseColor = btn.isPressed ? btn.color : COLOR_PANEL;
    
    // 3D Bevel Body
    if (btn.shape == 1) {
        tft.fillCircle(cx, cy, w / 2, baseColor);
        tft.drawCircle(cx, cy, w / 2, btn.color);
        if (!btn.isPressed) tft.drawCircle(cx - 2, cy - 2, w / 2, 0xFFFF); // Highlight
    } else {
        tft.fillRoundRect(x, y, w, h, 8, baseColor);
        tft.drawRoundRect(x, y, w, h, 8, btn.color);
        if (!btn.isPressed) tft.drawFastHLine(x + 2, y + 2, w - 4, 0xFFFF); // Top highlight
    }
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString(btn.label, cx, cy, 4);
}

void drawParticles() {
    for (int i = 0; i < 20; i++) {
        if (particles[i].active) {
            int x = particles[i].x;
            int y = particles[i].y;
            uint8_t alpha = particles[i].life * 255;
            uint16_t col = tft.alphaBlend(alpha, particles[i].color, COLOR_BG);
            
            if (x >= 0 && x < 240 && y >= 0 && y < 320) {
                tft.fillCircle(x, y, particles[i].life * 4, col);
            }
        }
    }
}

void drawHeader() {
    float pulse = (sin(globalPulse) * 0.5 + 0.5);
    
    // Title with Icon
    uint16_t btColor = bleKeyboard.isConnected() ? COLOR_ACCENT : COLOR_TEXT_DIM;
    tft.fillCircle(20, 20, 8, bleKeyboard.isConnected() ? COLOR_ACCENT : COLOR_PANEL);
    if (!bleKeyboard.isConnected()) tft.drawCircle(20, 20, 8, COLOR_TEXT_DIM);
    
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("ESP32-S3 Box", 120, 15, 2);
    
    // Settings icon (button defined in array handles the interactive part)
    // Here we just draw a line divider
    tft.drawFastHLine(10, 40, 220, COLOR_PANEL);
    
    // Battery indicator
    int batX = 205, batY = 15;
    tft.drawRect(batX, batY, 25, 12, COLOR_TEXT);
    tft.drawRect(batX+25, batY+3, 2, 6, COLOR_TEXT); // tip
    tft.fillRect(batX+2, batY+2, map(brightnessLevel, 0, 255, 2, 21), 8, COLOR_SUCCESS);
}

void drawStatusBar() {
    // Mode Text
    const char* modeStrs[] = {"REMOTE", "MOUSE", "KEYBOARD", "SETTINGS"};
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(BL_DATUM);
    tft.drawString(modeStrs[currentMode], 10, 315, 2);
    
    // ID Counter
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(BR_DATUM);
    tft.drawString("#" + String(commandCount), 230, 315, 2);
}

void flashScreenEdge(uint16_t color) {
    // Hiệu ứng flash viền màn hình khi nhấn nút
    tft.drawRect(0, 0, 240, 320, color);
    tft.drawRect(1, 1, 238, 318, color);
    delay(30);
    tft.drawRect(0, 0, 240, 320, COLOR_BG);
    tft.drawRect(1, 1, 238, 318, COLOR_BG);
}

// ==================== BLUETOOTH ====================
void sendCommand(const char* cmd) {
    Serial.println("Action: " + String(cmd));
    lastCommand = cmd;
    commandCount++;
}

void handleBTResponse(String &response) {
    response.trim();
    Serial.println("RX: " + response);
}

// ==================== TIẾT KIỆM NĂNG LƯỢNG ====================
void handleAutoSleep() {
    if (millis() - lastActivity > 30000) {
        if (brightnessLevel > 20) {
            brightnessLevel -= 2;
            analogWrite(TFT_LED_K, brightnessLevel);
        }
    } else {
        if (brightnessLevel < 180) {
            brightnessLevel += 5;
            analogWrite(TFT_LED_K, brightnessLevel);
        }
    }
}