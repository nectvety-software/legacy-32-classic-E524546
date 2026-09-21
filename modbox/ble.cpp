#include "ble.h"
#include "modbox_main.h"

//////////////// BLE Global Variables ////////////////
BLEMode bleMode = BLE_IDLE;
ControlMode controlMode = MODE_REMOTE;
BLEMenuState bleMenuState = BLE_MAIN;
int bleCursor = 0;
bool bleConnected = false;
bool bleAdvertising = false;
int mouseSpeed = 10;
bool clickSound = true;
bool backlightState = true;
bool airMouseMode = false;
bool remoteModeActive = false;
bool continuousMove = true;
int primaryBtn = 0;

String deviceName = "ESP32-Box";
String scanResult = "";
String clientData = "";
String serverLog = "";

static BLEServer* pServer = nullptr;
static BLEService* pService = nullptr;
static BLECharacteristic* pUartTx = nullptr;
static BLECharacteristic* pUartRx = nullptr;
static BLECharacteristic* pHIDReport = nullptr;
static BLECharacteristic* pVMouseData = nullptr;
static BLEAdvertising* pAdvertising = nullptr;

static BLEClient* pClient = nullptr;
static BLERemoteService* pRemoteService = nullptr;
static BLERemoteCharacteristic* pRemoteUartTx = nullptr;
static BLERemoteCharacteristic* pRemoteUartRx = nullptr;
bool clientConnected = false;

static String keyboardInput = "";
static bool keyboardUppercase = false;

static int currentHover = -1;
static unsigned long lastMoveTime = 0;
static bool rightBtnDown = false;
static bool okBtnDown = false;
static int tiktokMode = 0;
static unsigned long lastTikTokTime = 0;

static int bleScreenState = 0;
static int bleSettingIndex = 0;
static bool editingName = false;

static uint8_t mouseReport[4] = {0, 0, 0, 0};

////////////// BLE Notification System ////////////////
static String bleNotification = "";
static unsigned long notifStartTime = 0;
static const unsigned long NOTIF_DURATION = 3000;
static bool notifActive = false;

void bleShowNotification(String msg) {
    bleNotification = msg;
    notifStartTime = millis();
    notifActive = true;
    
    if (gfx != nullptr) {
        gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_GREEN);
        gfx->setTextColor(COLOR_BG);
        gfx->setCursor(10, 8);
        gfx->print(msg.c_str());
        delay(500);
    }
}

void bleUpdateNotification() {
    if (notifActive && gfx != nullptr) {
        if (millis() - notifStartTime > NOTIF_DURATION) {
            notifActive = false;
            drawStatusBar();
        }
    }
}

//////////////// Scan Results Storage ////////////////
int foundDevices = 0;
String devNames[MAX_FOUND_DEVICES];
int devRSSI[MAX_FOUND_DEVICES];
String devAddrs[MAX_FOUND_DEVICES];
int selectedDevice = 0;
bool scanning = false;

//////////////// BLE HID Report Map ////////////////
static uint8_t hidReportMap[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
    0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
    0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
    0xC0, 0xC0
};

//////////////// BLE Server Callbacks ////////////////
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleConnected = true;
        serverLog = "Client connected!";
        bleShowNotification("* DANG KET NOI *");
        if (clickSound) {
            tone(BUZZER, 2000, 80);
            delay(100);
            tone(BUZZER, 2500, 120);
        }
        if (bleScreenState == 12) bleDrawServer();
        else if (bleScreenState == 0) bleDrawMain();
    }
    void onDisconnect(BLEServer* pServer) {
        bleConnected = false;
        airMouseMode = false;
        remoteModeActive = false;
        serverLog = "Client disconnected";
        bleShowNotification("* MAT KET NOI *");
        if (clickSound) {
            tone(BUZZER, 800, 150);
            delay(200);
            tone(BUZZER, 600, 150);
        }
        if (pAdvertising != nullptr) {
            pAdvertising->start();
        }
        if (bleScreenState == 12) bleDrawServer();
        else if (bleScreenState == 0) bleDrawMain();
    }
};

//////////////// BLE UART Callback ////////////////
class UartRxCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        String rxData = pCharacteristic->getValue().c_str();
        serverLog = "RX: " + rxData;
        bleProcessCommand(rxData);
        bleDrawServer();
    }
};

//////////////// BLE Client Callbacks ////////////////
class ClientCallbacks: public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        clientConnected = true;
        clientData = "Connected!";
        bleShowNotification("* KET NOI SERVER *");
        if (clickSound) {
            tone(BUZZER, 2000, 80);
            delay(100);
            tone(BUZZER, 2500, 120);
        }
    }
    void onDisconnect(BLEClient* pclient) {
        clientConnected = false;
        clientData = "Disconnected";
        bleShowNotification("* MAT KET NOI *");
        if (clickSound) {
            tone(BUZZER, 800, 150);
            delay(200);
            tone(BUZZER, 600, 150);
        }
    }
};

//////////////// BLE Scan Callback ////////////////
class ScanDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (foundDevices < MAX_FOUND_DEVICES) {
            String name = advertisedDevice.getName().c_str();
            if (name.length() == 0) name = "Unknown";
            devNames[foundDevices] = name;
            devRSSI[foundDevices] = advertisedDevice.getRSSI();
            devAddrs[foundDevices] = advertisedDevice.getAddress().toString().c_str();
            foundDevices++;
        }
    }
};

