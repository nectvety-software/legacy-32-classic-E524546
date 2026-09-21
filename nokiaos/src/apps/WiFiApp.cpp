#include "WiFiApp.h"
#include "../gui/UIRenderer.h"
#include "../gui/Theme.h"

extern TFT_eSPI tft;

void WiFiApp::begin() {
    state = STATE_LIST;
    networkCount = 0;
    selectedNetwork = -1;
    scanNetworks();
}

void WiFiApp::update() {
    InputManager& input = InputManager::getInstance();
    input.update();
    
    if (state == STATE_SCANNING) {
        return;
    }
    
    if (state == STATE_CONNECTING && connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            state = STATE_CONNECTED;
            connecting = false;
            draw();
        } else if (millis() - connectStartTime > 15000) {
            state = STATE_LIST;
            connecting = false;
            draw();
        }
        return;
    }
    
    if (input.isUp() && selectedNetwork > 0) {
        selectedNetwork--;
        if (selectedNetwork < scrollOffset) scrollOffset = selectedNetwork;
        draw();
    }
    else if (input.isDown() && selectedNetwork < networkCount - 1) {
        selectedNetwork++;
        if (selectedNetwork >= scrollOffset + getVisibleCount()) {
            scrollOffset = selectedNetwork - getVisibleCount() + 1;
        }
        draw();
    }
    else if (input.isStart()) {
        if (selectedNetwork >= 0 && selectedNetwork < networkCount) {
            state = STATE_CONNECTING;
            connecting = true;
            connectStartTime = millis();
            WiFi.begin(networks[selectedNetwork].c_str());
            draw();
        }
    }
    else if (input.isOption()) {
        scanNetworks();
    }
}

bool WiFiApp::onKey(uint8_t key) {
    if (key == KEY_MENU) {
        AppManager::getInstance().goBack();
        return true;
    }
    return false;
}

void WiFiApp::scanNetworks() {
    state = STATE_SCANNING;
    networkCount = 0;
    draw();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    networkCount = WiFi.scanNetworks();
    if (networkCount > MAX_NETWORKS) networkCount = MAX_NETWORKS;
    
    for (int i = 0; i < networkCount; i++) {
        networks[i] = WiFi.SSID(i);
    }
    
    selectedNetwork = networkCount > 0 ? 0 : -1;
    scrollOffset = 0;
    state = STATE_LIST;
    draw();
}

void WiFiApp::draw() {
    UIRenderer& ui = UIRenderer::getInstance();
    ui.clearScreen(COLOR_BG_DARK);
    
    if (state == STATE_SCANNING) {
        ui.drawTitleBar("WiFi - Scanning...");
        ui.drawTextCentered("Scanning networks...", SCREEN_HEIGHT / 2, COLOR_TEXT_DIM);
        return;
    }
    
    if (state == STATE_CONNECTING) {
        ui.drawTitleBar("WiFi - Connecting");
        ui.drawTextCentered("Connecting...", SCREEN_HEIGHT / 2 - 20, COLOR_TEXT);
        ui.drawProgressBar(20, SCREEN_HEIGHT / 2, SCREEN_WIDTH - 40, 20, 0.5);
        return;
    }
    
    if (state == STATE_CONNECTED) {
        ui.drawTitleBar("WiFi - Connected");
        ui.drawText("SSID:", 10, 50, COLOR_TEXT_DIM);
        ui.drawText(networks[selectedNetwork].c_str(), 70, 50, COLOR_TEXT);
        ui.drawText("IP:", 10, 70, COLOR_TEXT_DIM);
        ui.drawText(WiFi.localIP().toString().c_str(), 50, 70, COLOR_PRIMARY);
        ui.drawButton(80, 250, 80, 30, "Disconnect", false);
        return;
    }
    
    ui.drawTitleBar("WiFi");
    
    uint8_t visibleCount = getVisibleCount();
    for (uint8_t i = 0; i < visibleCount && (scrollOffset + i) < networkCount; i++) {
        uint8_t idx = scrollOffset + i;
        bool isSelected = (idx == selectedNetwork);
        
        int y = TITLE_HEIGHT + i * 35;
        if (isSelected) {
            tft.fillRect(0, y, SCREEN_WIDTH, 35, COLOR_SELECTED);
        }
        
        tft.setTextFont(FONT_MENU);
        tft.setTextColor(isSelected ? COLOR_BG_DARK : COLOR_TEXT);
        tft.setCursor(10, y + 5);
        tft.print(networks[idx]);
        
        int32_t rssi = WiFi.RSSI(idx);
        uint16_t color = rssi > -50 ? COLOR_SUCCESS : (rssi > -70 ? COLOR_PRIMARY : COLOR_WARNING);
        tft.setTextColor(color);
        tft.setCursor(10, y + 20);
        tft.printf("%d dBm", rssi);
    }
    
    ui.drawStatusBar();
    char status[32];
    snprintf(status, sizeof(status), "Networks: %d", networkCount);
    ui.drawText(status, 5, SCREEN_HEIGHT - STATUS_HEIGHT + 3, COLOR_TEXT_DIM);
}

uint8_t WiFiApp::getVisibleCount() {
    return (SCREEN_HEIGHT - TITLE_HEIGHT - STATUS_HEIGHT) / 35;
}