//////////////// BLE HID Functions ////////////////
void sendMouseReport(int8_t x, int8_t y, int8_t wheel, uint8_t buttons) {
    if (bleConnected && pHIDReport != nullptr) {
        mouseReport[0] = buttons;
        mouseReport[1] = x;
        mouseReport[2] = y;
        mouseReport[3] = wheel;
        pHIDReport->setValue(mouseReport, 4);
        pHIDReport->notify();
        delay(10);
    }
}

void sendMediaKey(uint8_t key) {
    if (bleConnected && pHIDReport != nullptr) {
        uint8_t report[4] = {key, 0, 0, 0};
        pHIDReport->setValue(report, 4);
        pHIDReport->notify();
        delay(50);
        report[0] = 0;
        pHIDReport->setValue(report, 4);
        pHIDReport->notify();
    }
}

void sendConsumerKey(uint16_t key) {
    if (bleConnected && pHIDReport != nullptr) {
        uint8_t report[4] = {0, 0, 0, 0};
        report[0] = key & 0xFF;
        report[1] = (key >> 8) & 0xFF;
        pHIDReport->setValue(report, 4);
        pHIDReport->notify();
        delay(50);
        pHIDReport->setValue(mouseReport, 4);
        pHIDReport->notify();
    }
}

void playClickSound() {
    if (clickSound) {
        tone(BUZZER, 800, 30);
    }
}

void playBeep(int freq, int duration) {
    tone(BUZZER, freq, duration);
}

////////////// Virtual Mouse Functions ////////////////
void bleSendVMouseMove(int16_t x, int16_t y) {
    if (bleConnected && pVMouseData != nullptr) {
        uint8_t data[5] = {0x01, (uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF),
                           (uint8_t)(y & 0xFF), (uint8_t)((y >> 8) & 0xFF)};
        pVMouseData->setValue(data, 5);
        pVMouseData->notify();
    }
}

void bleSendVMouseClick(uint8_t button) {
    if (bleConnected && pVMouseData != nullptr) {
        uint8_t data[2] = {0x02, button};
        pVMouseData->setValue(data, 2);
        pVMouseData->notify();
    }
}

void bleSendVMouseScroll(int8_t scroll) {
    if (bleConnected && pVMouseData != nullptr) {
        uint8_t data[2] = {0x03, (uint8_t)scroll};
        pVMouseData->setValue(data, 2);
        pVMouseData->notify();
    }
}

void bleSendVMouseShow() {
    if (bleConnected && pVMouseData != nullptr) {
        uint8_t data[2] = {0x10, 0x01};
        pVMouseData->setValue(data, 2);
        pVMouseData->notify();
    }
}

void bleSendVMouseHide() {
    if (bleConnected && pVMouseData != nullptr) {
        uint8_t data[2] = {0x10, 0x00};
        pVMouseData->setValue(data, 2);
        pVMouseData->notify();
    }
}

void bleSendVMousePos(int16_t x, int16_t y) {
    if (bleConnected && pVMouseData != nullptr) {
        uint8_t data[5] = {0x11, (uint8_t)(x & 0xFF), (uint8_t)((x >> 8) & 0xFF),
                           (uint8_t)(y & 0xFF), (uint8_t)((y >> 8) & 0xFF)};
        pVMouseData->setValue(data, 5);
        pVMouseData->notify();
    }
}

//////////////// BLE Functions ////////////////
void bleInit() {
    BLEDevice::init(deviceName.c_str());
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
    BLEService* pHIDService = pServer->createService(BLE_SERVICE_HID);
    
    BLECharacteristic* pHIDInfo = pHIDService->createCharacteristic("2A4A", BLECharacteristic::PROPERTY_READ);
    uint8_t hidInfo[] = {0x11, 0x01, 0x00, 0x01};
    pHIDInfo->setValue(hidInfo, 4);
    
    BLECharacteristic* pReportMap = pHIDService->createCharacteristic("2A4B", BLECharacteristic::PROPERTY_READ);
    pReportMap->setValue(hidReportMap, sizeof(hidReportMap));
    
    pHIDReport = pHIDService->createCharacteristic(BLE_SERVICE_HID, 
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR);
    pHIDReport->addDescriptor(new BLE2902());
    pHIDReport->setValue(mouseReport, 4);
    
    BLECharacteristic* pControl = pHIDService->createCharacteristic("2A4C", BLECharacteristic::PROPERTY_WRITE_NR);
    uint8_t ctrl[1] = {0x00};
    pControl->setValue(ctrl, 1);
    
    pHIDService->start();
    
    BLEService* pBatteryService = pServer->createService(BLE_SERVICE_BATTERY);
    BLECharacteristic* pBatteryLevel = pBatteryService->createCharacteristic(BLE_CHAR_BATTERY, BLECharacteristic::PROPERTY_READ);
    pBatteryService->start();
    
    pService = pServer->createService(BLE_SERVICE_UART);
    pUartRx = pService->createCharacteristic(BLE_CHAR_UART_RX, BLECharacteristic::PROPERTY_WRITE);
    pUartTx = pService->createCharacteristic(BLE_CHAR_UART_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pUartTx->addDescriptor(new BLE2902());
    pUartRx->setCallbacks(new UartRxCallback());
    pService->start();
    
    BLEService* pVMouseService = pServer->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    pVMouseData = pVMouseService->createCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26a8",
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR);
    pVMouseData->addDescriptor(new BLE2902());
    pVMouseService->start();
    
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_HID);
    pAdvertising->addServiceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x08);
    pAdvertising->setAppearance(0x05C0);
    pAdvertising->start();
    bleAdvertising = true;
}

void bleStartServer(const char* serviceUUID) {
    bleStopAll();
    bleInit();
    
    pAdvertising->addServiceUUID(serviceUUID);
    pAdvertising->start();
    bleAdvertising = true;
    bleMode = BLE_SERVER;
}

void bleStartClient() {
    bleStopAll();
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
}

bool bleConnectToDevice(const String& address, const char* serviceUUID) {
    bleStartClient();
    BLEAddress addr(address.c_str());
    if (pClient->connect(addr)) {
        pRemoteService = pClient->getService(serviceUUID);
        if (pRemoteService != nullptr) {
            pRemoteUartTx = pRemoteService->getCharacteristic(BLE_CHAR_UART_TX);
            pRemoteUartRx = pRemoteService->getCharacteristic(BLE_CHAR_UART_RX);
            if (pRemoteUartTx != nullptr && pRemoteUartRx != nullptr) {
                pRemoteUartTx->registerForNotify(nullptr);
                clientConnected = true;
                bleMode = BLE_CLIENT;
                return true;
            }
        }
    }
    if (pClient != nullptr) pClient->disconnect();
    return false;
}

bool bleClientConnectByAddress(const String& address, const String& serviceUUID) {
    if (pClient != nullptr) {
        bleClientDisconnect();
    }
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
    BLEAddress addr(address.c_str());
    if (pClient->connect(addr)) {
        if (serviceUUID.length() > 0) {
            BLERemoteService* pSvc = pClient->getService(serviceUUID.c_str());
            if (pSvc != nullptr) {
                clientConnected = true;
                bleMode = BLE_CLIENT;
                return true;
            }
        }
        clientConnected = true;
        bleMode = BLE_CLIENT;
        return true;
    }
    delete pClient;
    pClient = nullptr;
    return false;
}

void bleClientDisconnect() {
    if (pClient != nullptr) {
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        clientConnected = false;
        bleMode = BLE_IDLE;
    }
}

bool bleClientIsConnected() {
    return clientConnected && pClient != nullptr;
}

void bleSendData(const String& data) {
    if (bleMode == BLE_SERVER && pUartRx != nullptr) {
        pUartRx->setValue(data.c_str());
        pUartRx->notify();
    } else if (bleMode == BLE_CLIENT && pRemoteUartRx != nullptr) {
        pRemoteUartRx->writeValue(data.c_str());
    }
}

void bleStopAll() {
    if (pServer != nullptr) {
        if (pService != nullptr) {
            pServer->removeService(pService);
            pService = nullptr;
        }
        pServer = nullptr;
    }
    if (pClient != nullptr) {
        pClient->disconnect();
        pClient = nullptr;
    }
    if (pAdvertising != nullptr) {
        pAdvertising->stop();
        pAdvertising = nullptr;
    }
    pUartTx = nullptr;
    pUartRx = nullptr;
    pHIDReport = nullptr;
    pVMouseData = nullptr;
    pRemoteService = nullptr;
    pRemoteUartTx = nullptr;
    pRemoteUartRx = nullptr;
    bleMode = BLE_IDLE;
    bleConnected = false;
    bleAdvertising = false;
    clientConnected = false;
}

void bleScan(int duration) {
    foundDevices = 0;
    scanning = true;
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new ScanDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(duration, false);
    scanning = false;
}

String bleGetConnectedDevice() {
    if (bleMode == BLE_SERVER && bleConnected) return "Client connected";
    if (bleMode == BLE_CLIENT && clientConnected) return "Connected to server";
    return "Not connected";
}

//////////////// BLE Command Processing ////////////////
void bleProcessCommand(String cmd) {
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "BUZZER") {
        playBeep(1000, 200);
        bleSendData("BUZZER_OK");
    }
    else if (cmd == "BL_ON") {
        digitalWrite(TFT_BL, HIGH);
        backlightState = true;
        bleSendData("BACKLIGHT_ON");
    }
    else if (cmd == "BL_OFF") {
        digitalWrite(TFT_BL, LOW);
        backlightState = false;
        bleSendData("BACKLIGHT_OFF");
    }
    else if (cmd == "STATUS") {
        String status = "REMOTE:" + String(remoteModeActive ? "ON" : "OFF") + 
                       "|MOUSE:" + String(airMouseMode ? "ON" : "OFF") +
                       "|NAME:" + deviceName;
        bleSendData(status);
    }
    else if (cmd.startsWith("NAME:")) {
        String newName = cmd.substring(5);
        newName.trim();
        if (newName.length() > 0 && newName.length() < 32) {
            deviceName = newName;
            bleSendData("NAME_CHANGED:" + deviceName);
        } else {
            bleSendData("NAME_ERROR");
        }
    }
    else if (cmd == "REMOTE_ON") {
        remoteModeActive = true;
        bleSendData("REMOTE_ON");
    }
    else if (cmd == "REMOTE_OFF") {
        remoteModeActive = false;
        bleSendData("REMOTE_OFF");
    }
    else if (cmd == "MOUSE_ON") {
        airMouseMode = true;
        bleSendData("MOUSE_ON");
    }
    else if (cmd == "MOUSE_OFF") {
        airMouseMode = false;
        bleSendData("MOUSE_OFF");
    }
    else if (cmd == "MEDIA_PLAY" || cmd == "MEDIA_PAUSE") {
        sendMediaKey(0x00);
        bleSendData("PLAY");
    }
    else if (cmd == "MEDIA_PREV") {
        sendMediaKey(0x16);
        bleSendData("PREV");
    }
    else if (cmd == "MEDIA_NEXT") {
        sendMediaKey(0x15);
        bleSendData("NEXT");
    }
    else if (cmd == "MEDIA_VOL_UP") {
        sendMediaKey(0x19);
        bleSendData("VOL_UP");
    }
    else if (cmd == "MEDIA_VOL_DOWN") {
        sendMediaKey(0x18);
        bleSendData("VOL_DOWN");
    }
    else if (cmd.startsWith("MOVE:") && (remoteModeActive || airMouseMode) && bleConnected) {
        String coords = cmd.substring(5);
        int comma = coords.indexOf(',');
        if (comma > 0) {
            int8_t x = coords.substring(0, comma).toInt();
            int8_t y = coords.substring(comma + 1).toInt();
            sendMouseReport(x, y, 0, 0);
        }
        bleSendData("MOVE_OK");
    }
    else if (cmd == "CLICK" && (remoteModeActive || airMouseMode)) {
        sendMouseReport(0, 0, 0, 1);
        bleSendData("CLICK_OK");
    }
    else if (cmd == "CLICK_R" && (remoteModeActive || airMouseMode)) {
        sendMouseReport(0, 0, 0, 2);
        bleSendData("CLICK_R_OK");
    }
    else if (cmd.startsWith("CLIENT_CONN:")) {
        String target = cmd.substring(12);
        int colonIdx = target.indexOf(':');
        String addr = target.substring(0, colonIdx);
        String uuid = colonIdx > 0 ? target.substring(colonIdx + 1) : "";
        if (bleClientConnectByAddress(addr, uuid)) bleSendData("CLIENT_CONN_OK");
        else bleSendData("CLIENT_CONN_FAIL");
    }
    else if (cmd == "CLIENT_DISC") {
        bleClientDisconnect();
        bleSendData("CLIENT_DISC_OK");
    }
    else if (cmd == "CLIENT_PAIRED") {
        bleSendData("CLIENT_PAIRED:" + String(bleClientIsConnected() ? "YES" : "NO"));
    }
    else if (cmd == "CLIENT_SCAN") {
        bleScan(5);
        bleSendData("CLIENT_SCAN_DONE:" + String(foundDevices));
    }
    else if (cmd == "CLIENT_ADDRS") {
        String result = "CLIENT_ADDRS:" + String(foundDevices);
        for (int i = 0; i < foundDevices; i++) {
            result += "|" + devNames[i] + ":" + devAddrs[i] + ":" + String(devRSSI[i]) + "dBm";
        }
        bleSendData(result);
    }
    else if (cmd == "SERVER_ACCEPT") {
        bleStartServer(BLE_SERVICE_UART);
        bleSendData("SERVER_ACCEPT_OK");
    }
    else if (cmd.startsWith("SERVER_ACCEPT_UUID:")) {
        String uuid = cmd.substring(17);
        uuid.trim();
        bleStartServer(uuid.c_str());
        bleSendData("SERVER_ACCEPT_UUID_OK:" + uuid);
    }
    else if (cmd == "SERVER_STOP") {
        bleStopAll();
        bleSendData("SERVER_STOP_OK");
    }
    else if (cmd == "SERVER_STATUS") {
        String status = "SERVER_CONN:" + String(bleConnected ? "YES" : "NO") +
                       "|SERVER_ADV:" + String(bleAdvertising ? "YES" : "NO") +
                       "|SERVER_MODE:" + String(bleMode == BLE_SERVER ? "SERVER" : "IDLE");
        bleSendData(status);
    }
    else if (cmd == "SERVER_DISCONNECT") {
        if (bleConnected && pServer != nullptr) {
            bleSendData("SERVER_DISCONNECT_OK");
        } else {
            bleSendData("SERVER_DISCONNECT_NONE");
        }
    }
    else {
        bleSendData("UNKNOWN_CMD:" + cmd);
    }
}

//////////////// BLE Mouse Control ////////////////
void bleMouseControl() {
    bool mouseEnabled = (remoteModeActive || airMouseMode);
    
    if (mouseEnabled && bleConnected && (bleScreenState == 4)) {
        if (millis() - lastMoveTime > 80) {
            lastMoveTime = millis();
            
            if (buttonPressed(KEY_UP)) {
                sendMouseReport(0, -mouseSpeed, 0, 0);
                playClickSound();
            }
            if (buttonPressed(KEY_DOWN)) {
                sendMouseReport(0, mouseSpeed, 0, 0);
                playClickSound();
            }
            if (buttonPressed(KEY_LEFT)) {
                sendMouseReport(-mouseSpeed, 0, 0, 0);
                playClickSound();
            }
            if (buttonPressed(KEY_RIGHT)) {
                sendMouseReport(mouseSpeed, 0, 0, 0);
                playClickSound();
            }
        }
    }
    
    if (mouseEnabled && (bleScreenState == 4)) {
        currentHover = -1;
        if (digitalRead(KEY_UP) == LOW) currentHover = 0;
        else if (digitalRead(KEY_DOWN) == LOW) currentHover = 1;
        else if (digitalRead(KEY_LEFT) == LOW) currentHover = 2;
        else if (digitalRead(KEY_RIGHT) == LOW) currentHover = 3;
    }
    
    if (mouseEnabled && buttonPressed(KEY_A) && (bleScreenState == 4)) {
        if (bleConnected) {
            sendMouseReport(0, 0, 0, 2);
            playClickSound();
        }
        rightBtnDown = true;
    } else if (digitalRead(KEY_OPTION) == HIGH && rightBtnDown) {
        if (bleConnected) sendMouseReport(0, 0, 0, 0);
        rightBtnDown = false;
    }
    
    if (mouseEnabled && buttonPressed(KEY_OPTION) && (bleScreenState == 4)) {
        if (bleConnected) {
            sendMouseReport(0, 0, 0, 1);
            playClickSound();
        }
        okBtnDown = true;
    } else if (digitalRead(KEY_OPTION) == HIGH && okBtnDown) {
        if (bleConnected) sendMouseReport(0, 0, 0, 0);
        okBtnDown = false;
    }
}

//////////////// BLE UI Drawing ////////////////
void bleDrawMain() {
    bleScreenState = 0;
    gfx->fillScreen(COLOR_BG);
    
    // Top Bar
    gfx->fillRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(5, 6);
    gfx->print("Menu BLE");
    
    gfx->setCursor(180, 6);
    gfx->print("65%");
    gfx->drawRect(210, 5, 20, 10, COLOR_WHITE);
    gfx->fillRect(212, 7, 12, 6, COLOR_GREEN);
    gfx->fillRect(230, 8, 2, 4, COLOR_WHITE);
    
    const char* items[] = {"DIEU KHIEN", "VMOUSE", "QUET BLE", "BLE KHACH", "BLE MAY CHU"};
    for (int i = 0; i < 5; i++) {
        int y = 40 + i * 35;
        if (i == bleCursor) {
            gfx->fillRect(5, y - 5, 230, 35, COLOR_SEL_BG);
            gfx->setTextColor(COLOR_WHITE);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        gfx->setCursor(15, y + 5);
        gfx->print(items[i]);
        
        // Separator line
        gfx->drawFastHLine(5, y + 30, 230, COLOR_DARK_GRAY);
    }
    
    // Bottom Softkeys
    gfx->fillRect(0, 300, SCREEN_WIDTH, 20, COLOR_BG);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(5, 305);
    gfx->print("Chon");
    
    gfx->setCursor(SCREEN_WIDTH - 35, 305);
    gfx->print("Thoat");
}

void bleDrawControl() {
    bleScreenState = 4;
    bool isActive = (remoteModeActive || airMouseMode);
    
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_YELLOW);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(10, 8);
    gfx->print("Dieu khien");
    
    gfx->setTextColor(bleConnected ? COLOR_GREEN : COLOR_RED);
    gfx->setCursor(10, 35);
    gfx->print(bleConnected ? "[ CONNECTED ]" : "[ NOT CONNECTED ]");
    
    const char* modes[] = {"Dieu khien xa", "Chuot", "Ca hai"};
    
    for (int i = 0; i < 3; i++) {
        int y = 60 + i * 30;
        if (i == (int)controlMode) {
            gfx->fillRect(10, y, 220, 25, COLOR_GREEN);
            gfx->setTextColor(COLOR_BG);
        } else {
            gfx->setTextColor(COLOR_WHITE);
        }
        gfx->setCursor(20, y + 6);
        gfx->print(modes[i]);
    }
    
    int cx = 120, cy = 200;
    gfx->drawRect(cx - 50, cy - 50, 100, 100, COLOR_WHITE);
    
    uint16_t upColor = (currentHover == 0) ? COLOR_YELLOW : COLOR_GREEN;
    gfx->fillRect(cx - 12, cy - 45, 24, 30, upColor);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(cx - 3, cy - 40);
    gfx->print("^");
    
    uint16_t downColor = (currentHover == 1) ? COLOR_YELLOW : COLOR_GREEN;
    gfx->fillRect(cx - 12, cy + 15, 24, 30, downColor);
    gfx->setCursor(cx - 3, cy + 20);
    gfx->print("v");
    
    uint16_t leftColor = (currentHover == 2) ? COLOR_YELLOW : COLOR_GREEN;
    gfx->fillRect(cx - 45, cy - 12, 30, 24, leftColor);
    gfx->setCursor(cx - 40, cy - 7);
    gfx->print("<");
    
    uint16_t rightColor = (currentHover == 3) ? COLOR_YELLOW : COLOR_GREEN;
    gfx->fillRect(cx + 15, cy - 12, 30, 24, rightColor);
    gfx->setCursor(cx + 17, cy - 7);
    gfx->print(">");
    
    gfx->fillCircle(cx, cy, 18, COLOR_YELLOW);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(cx - 5, cy - 4);
    gfx->print("OK");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 285);
        gfx->print("A:Trai | B:Phai | BACK:Thoat");
}

void bleDrawScan() {
    bleScreenState = 7;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_YELLOW);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(10, 8);
    gfx->print("Quet BLE");
    
    if (scanning) {
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(20, 60);
        gfx->print("Dang quet...");
    } else if (foundDevices == 0) {
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(20, 60);
        gfx->print("Nhan OK de quet");
        gfx->setCursor(20, 80);
        gfx->print("Khong tim thay thiet bi");
    } else {
        gfx->setTextColor(COLOR_WHITE);
        gfx->setCursor(20, 35);
        gfx->print("Tim thay: ");
        gfx->print(foundDevices);
        gfx->print(" thiet bi");
        
        int maxShow = 6;
        int y = 55;
        for (int i = 0; i < foundDevices && i < maxShow; i++) {
            if (i == selectedDevice) {
                gfx->fillRect(10, y - 2, 220, 28, COLOR_YELLOW);
                gfx->setTextColor(COLOR_BG);
            } else {
                gfx->setTextColor(COLOR_WHITE);
            }
            gfx->setCursor(15, y + 4);
            gfx->print(devNames[i].substring(0, 12).c_str());
            gfx->setCursor(150, y + 4);
            gfx->print(devRSSI[i]);
            gfx->print(" dBm");
            y += 30;
        }
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("UP/DOWN:Chon | OK:Quet | BACK:Back");
}

void bleDrawClient() {
    bleScreenState = 8;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_YELLOW);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(10, 8);
    gfx->print("BLE Khach");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(20, 40);
    gfx->print("Trang thai:");
    gfx->setCursor(20, 56);
    gfx->print(bleGetConnectedDevice().c_str());
    
    gfx->setCursor(20, 90);
    gfx->print("OK: Quet & Ket noi");
    gfx->setCursor(20, 110);
    gfx->print("A: Gui lenh");
    gfx->setCursor(20, 130);
    gfx->print("B: Nhan du lieu");
    
    if (clientData.length() > 0) {
        gfx->setTextColor(COLOR_GREEN);
        gfx->setCursor(20, 170);
        gfx->print(clientData.c_str());
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
    gfx->print("MENU:Back");
}

void bleDrawServer() {
    bleScreenState = 12;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_YELLOW);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(10, 8);
    gfx->print("BLE May chu");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(20, 35);
    gfx->print("Ten:");
    gfx->setCursor(60, 35);
    gfx->print(deviceName.c_str());
    
    gfx->setCursor(20, 55);
    gfx->print("Trang thai:");
    gfx->setCursor(20, 71);
    gfx->print(bleAdvertising ? "Dang quang cao..." : "Dang cho...");
    
    gfx->setCursor(20, 95);
    gfx->print(bleConnected ? "Da ket noi!" : "Chua ket noi");
    
    gfx->fillRect(10, 115, 220, 50, COLOR_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(15, 120);
        gfx->print("Dich vu:");
    gfx->setCursor(15, 135);
    gfx->print("HID:1812 | BAT:180F | UART");
    
    if (serverLog.length() > 0) {
        gfx->setTextColor(COLOR_GREEN);
        gfx->setCursor(20, 175);
        gfx->print(serverLog.c_str());
    }
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 280);
        gfx->print("OK:BD/Dung | A:HID | BACK:Thoat");
}

void bleDrawSettings() {
    bleScreenState = 6;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_YELLOW);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(10, 8);
    gfx->print("Cai dat");
    
    const char* labels[] = {
        "Remote Mode",
        "Mouse Mode",
        "Click Sound",
        "Move Mode",
        "Mouse Speed",
        "Den nen"
    };
    
    for (int i = 0; i < 6; i++) {
        int y = 35 + i * 28;
        uint16_t bg = (i == bleSettingIndex) ? COLOR_YELLOW : COLOR_GRAY;
        gfx->fillRect(10, y, 220, 25, bg);
        gfx->setTextColor(i == bleSettingIndex ? COLOR_BG : COLOR_WHITE);
        gfx->setCursor(15, y + 6);
        gfx->print(labels[i]);
        
        gfx->setCursor(160, y + 6);
        if (i == 0) gfx->print(remoteModeActive ? "ON" : "OFF");
        else if (i == 1) gfx->print(airMouseMode ? "ON" : "OFF");
        else if (i == 2) gfx->print(clickSound ? "ON" : "OFF");
        else if (i == 3) gfx->print(continuousMove ? "FAST" : "STEP");
        else if (i == 4) gfx->print(mouseSpeed);
        else if (i == 5) gfx->print(backlightState ? "ON" : "OFF");
    }
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(15, 205);
    gfx->print("Ten: ");
    gfx->print(deviceName.c_str());
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
        gfx->print("OK:Chuyen | UP/DOWN:Di chuyen | BACK:Thoat");
}

void bleDrawKeyboard() {
    bleScreenState = 5;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_YELLOW);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(10, 8);
    gfx->print(keyboardUppercase ? "UPPERCASE" : "lowercase");
    
    gfx->fillRect(10, 30, 220, 30, COLOR_GRAY);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(15, 38);
    gfx->print(keyboardInput.c_str());
    
    const char row1[] = "1234567890";
    const char row2[] = "QWERTYUIOP";
    const char row3[] = "ASDFGHJKL";
    const char row4[] = "ZXCVBNM";
    
    int y = 75;
    for (int i = 0; i < 10; i++) {
        int x = 15 + i * 21;
        gfx->drawRect(x, y, 18, 22, COLOR_WHITE);
        gfx->setCursor(x + 4, y + 6);
        gfx->print(keyboardUppercase ? row2[i] : tolower(row2[i]));
    }
    
    y = 100;
    for (int i = 0; i < 9; i++) {
        int x = 25 + i * 21;
        gfx->drawRect(x, y, 18, 22, COLOR_WHITE);
        gfx->setCursor(x + 4, y + 6);
        gfx->print(keyboardUppercase ? row3[i] : tolower(row3[i]));
    }
    
    y = 125;
    for (int i = 0; i < 7; i++) {
        int x = 45 + i * 21;
        gfx->drawRect(x, y, 18, 22, COLOR_WHITE);
        gfx->setCursor(x + 4, y + 6);
        gfx->print(keyboardUppercase ? row4[i] : tolower(row4[i]));
    }
    
    gfx->fillRect(15, 155, 40, 22, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(20, 160);
    gfx->print(keyboardUppercase ? "ABC" : "abc");
    
    gfx->fillRect(65, 155, 80, 22, COLOR_GREEN);
    gfx->setCursor(85, 160);
    gfx->print("SPACE");
    
    gfx->fillRect(155, 155, 75, 22, COLOR_RED);
    gfx->setCursor(175, 160);
    gfx->print("OK");
    
    gfx->setTextColor(COLOR_GRAY);
    gfx->setCursor(10, 290);
        gfx->print("SEL:ABC  A:Cach  B:Xoa");
}

void bleDrawVMouse() {
    bleScreenState = 13;
    gfx->fillScreen(COLOR_BG);
    gfx->fillRect(0, 0, SCREEN_WIDTH, 24, COLOR_BLUE);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 8);
        gfx->print("Dieu khien Chuot");
    
    gfx->setTextColor(bleConnected ? COLOR_GREEN : COLOR_RED);
    gfx->setCursor(10, 35);
    gfx->print(bleConnected ? "[DA KET NOI]" : "[CHUA KET NOI]");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 60);
        gfx->print("Dieu khien chuot dien thoai");
    gfx->setCursor(10, 75);
    gfx->print("dien thoai qua BLE");
    
    int cx = 120, cy = 180;
    gfx->drawRect(cx - 50, cy - 50, 100, 100, COLOR_WHITE);
    
    gfx->fillRect(cx - 12, cy - 45, 24, 30, COLOR_GREEN);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(cx - 3, cy - 40);
    gfx->print("^");
    
    gfx->fillRect(cx - 12, cy + 15, 24, 30, COLOR_GREEN);
    gfx->setCursor(cx - 3, cy + 20);
    gfx->print("v");
    
    gfx->fillRect(cx - 45, cy - 12, 30, 24, COLOR_GREEN);
    gfx->setCursor(cx - 40, cy - 7);
    gfx->print("<");
    
    gfx->fillRect(cx + 15, cy - 12, 30, 24, COLOR_GREEN);
    gfx->setCursor(cx + 17, cy - 7);
    gfx->print(">");
    
    gfx->fillCircle(cx, cy, 18, COLOR_YELLOW);
    gfx->setTextColor(COLOR_BG);
    gfx->setCursor(cx - 5, cy - 4);
    gfx->print("OK");
    
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(20, 270);
        gfx->print("A:Click trai");
        
        gfx->setCursor(20, 70);
        gfx->print("B:Click phai");
        
        gfx->setCursor(20, 90);
        gfx->print("MENU:Thoat");
}

//////////////// BLE Input Handler ////////////////
void bleInputHandler() {
    bleMouseControl();
    
    switch (bleMenuState) {
        case BLE_MAIN:
            if (buttonPressed(KEY_UP)) {
                bleCursor = (bleCursor - 1 + 5) % 5;
                bleDrawMain();
            }
            if (buttonPressed(KEY_DOWN)) {
                bleCursor = (bleCursor + 1) % 5;
                bleDrawMain();
            }
            if (buttonPressed(KEY_START)) {
                switch (bleCursor) {
                    case 0: bleMenuState = BLE_CONTROL; bleDrawControl(); break;
                    case 1: 
                        bleMenuState = BLE_VMOUSE; 
                        bleSendVMouseShow();
                        bleDrawVMouse(); 
                        break;
                    case 2: 
                        bleMenuState = BLE_SCAN; 
                        selectedDevice = 0;
                        foundDevices = 0;
                        bleScan(5); 
                        bleDrawScan(); 
                        break;
                    case 3: bleMenuState = BLE_CLIENT_MENU; bleStopAll(); bleInit(); if(pAdvertising) pAdvertising->stop(); bleAdvertising = false; bleDrawClient(); break;
                    case 4: bleMenuState = BLE_SERVER_MENU; bleStopAll(); bleInit(); bleDrawServer(); break;
                }
            }
            if (buttonPressed(KEY_A)) {
                systemState = SYS_MAIN;
                drawMainUI();
            }
            break;
            
        case BLE_CONTROL:
            if (buttonPressed(KEY_UP)) {
                controlMode = (ControlMode)((controlMode + 2) % 3);
                if (controlMode == MODE_REMOTE || controlMode == MODE_BOTH) remoteModeActive = true;
                if (controlMode == MODE_MOUSE || controlMode == MODE_BOTH) airMouseMode = true;
                if (controlMode == MODE_REMOTE) airMouseMode = false;
                if (controlMode == MODE_MOUSE) remoteModeActive = false;
                bleDrawControl();
            }
            if (buttonPressed(KEY_DOWN)) {
                controlMode = (ControlMode)((controlMode + 1) % 3);
                if (controlMode == MODE_REMOTE || controlMode == MODE_BOTH) remoteModeActive = true;
                if (controlMode == MODE_MOUSE || controlMode == MODE_BOTH) airMouseMode = true;
                if (controlMode == MODE_REMOTE) airMouseMode = false;
                if (controlMode == MODE_MOUSE) remoteModeActive = false;
                bleDrawControl();
            }
            if (buttonPressed(KEY_LEFT)) {
                airMouseMode = !airMouseMode;
                if (airMouseMode) remoteModeActive = true;
                bleDrawControl();
            }
            if (buttonPressed(KEY_RIGHT)) {
                remoteModeActive = !remoteModeActive;
                if (remoteModeActive) airMouseMode = true;
                bleDrawControl();
            }
            if (buttonPressed(KEY_A)) {
                bleMenuState = BLE_MAIN;
                bleCursor = 0;
                bleDrawMain();
            }
            break;
            
        case BLE_SCAN:
            if (buttonPressed(KEY_UP)) {
                if (foundDevices > 0) {
                    selectedDevice = (selectedDevice - 1 + foundDevices) % foundDevices;
                    bleDrawScan();
                }
            }
            if (buttonPressed(KEY_DOWN)) {
                if (foundDevices > 0) {
                    selectedDevice = (selectedDevice + 1) % foundDevices;
                    bleDrawScan();
                }
            }
            if (buttonPressed(KEY_START)) {
                bleScan(5);
                selectedDevice = 0;
                bleDrawScan();
            }
            if (buttonPressed(KEY_A)) {
                bleMenuState = BLE_MAIN;
                bleCursor = 2;
                bleDrawMain();
            }
            break;
            
        case BLE_CLIENT_MENU:
            if (buttonPressed(KEY_START)) {
                bleScan(5);
                bleDrawClient();
            }
            if (buttonPressed(KEY_A)) {
                bleMenuState = BLE_MAIN;
                bleCursor = 3;
                bleDrawMain();
            }
            break;
            
        case BLE_SERVER_MENU:
            if (buttonPressed(KEY_START)) {
                if (bleAdvertising) {
                    if (pAdvertising != nullptr) pAdvertising->stop();
                    bleAdvertising = false;
                } else {
                    bleStartServer(BLE_SERVICE_UART);
                }
                bleDrawServer();
            }
            if (buttonPressed(KEY_START)) {
                bleStartServer(BLE_SERVICE_HID);
                bleDrawServer();
            }
            if (buttonPressed(KEY_A)) {
                bleMenuState = BLE_MAIN;
                bleCursor = 4;
                bleDrawMain();
            }
            break;
            
        case BLE_SETTINGS:
            if (buttonPressed(KEY_UP)) {
                bleSettingIndex = (bleSettingIndex > 0) ? bleSettingIndex - 1 : 5;
                bleDrawSettings();
            }
            if (buttonPressed(KEY_DOWN)) {
                bleSettingIndex = (bleSettingIndex < 5) ? bleSettingIndex + 1 : 0;
                bleDrawSettings();
            }
            if (buttonPressed(KEY_START)) {
                switch (bleSettingIndex) {
                    case 0: remoteModeActive = !remoteModeActive; break;
                    case 1: airMouseMode = !airMouseMode; break;
                    case 2: clickSound = !clickSound; break;
                    case 3: continuousMove = !continuousMove; break;
                    case 4: mouseSpeed = (mouseSpeed < 20) ? mouseSpeed + 2 : 5; break;
                    case 5: 
                        backlightState = !backlightState;
                        digitalWrite(TFT_BL, backlightState ? HIGH : LOW);
                        break;
                }
                bleDrawSettings();
            }
            if (buttonPressed(KEY_A)) {
                bleMenuState = BLE_KEYBOARD;
                keyboardInput = deviceName;
                bleDrawKeyboard();
            }
            if (buttonPressed(KEY_A)) {
                bleMenuState = BLE_MAIN;
                bleCursor = 0;
                bleDrawMain();
            }
            break;
            
        case BLE_KEYBOARD:
            if (buttonPressed(KEY_START)) {
                keyboardUppercase = !keyboardUppercase;
                bleDrawKeyboard();
            }
            if (buttonPressed(KEY_OPTION)) {
                keyboardInput += " ";
                bleDrawKeyboard();
            }
            if (buttonPressed(KEY_B)) {
                if (keyboardInput.length() > 0) {
                    keyboardInput.remove(keyboardInput.length() - 1);
                }
                bleDrawKeyboard();
            }
            if (buttonPressed(KEY_MENU) || buttonPressed(KEY_RIGHT)) {
                if (keyboardInput.length() > 0) {
                    deviceName = keyboardInput;
                    keyboardInput = "";
                }
                bleMenuState = BLE_SETTINGS;
                bleDrawSettings();
            }
            break;
            
        case BLE_VMOUSE:
            if (buttonPressed(KEY_UP)) {
                bleSendVMouseMove(0, -10);
                playClickSound();
            }
            if (buttonPressed(KEY_DOWN)) {
                bleSendVMouseMove(0, 10);
                playClickSound();
            }
            if (buttonPressed(KEY_LEFT)) {
                bleSendVMouseMove(-10, 0);
                playClickSound();
            }
            if (buttonPressed(KEY_RIGHT)) {
                bleSendVMouseMove(10, 0);
                playClickSound();
            }
            if (buttonPressed(KEY_OPTION)) {
                bleSendVMouseClick(0x01);
                playClickSound();
            }
            if (buttonPressed(KEY_A)) {
                bleSendVMouseClick(0x02);
                playClickSound();
            }
            if (buttonPressed(KEY_START)) {
                bleSendVMouseClick(0x01);
                delay(50);
                bleSendVMouseClick(0x00);
                playClickSound();
            }
            if (buttonPressed(KEY_MENU)) {
                bleSendVMouseHide();
                bleMenuState = BLE_MAIN;
                bleCursor = 0;
                bleDrawMain();
            }
            break;
    }
}

void appBLE() {
    bleMenuState = BLE_MAIN;
    bleCursor = 0;
    bleSettingIndex = 0;
    selectedDevice = 0;
    foundDevices = 0;
    bleInit();
    bleDrawMain();
    
    while (true) {
        bleInputHandler();
        delay(50);
        if (systemState == SYS_MAIN) {
            break;
        }
    }
}
