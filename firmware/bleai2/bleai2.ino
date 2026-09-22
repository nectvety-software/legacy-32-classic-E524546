#define ST7789_DRIVER
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_SDA 12
#define TFT_SCL 48
#define TFT_CS 14
#define TFT_DC 47
#define TFT_RST 3
#define TFT_BL 39
#define SPI_FREQUENCY 20000000

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertisedDevice.h>

TFT_eSPI tft = TFT_eSPI();

void playBeep(int freq, int duration);
void clientDisconnect();
void serverStopAdvertising();
void serverAcceptConnection();
void serverAcceptConnectionWithUUID(String uuid);
void serverStopAccepting();
bool serverIsAccepting();
bool serverIsConnected();
void drawServerMenu();
void drawServerUUIDSelect();
void drawMainMenu();

#define MAX_CLIENT_SERVICES 10
#define MAX_CLIENT_CHARS 10
#define CLIENT_BUFFER_SIZE 512
#define MAX_SERVER_CLIENTS 3

#define KEY_UP 7
#define KEY_DOWN 46
#define KEY_LEFT 45
#define KEY_RIGHT 6
#define KEY_MENU 18
#define KEY_OPTION 8
#define KEY_SELECT 16
#define KEY_START 17
#define KEY_A 15
#define KEY_B 5

#define PIN_BAT 1

#define BGCOLOR 0x0000
#define FGCOLOR 0x07E0
#define TXTCOLOR 0xFFFF
#define ERRCOLOR 0xF800
#define BUTTON_BG 0x1082
#define SELECTED 0xF8B8
#define HID_SERVICE_UUID "1812"
#define HID_REPORT_UUID "2A4D"
#define BATTERY_SERVICE_UUID "180F"
#define BATTERY_LEVEL_UUID "2A19"
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

#define EEPROM_SIZE 64
#define NAME_ADDR 0
#define CLICK_SOUND_ADDR 32

BLEServer *pServer = NULL;
BLECharacteristic *pHIDReport = NULL;
BLECharacteristic *pUART = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

int currentMenu = 0;
int settingIndex = 0;
bool airMouseMode = false;
bool remoteModeActive = false;
bool keyboardMode = false;
bool gamepadMode = false;
bool clickSound = true;
bool continuousMove = true;
int mouseSpeed = 10;
bool editingName = false;
bool tiktokMode = false;
int currentHover = -1;
unsigned long bLongPress = 0;

bool mouseLeftHeld = false;
bool mouseRightHeld = false;
unsigned long mouseLeftPressTime = 0;
unsigned long mouseRightPressTime = 0;
bool mouseDragging = false;
int8_t mouseDragStartX = 0;
int8_t mouseDragStartY = 0;
bool doubleClickEnabled = false;
unsigned long lastLeftClickTime = 0;

int keyboardModeType = 0;
int kbQuickRow = 0;
bool kbShiftHeld = false;
bool kbCtrlHeld = false;
bool kbAltHeld = false;
bool kbGuiHeld = false;

int gamepadModeType = 0;
int8_t gamepadX = 0;
int8_t gamepadY = 0;
int8_t gamepadZ = 0;
int8_t gamepadRz = 0;
bool gamepadBtn[8] = {false};

int airMouseTab = 0;

int settingsSelect = 0;
int gamepadBrightness = 80;
bool gamepadSound = true;
bool gamepadVibration = false;
int sleepTimer = 30;
const char* btPowerLevels[] = {"Low", "Medium", "High"};
int btPowerIndex = 2;

unsigned long lastActivityTime = 0;
int screenBrightness = 255;
bool screenSleeping = false;
int animationFrame = 0;
unsigned long lastFrameTime = 0;

struct Particle {
    int16_t x, y;
    int8_t vx, vy;
    uint8_t life;
    uint16_t color;
};
#define MAX_PARTICLES 30
Particle particles[MAX_PARTICLES];
int particleCount = 0;

int remoteHoverBtn = -1;
bool remoteBtnPressed[12] = {false};
unsigned long remoteBtnPressTime[12] = {0};

bool animScaleDown[12] = {false};
float animScale[12] = {1.0f};

int screenFlashAlpha = 0;
uint32_t screenFlashColor = 0x07E0;
unsigned long lastDimTime = 0;
int currentBrightness = 255;
int targetBrightness = 255;
bool scanning = false;
int foundDevices = 0;
int scanOffset = 0;
int selectedDevice = 0;
String devNames[20];
int devRSSI[20];
String devAddrs[20];

BLEScan* pBLEScan;

BLEClient* pClient = NULL;
bool clientConnected = false;
String clientConnectedName = "";
String clientConnectedAddr = "";
bool clientConnecting = false;
int clientScanOffset = 0;
int clientSelectedDevice = 0;
int clientFoundDevices = 0;
String clientDevNames[20];
int clientDevRSSI[20];
String clientDevAddrs[20];
String clientDevUUIDs[20];
String clientTargetUUID = "";
bool clientScanning = false;
bool clientFilterByUUID = false;
int clientScreenState = 0;
int clientSelectedService = 0;
int clientSelectedChar = 0;
char clientReceiveBuffer[CLIENT_BUFFER_SIZE];
int clientReceiveIndex = 0;
void* clientNotifyChar = NULL;

bool serverAccepting = false;
bool serverAcceptingWithUUID = false;
String serverAcceptUUID = "";
bool serverWaitingConnection = false;
BLEAdvertising* pServerAdvertising = NULL;
bool serverAdvStarted = false;

class ClientNotifyCallbacks;
class ClientAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (clientFoundDevices < 20) {
            String name = advertisedDevice.getName().c_str();
            if (name.length() == 0) name = "Unknown";
            clientDevNames[clientFoundDevices] = name;
            clientDevRSSI[clientFoundDevices] = advertisedDevice.getRSSI();
            clientDevAddrs[clientFoundDevices] = advertisedDevice.getAddress().toString().c_str();
            if (advertisedDevice.haveServiceUUID()) {
                clientDevUUIDs[clientFoundDevices] = advertisedDevice.getServiceUUID().toString().c_str();
            } else {
                clientDevUUIDs[clientFoundDevices] = "";
            }
            clientFoundDevices++;
        }
    }
};

class ClientCallbacks: public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        clientConnected = true;
        if (clickSound) { playBeep(2000, 80); delay(100); playBeep(2500, 120); }
        Serial.println("BLE Client Connected");
    }
    void onDisconnect(BLEClient* pclient) {
        clientConnected = false;
        clientConnectedName = "";
        clientConnectedAddr = "";
        clientNotifyChar = NULL;
        if (clickSound) { playBeep(800, 150); delay(200); playBeep(600, 150); }
        Serial.println("BLE Client Disconnected");
    }
};

class ClientNotifyCallbacks {
public:
    void onNotify(void* pChar, uint8_t* data, size_t length, bool isNotify) {
        Serial.print("Client RX: ");
        for (size_t i = 0; i < length && clientReceiveIndex < CLIENT_BUFFER_SIZE - 1; i++) {
            char c = (char)data[i];
            if (c >= 32 && c <= 126) {
                clientReceiveBuffer[clientReceiveIndex++] = c;
            }
        }
        clientReceiveBuffer[clientReceiveIndex] = 0;
        Serial.println((char*)data);
    }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (foundDevices < 20) {
            String name = advertisedDevice.getName().c_str();
            if (name.length() == 0) name = "Unknown";
            devNames[foundDevices] = name;
            devRSSI[foundDevices] = advertisedDevice.getRSSI();
            devAddrs[foundDevices] = advertisedDevice.getAddress().toString().c_str();
            foundDevices++;
        }
    }
};

void clientDisconnect() {
    if (pClient) {
        pClient->disconnect();
        delete pClient;
        pClient = NULL;
        clientConnected = false;
        clientConnectedName = "";
        clientConnectedAddr = "";
        clientNotifyChar = NULL;
        if (clickSound) playBeep(600, 100);
    }
}

bool clientConnectByAddress(String address, String serviceUUID) {
    if (pClient) {
        clientDisconnect();
        delay(300);
    }
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
    BLEAddress addr(address.c_str());
    Serial.println("Connecting to: " + address);
    if (pClient->connect(addr)) {
        clientConnectedAddr = address;
        if (serviceUUID.length() > 0) {
            BLERemoteService* pRemoteService = pClient->getService(serviceUUID.c_str());
            if (pRemoteService) {
                clientConnectedName = "Service Connected";
                return true;
            }
        }
        clientConnectedName = "Connected";
        return true;
    }
    delete pClient;
    pClient = NULL;
    return false;
}

bool clientConnectByUUID(String serviceUUID) {
    if (pClient) {
        clientDisconnect();
    }
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
    Serial.println("Connecting by UUID: " + serviceUUID);
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID.c_str());
    if (pRemoteService) {
        clientConnectedName = "Service Found";
        clientConnectedAddr = pClient->getPeerAddress().toString().c_str();
        return true;
    }
    delete pClient;
    pClient = NULL;
    return false;
}

bool clientIsConnected() {
    return clientConnected && pClient != NULL;
}

int clientAvailable() {
    return clientReceiveIndex;
}

String clientRead() {
    if (clientReceiveIndex == 0) return "";
    clientReceiveBuffer[clientReceiveIndex] = 0;
    String data = String(clientReceiveBuffer);
    clientReceiveIndex = 0;
    return data;
}

void clientSendData(String data) {
    if (!clientConnected || !pClient) return;
    Serial.println("Client TX: " + data);
}

void clientSendBytes(uint8_t* data, size_t length) {
    if (!clientConnected || !pClient) return;
    Serial.println("Client TX bytes: " + String(length));
}

static BLEAdvertisedDeviceCallbacks* clientScanCallbacks = nullptr;

void startClientScan() {
    clientFoundDevices = 0;
    clientScanning = true;
    if (clientScanCallbacks) delete clientScanCallbacks;
    clientScanCallbacks = new ClientAdvertisedDeviceCallbacks();
    BLEDevice::getScan()->setAdvertisedDeviceCallbacks(clientScanCallbacks);
    BLEDevice::getScan()->setInterval(110);
    BLEDevice::getScan()->setWindow(100);
    BLEDevice::getScan()->setActiveScan(true);
    BLEDevice::getScan()->start(8, false);
    clientScanning = false;
}

void startClientScanByUUID(String uuid) {
    clientFoundDevices = 0;
    clientScanning = true;
    clientFilterByUUID = true;
    clientTargetUUID = uuid;
    if (clientScanCallbacks) delete clientScanCallbacks;
    clientScanCallbacks = new ClientAdvertisedDeviceCallbacks();
    BLEDevice::getScan()->setAdvertisedDeviceCallbacks(clientScanCallbacks);
    BLEDevice::getScan()->setInterval(110);
    BLEDevice::getScan()->setWindow(100);
    BLEDevice::getScan()->setActiveScan(true);
    BLEDevice::getScan()->start(8, false);
    clientScanning = false;
}

void serverStartAdvertising() {
    if (!serverAdvStarted) {
        BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMaxPreferred(0x12);
        pAdvertising->setAppearance(0x05C0);
        pAdvertising->start();
        serverAdvStarted = true;
        Serial.println("Server advertising started");
    }
}

class ClientNotifyCallbacks;

void serverStopAdvertising() {
    if (serverAdvStarted) {
        BLEDevice::getAdvertising()->stop();
        serverAdvStarted = false;
        Serial.println("Server advertising stopped");
    }
}

void serverAcceptConnection() {
    serverWaitingConnection = true;
    serverAccepting = true;
    serverAcceptingWithUUID = false;
    serverStartAdvertising();
    Serial.println("Server accepting connections...");
}

void serverAcceptConnectionWithUUID(String uuid) {
    serverWaitingConnection = true;
    serverAccepting = true;
    serverAcceptingWithUUID = true;
    serverAcceptUUID = uuid;
    serverStopAdvertising();
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(uuid);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->setAppearance(0x05C0);
    pAdvertising->start();
    serverAdvStarted = true;
    Serial.println("Server accepting connections with UUID: " + uuid);
}

void serverStopAccepting() {
    serverWaitingConnection = false;
    serverAccepting = false;
    serverAcceptingWithUUID = false;
    serverAcceptUUID = "";
    Serial.println("Server stopped accepting");
}

bool serverIsAccepting() {
    return serverAccepting;
}

bool serverIsConnected() {
    return deviceConnected;
}

String serverGetConnectedDevice() {
    if (deviceConnected) {
        return "Device connected";
    }
    return "(waiting)";
}

char deviceName[32] = "ESP32-Box";
char tempName[32] = "";
int kbSelectedRow = 0;
int kbSelectedCol = 0;
bool kbUpperCase = true;
bool kbShowNumbers = false;
int kbPage = 0;
int screenState = 0;

uint8_t mouseReport[4] = {0, 0, 0, 0};

void loadSettings() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(NAME_ADDR, deviceName);
    if (strlen(deviceName) == 0 || deviceName[0] == 0xFF) strcpy(deviceName, "ESP32-Box");
    uint8_t savedClickSound = EEPROM.read(CLICK_SOUND_ADDR);
    if (savedClickSound == 0 || savedClickSound == 1) clickSound = savedClickSound;
    EEPROM.end();
}

void saveSettings() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(NAME_ADDR, deviceName);
    EEPROM.write(CLICK_SOUND_ADDR, clickSound ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
}

void playBeep(int freq, int duration) { }
void playClickSound() { }

void sendMouseReport(int8_t x, int8_t y, int8_t wheel, uint8_t buttons) {
    if (deviceConnected && pHIDReport) {
        mouseReport[0] = buttons; mouseReport[1] = x; mouseReport[2] = y; mouseReport[3] = wheel;
        pHIDReport->setValue(mouseReport, 4);
        pHIDReport->notify();
        delay(10);
    }
}

void sendMediaKey(uint8_t key) {
    if (deviceConnected && pHIDReport) {
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
    if (deviceConnected && pHIDReport) {
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

void sendKeyReport(uint8_t modifiers, uint8_t key1, uint8_t key2, uint8_t key3, uint8_t key4, uint8_t key5, uint8_t key6) {
    if (deviceConnected && pHIDReport) {
        uint8_t report[8] = {modifiers, 0, key1, key2, key3, key4, key5, key6};
        pHIDReport->setValue(report, 8);
        pHIDReport->notify();
        delay(10);
        uint8_t clearReport[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        pHIDReport->setValue(clearReport, 8);
        pHIDReport->notify();
    }
}

void sendKeyWithModifier(uint8_t modifier, uint8_t key) {
    sendKeyReport(modifier, key, 0, 0, 0, 0, 0);
}

const uint8_t USB_HID_KEY_A = 0x04;
const uint8_t USB_HID_KEY_B = 0x05;
const uint8_t USB_HID_KEY_C = 0x06;
const uint8_t USB_HID_KEY_D = 0x07;
const uint8_t USB_HID_KEY_E = 0x08;
const uint8_t USB_HID_KEY_F = 0x09;
const uint8_t USB_HID_KEY_G = 0x0A;
const uint8_t USB_HID_KEY_H = 0x0B;
const uint8_t USB_HID_KEY_I = 0x0C;
const uint8_t USB_HID_KEY_J = 0x0D;
const uint8_t USB_HID_KEY_K = 0x0E;
const uint8_t USB_HID_KEY_L = 0x0F;
const uint8_t USB_HID_KEY_M = 0x10;
const uint8_t USB_HID_KEY_N = 0x11;
const uint8_t USB_HID_KEY_O = 0x12;
const uint8_t USB_HID_KEY_P = 0x13;
const uint8_t USB_HID_KEY_Q = 0x14;
const uint8_t USB_HID_KEY_R = 0x15;
const uint8_t USB_HID_KEY_S = 0x16;
const uint8_t USB_HID_KEY_T = 0x17;
const uint8_t USB_HID_KEY_U = 0x18;
const uint8_t USB_HID_KEY_V = 0x19;
const uint8_t USB_HID_KEY_W = 0x1A;
const uint8_t USB_HID_KEY_X = 0x1B;
const uint8_t USB_HID_KEY_Y = 0x1C;
const uint8_t USB_HID_KEY_Z = 0x1D;
const uint8_t USB_HID_KEY_1 = 0x1E;
const uint8_t USB_HID_KEY_2 = 0x1F;
const uint8_t USB_HID_KEY_3 = 0x20;
const uint8_t USB_HID_KEY_4 = 0x21;
const uint8_t USB_HID_KEY_5 = 0x22;
const uint8_t USB_HID_KEY_6 = 0x23;
const uint8_t USB_HID_KEY_7 = 0x24;
const uint8_t USB_HID_KEY_8 = 0x25;
const uint8_t USB_HID_KEY_9 = 0x26;
const uint8_t USB_HID_KEY_0 = 0x27;
const uint8_t USB_HID_KEY_ENTER = 0x28;
const uint8_t USB_HID_KEY_ESC = 0x29;
const uint8_t USB_HID_KEY_BACKSPACE = 0x2A;
const uint8_t USB_HID_KEY_TAB = 0x2B;
const uint8_t USB_HID_KEY_SPACE = 0x2C;
const uint8_t USB_HID_KEY_MINUS = 0x2D;
const uint8_t USB_HID_KEY_EQUAL = 0x2E;
const uint8_t USB_HID_KEY_LBRACKET = 0x2F;
const uint8_t USB_HID_KEY_RBRACKET = 0x30;
const uint8_t USB_HID_KEY_BACKSLASH = 0x31;
const uint8_t USB_HID_KEY_SEMICOLON = 0x33;
const uint8_t USB_HID_KEY_QUOTE = 0x34;
const uint8_t USB_HID_KEY_TILDE = 0x35;
const uint8_t USB_HID_KEY_COMMA = 0x36;
const uint8_t USB_HID_KEY_PERIOD = 0x37;
const uint8_t USB_HID_KEY_SLASH = 0x38;
const uint8_t USB_HID_KEY_CAPSLOCK = 0x39;
const uint8_t USB_HID_KEY_F1 = 0x3A;
const uint8_t USB_HID_KEY_F2 = 0x3B;
const uint8_t USB_HID_KEY_F3 = 0x3C;
const uint8_t USB_HID_KEY_F4 = 0x3D;
const uint8_t USB_HID_KEY_F5 = 0x3E;
const uint8_t USB_HID_KEY_F6 = 0x3F;
const uint8_t USB_HID_KEY_F7 = 0x40;
const uint8_t USB_HID_KEY_F8 = 0x41;
const uint8_t USB_HID_KEY_F9 = 0x42;
const uint8_t USB_HID_KEY_F10 = 0x43;
const uint8_t USB_HID_KEY_F11 = 0x44;
const uint8_t USB_HID_KEY_F12 = 0x45;
const uint8_t USB_HID_KEY_PRINTSCREEN = 0x46;
const uint8_t USB_HID_KEY_SCROLLLOCK = 0x47;
const uint8_t USB_HID_KEY_PAUSE = 0x48;
const uint8_t USB_HID_KEY_INSERT = 0x49;
const uint8_t USB_HID_KEY_HOME = 0x4A;
const uint8_t USB_HID_KEY_PAGEUP = 0x4B;
const uint8_t USB_HID_KEY_DELETE = 0x4C;
const uint8_t USB_HID_KEY_END = 0x4D;
const uint8_t USB_HID_KEY_PAGEDOWN = 0x4E;
const uint8_t USB_HID_KEY_RIGHT = 0x4F;
const uint8_t USB_HID_KEY_LEFT = 0x50;
const uint8_t USB_HID_KEY_DOWN = 0x51;
const uint8_t USB_HID_KEY_UP = 0x52;
const uint8_t USB_HID_KEY_KP_NUMLOCK = 0x53;
const uint8_t USB_HID_KEY_KP_DIVIDE = 0x54;
const uint8_t USB_HID_KEY_KP_MULTIPLY = 0x55;
const uint8_t USB_HID_KEY_KP_MINUS = 0x56;
const uint8_t USB_HID_KEY_KP_PLUS = 0x57;
const uint8_t USB_HID_KEY_KP_ENTER = 0x58;
const uint8_t USB_HID_KEY_KP_1 = 0x59;
const uint8_t USB_HID_KEY_KP_2 = 0x5A;
const uint8_t USB_HID_KEY_KP_3 = 0x5B;
const uint8_t USB_HID_KEY_KP_4 = 0x5C;
const uint8_t USB_HID_KEY_KP_5 = 0x5D;
const uint8_t USB_HID_KEY_KP_6 = 0x5E;
const uint8_t USB_HID_KEY_KP_7 = 0x5F;
const uint8_t USB_HID_KEY_KP_8 = 0x60;
const uint8_t USB_HID_KEY_KP_9 = 0x61;
const uint8_t USB_HID_KEY_KP_0 = 0x62;
const uint8_t USB_HID_KEY_KP_DOT = 0x63;

const uint8_t USB_HID_MODIFIER_LEFTCTRL = 0x01;
const uint8_t USB_HID_MODIFIER_LEFTSHIFT = 0x02;
const uint8_t USB_HID_MODIFIER_LEFTALT = 0x04;
const uint8_t USB_HID_MODIFIER_LEFTGUI = 0x08;
const uint8_t USB_HID_MODIFIER_RIGHTCTRL = 0x10;
const uint8_t USB_HID_MODIFIER_RIGHTSHIFT = 0x20;
const uint8_t USB_HID_MODIFIER_RIGHTALT = 0x40;
const uint8_t USB_HID_MODIFIER_RIGHTGUI = 0x80;

uint8_t charToHIDKey(char c) {
    c = toupper(c);
    if (c >= 'A' && c <= 'Z') return USB_HID_KEY_A + (c - 'A');
    if (c >= '0' && c <= '9') return USB_HID_KEY_0 + (c - '0');
    switch(c) {
        case ' ': return USB_HID_KEY_SPACE;
        case '\n': return USB_HID_KEY_ENTER;
        case '\t': return USB_HID_KEY_TAB;
        case '\b': return USB_HID_KEY_BACKSPACE;
        case '!': return USB_HID_KEY_1;
        case '@': return USB_HID_KEY_2;
        case '#': return USB_HID_KEY_3;
        case '$': return USB_HID_KEY_4;
        case '%': return USB_HID_KEY_5;
        case '^': return USB_HID_KEY_6;
        case '&': return USB_HID_KEY_7;
        case '*': return USB_HID_KEY_8;
        case '(': return USB_HID_KEY_9;
        case ')': return USB_HID_KEY_0;
        case '-': return USB_HID_KEY_MINUS;
        case '=': return USB_HID_KEY_EQUAL;
        case '[': return USB_HID_KEY_LBRACKET;
        case ']': return USB_HID_KEY_RBRACKET;
        case '\\': return USB_HID_KEY_BACKSLASH;
        case ';': return USB_HID_KEY_SEMICOLON;
        case '\'': return USB_HID_KEY_QUOTE;
        case '`': return USB_HID_KEY_TILDE;
        case ',': return USB_HID_KEY_COMMA;
        case '.': return USB_HID_KEY_PERIOD;
        case '/': return USB_HID_KEY_SLASH;
        case '_': return USB_HID_KEY_MINUS;
        case '+': return USB_HID_KEY_EQUAL;
        case '{': return USB_HID_KEY_LBRACKET;
        case '}': return USB_HID_KEY_RBRACKET;
        case '|': return USB_HID_KEY_BACKSLASH;
        case ':': return USB_HID_KEY_SEMICOLON;
        case '"': return USB_HID_KEY_QUOTE;
        case '~': return USB_HID_KEY_TILDE;
        case '<': return USB_HID_KEY_COMMA;
        case '>': return USB_HID_KEY_PERIOD;
        case '?': return USB_HID_KEY_SLASH;
        default: return 0;
    }
}

void sendChar(char c, bool withShift) {
    uint8_t modifier = withShift ? USB_HID_MODIFIER_LEFTSHIFT : 0;
    uint8_t key = charToHIDKey(c);
    if (key > 0) sendKeyReport(modifier, key, 0, 0, 0, 0, 0);
}

void sendString(String s) {
    for (size_t i = 0; i < s.length(); i++) {
        bool needShift = false;
        char c = s[i];
        if (c >= 'A' && c <= 'Z') needShift = true;
        else if (c == '!' || c == '@' || c == '#' || c == '$' || c == '%' || 
                  c == '^' || c == '&' || c == '*' || c == '(' || c == ')' ||
                  c == '_' || c == '+' || c == '{' || c == '}' || c == '|' ||
                  c == ':' || c == '"' || c == '~' || c == '<' || c == '>' || c == '?') needShift = true;
        sendChar(c, needShift);
        delay(15);
    }
}

uint16_t blendColor(uint16_t c1, uint16_t c2, uint8_t ratio) {
    uint8_t r1 = (c1 >> 11) & 0x1F;
    uint8_t g1 = (c1 >> 5) & 0x3F;
    uint8_t b1 = c1 & 0x1F;
    uint8_t r2 = (c2 >> 11) & 0x1F;
    uint8_t g2 = (c2 >> 5) & 0x3F;
    uint8_t b2 = c2 & 0x1F;
    r1 = (r1 * ratio + r2 * (255 - ratio)) / 255;
    g1 = (g1 * ratio + g2 * (255 - ratio)) / 255;
    b1 = (b1 * ratio + b2 * (255 - ratio)) / 255;
    return (r1 << 11) | (g1 << 5) | b1;
}

void drawNeonGlow(int x, int y, int w, int h, uint16_t color, bool pressed) {
    if (pressed) {
        tft.fillRoundRect(x, y, w, h, 4, color);
        return;
    }
    for (int i = 3; i >= 0; i--) {
        uint8_t alpha = 30 + (3 - i) * 25;
        uint16_t glowColor = blendColor(color, 0x0000, alpha);
        tft.fillRoundRect(x - i, y - i, w + i*2, h + i*2, 4, glowColor);
    }
    tft.fillRoundRect(x, y, w, h, 4, color);
}

void draw3DButton(int x, int y, int w, int h, uint16_t color, bool selected, const char* label, int textSize = 1) {
    uint16_t topColor = blendColor(color, 0xFFFF, 60);
    uint16_t bottomColor = blendColor(color, 0x0000, 40);
    uint16_t shadowColor = blendColor(color, 0x0000, 50);
    
    tft.fillRoundRect(x, y + 2, w, h, 4, shadowColor);
    tft.fillRoundRect(x, y, w, h - 2, 4, bottomColor);
    tft.fillRoundRect(x, y, w, h / 2, 4, topColor);
    
    if (selected) {
        for (int i = 2; i >= 0; i--) {
            uint16_t glowColor = blendColor(0x07E0, 0x0000, 80 + i * 30);
            tft.drawRoundRect(x - i, y - i, w + i*2, h + i*2, 4, glowColor);
        }
    }
    
    tft.setTextSize(textSize);
    tft.setTextColor(0x0000);
    tft.drawCentreString(label, x + w/2, y + h/2 - 3, 1);
}

void drawCyberButton(int x, int y, int w, int h, uint16_t color, bool selected, const char* label, float scale = 1.0f) {
    int scaledW = w * scale;
    int scaledH = h * scale;
    int offsetX = (w - scaledW) / 2;
    int offsetY = (h - scaledH) / 2;
    
    if (!selected && !screenSleeping) {
        for (int i = 3; i >= 1; i--) {
            uint16_t glowColor = blendColor(color, BGCOLOR, 100 + i * 40);
            tft.fillRoundRect(x + offsetX - i, y + offsetY - i, scaledW + i*2, scaledH + i*2, 4, glowColor);
        }
    }
    
    tft.fillRoundRect(x + offsetX, y + offsetY + 2, scaledW, scaledH - 2, 4, blendColor(color, 0x0000, 30));
    tft.fillRoundRect(x + offsetX, y + offsetY, scaledW, scaledH - 4, 4, color);
    tft.fillRoundRect(x + offsetX, y + offsetY, scaledW, (scaledH - 4) / 2, 4, blendColor(color, 0xFFFF, 50));
    
    tft.setTextSize(1);
    tft.setTextColor(0x0000);
    tft.drawCentreString(label, x + w/2, y + h/2 - 3, 1);
}

void spawnParticles(int x, int y, uint16_t color, int count) {
    for (int i = 0; i < count && particleCount < MAX_PARTICLES; i++) {
        particles[particleCount].x = x;
        particles[particleCount].y = y;
        particles[particleCount].vx = random(-5, 6);
        particles[particleCount].vy = random(-8, 2);
        particles[particleCount].life = 255;
        particles[particleCount].color = color;
        particleCount++;
    }
}

void updateParticles() {
    for (int i = particleCount - 1; i >= 0; i--) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vy += 1;
        particles[i].life -= 15;
        
        if (particles[i].life < 30) {
            uint16_t c = particles[i].color;
            tft.fillCircle(particles[i].x, particles[i].y, 2, blendColor(c, BGCOLOR, particles[i].life));
        }
        
        if (particles[i].life == 0) {
            for (int j = i; j < particleCount - 1; j++) {
                particles[j] = particles[j + 1];
            }
            particleCount--;
        }
    }
}

void drawGradientBackground() {
    for (int y = 0; y < 320; y++) {
        uint8_t r = 5 + (y * 15 / 320);
        uint8_t g = 5 + (y * 8 / 320);
        uint16_t c = (r << 11) | (g << 5) | 5;
        tft.drawFastHLine(0, y, 240, c);
    }
}

void drawDPad3D(int cx, int cy, int btnSize, int currentBtn) {
    int upY = cy - btnSize * 2;
    int downY = cy + btnSize;
    int leftX = cx - btnSize * 2;
    int rightX = cx + btnSize;
    
    uint16_t normalColor = 0x1082;
    uint16_t hoverColor = 0x07E0;
    uint16_t pressedColor = 0xF800;
    
    draw3DButton(cx - btnSize/2 - 5, upY - 5, btnSize + 10, btnSize + 10, 
                 currentBtn == 0 ? hoverColor : normalColor, currentBtn == 0, "^", 2);
    draw3DButton(cx - btnSize/2 - 5, downY - 5, btnSize + 10, btnSize + 10, 
                 currentBtn == 1 ? hoverColor : normalColor, currentBtn == 1, "v", 2);
    draw3DButton(leftX - 5, cy - btnSize/2 - 5, btnSize + 10, btnSize + 10, 
                 currentBtn == 2 ? hoverColor : normalColor, currentBtn == 2, "<", 2);
    draw3DButton(rightX - 5, cy - btnSize/2 - 5, btnSize + 10, btnSize + 10, 
                 currentBtn == 3 ? hoverColor : normalColor, currentBtn == 3, ">", 2);
    draw3DButton(cx - btnSize/2 - 5, cy - btnSize/2 - 5, btnSize + 10, btnSize + 10, 
                 currentBtn == 4 ? pressedColor : SELECTED, currentBtn == 4, "OK", 1);
}

void drawABButtons(int cx, int cy, int aPressed, int bPressed) {
    int aX = cx + 70, aY = cy + 20;
    int bX = cx - 90, bY = cy + 20;
    
    draw3DButton(bX, bY, 35, 35, bPressed ? 0xF800 : 0xA000, bPressed, "B", 2);
    draw3DButton(aX, aY, 45, 45, aPressed ? 0x07E0 : 0x00A0, aPressed, "A", 2);
}

void drawMenuButtons(int y) {
    int btnW = 40, btnH = 25;
    draw3DButton(20, y, btnW, btnH, remoteBtnPressed[8] ? SELECTED : BUTTON_BG, remoteBtnPressed[8], "MENU", 1);
    draw3DButton(70, y, btnW, btnH, remoteBtnPressed[9] ? SELECTED : BUTTON_BG, remoteBtnPressed[9], "SEL", 1);
    draw3DButton(130, y, btnW, btnH, remoteBtnPressed[10] ? SELECTED : BUTTON_BG, remoteBtnPressed[10], "STRT", 1);
    draw3DButton(180, y, btnW, btnH, remoteBtnPressed[11] ? SELECTED : BUTTON_BG, remoteBtnPressed[11], "PLAY", 1);
}

void triggerScreenFlash(uint16_t color, int alpha) {
    screenFlashColor = color;
    screenFlashAlpha = alpha;
}

void drawScreenFlash() {
    if (screenFlashAlpha > 0) {
        tft.fillRect(0, 0, 240, 320, blendColor(screenFlashColor, 0x0000, screenFlashAlpha));
        screenFlashAlpha -= 15;
        if (screenFlashAlpha < 0) screenFlashAlpha = 0;
    }
}

void drawHeaderNeon(String title) {
    tft.fillRect(0, 0, 240, 28, 0x0010);
    for (int i = 0; i < 3; i++) {
        tft.drawRect(i, i, 240 - i*2, 28 - i*2, blendColor(FGCOLOR, BGCOLOR, 100 + i * 30));
    }
    tft.setTextColor(FGCOLOR);
    tft.setTextSize(2);
    tft.drawCentreString(title, 120, 5, 1);
    
    int btX = 210;
    uint16_t btColor = deviceConnected ? 0x07E0 : ERRCOLOR;
    uint32_t pulsePhase = millis() % 2000;
    if (deviceConnected) {
        float pulse = sin(pulsePhase * 3.14159 / 1000.0) * 0.5 + 0.5;
        btColor = blendColor(0x07E0, BGCOLOR, (int)(100 * (1 - pulse)));
    }
    tft.fillCircle(btX, 14, 6, btColor);
    tft.setTextSize(1);
    tft.setTextColor(0x8410);
    tft.drawString("BLE", 8, 10, 1);
}

void drawHeader(String title) {
    tft.fillRect(0, 0, 240, 25, FGCOLOR);
    tft.setTextColor(BGCOLOR);
    tft.setTextSize(2);
    tft.drawCentreString(title, 120, 4, 1);
}

void drawMainMenu() {
    screenState = 0;
    tft.fillScreen(BGCOLOR);
    drawHeader("ESP32-BOX");
    const char* items[] = {"HOME", "CONTROL", "BLE SCAN", "BLE CLIENT", "BLE SERVER", "SETTINGS"};
    for (int i = 0; i < 6; i++) {
        int y = 32 + i * 28;
        if (i == currentMenu) {
        tft.fillRoundRect(5, y, 230, 24, 4, FGCOLOR);
        tft.setTextColor(BGCOLOR);
    } else {
        tft.fillRoundRect(5, y, 230, 24, 4, BUTTON_BG);
        tft.setTextColor(TXTCOLOR);
    }
    tft.setTextSize(1);
    tft.drawString(items[i], 15, y + 6, 1);
    }
    tft.setTextSize(1);
    tft.setTextColor(0x8410);
    tft.drawCentreString("OK: Select | BACK: Back", 120, 228, 1);
}

void drawHome() {
    screenState = 1;
    tft.fillScreen(BGCOLOR);
    drawHeader("HOME");
    int bat = analogRead(PIN_BAT);
    float battery = (bat / 4095.0) * 4.2;
    int batteryPercent = map(bat, 0, 4095, 0, 100);
    
    tft.fillRoundRect(5, 32, 110, 55, 5, BUTTON_BG);
    tft.fillRoundRect(125, 32, 110, 55, 5, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("BLUETOOTH", 15, 36, 1);
    tft.drawString("STATUS", 135, 36, 1);
    tft.setTextSize(3); tft.setTextColor(FGCOLOR);
    tft.drawString("BLE", 25, 50, 1);
    tft.drawString(deviceConnected ? "ON" : "OFF", 140, 50, 1);
    
    tft.fillRoundRect(5, 92, 230, 40, 5, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("DEVICE NAME", 15, 96, 1);
    tft.setTextSize(3); tft.setTextColor(TXTCOLOR);
    tft.drawString(deviceName, 15, 112, 1);
    
    int barWidth = 100;
    tft.fillRoundRect(135, 98, barWidth, 18, 3, 0x0008);
    uint16_t batColor = batteryPercent > 50 ? FGCOLOR : (batteryPercent > 20 ? 0xF8B8 : ERRCOLOR);
    tft.fillRoundRect(135, 98, map(batteryPercent, 0, 100, 0, barWidth), 18, 3, batColor);
    tft.setTextSize(1); tft.setTextColor(TXTCOLOR);
    tft.drawString(String(batteryPercent) + "%", 140 + barWidth, 100, 1);
    
    tft.fillRoundRect(5, 138, 110, 30, 5, deviceConnected ? 0x10A2 : ERRCOLOR);
    tft.setTextSize(2); tft.setTextColor(deviceConnected ? FGCOLOR : ERRCOLOR);
    tft.drawCentreString(deviceConnected ? "CONNECTED" : "OFFLINE", 60, 145, 1);
    
    tft.fillRoundRect(125, 138, 110, 30, 5, (remoteModeActive || airMouseMode) ? FGCOLOR : BUTTON_BG);
    tft.setTextColor((remoteModeActive || airMouseMode) ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString((remoteModeActive || airMouseMode) ? "ACTIVE" : "STANDBY", 180, 145, 1);
    
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawCentreString("BACK: Menu", 120, 228, 1);
}

int currentRemoteMode = 0;
int remoteButtonId = -1;
static int remoteCounter = 42;

void drawMainRemote() {
    screenState = 2;
    tft.fillScreen(0x0A0A);
    
    tft.setTextSize(2);
    tft.setTextColor(0x00BCD4);
    tft.drawCentreString("ESP32-S3 REMOTE", 120, 8, 1);
    
    tft.drawFastHLine(10, 32, 220, 0x00BCD4);
    
    const char* modeLabels[] = {"REMOTE", "MOUSE", "KEYBOARD", "GAMEPAD"};
    int activeMode = 0;
    if (airMouseMode) activeMode = 1;
    else if (keyboardMode) activeMode = 2;
    else if (gamepadMode) activeMode = 3;
    
    tft.setTextSize(1);
    tft.setTextColor(0x00BCD4);
    tft.drawString("MODE:", 10, 38, 1);
    tft.setTextColor(0x4CAF50);
    tft.drawString(modeLabels[activeMode], 55, 38, 1);
    
    int cx = 120, cy = 130;
    
    bool btnBPressed = (remoteButtonId == 0);
    bool btnAPressed = (remoteButtonId == 1);
    bool btnOKPressed = (remoteButtonId == 2);
    
    if (btnBPressed) {
        for (int i = 3; i >= 1; i--) {
            tft.fillCircle(cx - 50, cy - 30, 22 + i * 3, 0x20A2);
        }
    }
    tft.fillCircle(cx - 50, cy - 30, 20, btnBPressed ? 0x1082 : 0x2841);
    tft.setTextSize(2);
    tft.setTextColor(btnBPressed ? 0x0000 : 0xFFFF);
    tft.drawCentreString("B", cx - 50, cy - 35, 1);
    
    if (btnAPressed) {
        for (int i = 3; i >= 1; i--) {
            tft.fillCircle(cx + 50, cy - 30, 22 + i * 3, 0xF8B8);
        }
    }
    tft.fillCircle(cx + 50, cy - 30, 20, btnAPressed ? 0xF8B8 : 0xA541);
    tft.setTextSize(2);
    tft.setTextColor(btnAPressed ? 0x0000 : 0xFFFF);
    tft.drawCentreString("A", cx + 50, cy - 35, 1);
    
    if (btnOKPressed) {
        for (int i = 3; i >= 1; i--) {
            tft.fillCircle(cx, cy, 28 + i * 3, 0x20A2);
        }
    }
    tft.fillCircle(cx, cy, 25, btnOKPressed ? 0x07E0 : 0x18C3);
    tft.setTextSize(2);
    tft.setTextColor(btnOKPressed ? 0x0000 : 0xFFFF);
    tft.drawCentreString("OK", cx, cy - 7, 1);
    
    tft.fillRoundRect(cx - 8, cy + 40, 16, 12, 2, 0x1082);
    tft.setTextColor(0xFFFF);
    tft.setTextSize(1);
    tft.drawString("#", cx - 10, cy + 42, 1);
    tft.drawNumber(remoteCounter, cx + 6, cy + 42, 1);
    
    tft.fillRoundRect(10, 260, 220, 30, 4, 0x1E1E1E);
    tft.drawRoundRect(10, 260, 220, 30, 4, 0x333333);
    tft.setTextColor(0x888888);
    tft.setTextSize(1);
    tft.drawString("Navigate: <- -> | OK: Select | A: Settings", 15, 268, 1);
    
    if (deviceConnected) {
        tft.fillCircle(210, 15, 5, 0x4CAF50);
        tft.setTextColor(0x4CAF50);
        tft.drawString("BLE", 180, 10, 1);
    } else {
        tft.fillCircle(210, 15, 5, 0xF800);
        tft.setTextColor(0xF800);
        tft.drawString("BLE", 180, 10, 1);
    }
    
    lastActivityTime = millis();
}

void drawMediaRemote() {
    drawMainRemote();
}

void drawGamepadSettings() {
    screenState = 18;
    tft.fillScreen(0x0A0A);
    
    tft.setTextSize(2);
    tft.setTextColor(0x00BCD4);
    tft.drawCentreString("GAMEPAD SETTINGS", 120, 8, 1);
    tft.drawFastHLine(10, 32, 220, 0x00BCD4);
    
    const char* labels[] = {"Brightness", "Sound", "Vibration", "Sleep Timer", "BT Power", "About"};
    const char* values[] = {
        "80%",
        "ON",
        "OFF",
        "30s",
        "High",
        "v1.0"
    };
    
    for (int i = 0; i < 6; i++) {
        int y = 45 + i * 38;
        uint16_t bgColor = (i == settingsSelect) ? 0x00BCD4 : 0x1E1E1E;
        uint16_t txtColor = (i == settingsSelect) ? 0x0000 : 0xFFFFFF;
        
        tft.fillRoundRect(10, y, 220, 32, 4, bgColor);
        
        tft.setTextSize(1);
        tft.setTextColor(txtColor);
        tft.drawString(labels[i], 20, y + 10, 1);
        
        tft.setTextColor((i == settingsSelect) ? 0x0000 : 0x00BCD4);
        
        if (i == 0) {
            String bStr = String(gamepadBrightness) + "%";
            tft.drawRightString(bStr, 215, y + 10, 1);
            tft.fillRoundRect(20, y + 20, 180, 6, 2, 0x333333);
            tft.fillRoundRect(20, y + 20, map(gamepadBrightness, 0, 100, 0, 180), 6, 2, (i == settingsSelect) ? 0x0000 : 0x4CAF50);
        } else if (i == 1) {
            tft.drawRightString(gamepadSound ? "ON" : "OFF", 215, y + 10, 1);
        } else if (i == 2) {
            tft.drawRightString(gamepadVibration ? "ON" : "OFF", 215, y + 10, 1);
        } else if (i == 3) {
            String sStr = String(sleepTimer) + "s";
            tft.drawRightString(sStr, 215, y + 10, 1);
        } else if (i == 4) {
            tft.drawRightString(btPowerLevels[btPowerIndex], 215, y + 10, 1);
        } else if (i == 5) {
            tft.drawRightString("v1.0", 215, y + 10, 1);
        }
    }
    
    if (deviceConnected) {
        tft.fillRoundRect(150, 280, 80, 20, 3, 0x1E1E1E);
        tft.fillCircle(160, 290, 4, 0x4CAF50);
        tft.setTextSize(1);
        tft.setTextColor(0x4CAF50);
        tft.drawString("CONNECTED", 170, 284, 1);
    } else {
        tft.fillRoundRect(150, 280, 80, 20, 3, 0x1E1E1E);
        tft.fillCircle(160, 290, 4, 0xF800);
        tft.setTextSize(1);
        tft.setTextColor(0xF800);
        tft.drawString("OFFLINE", 170, 284, 1);
    }
    
    tft.setTextSize(1);
    tft.setTextColor(0x888888);
    tft.drawString("Navigate: ", 10, 284, 1);
    tft.setTextColor(0x00BCD4);
    tft.drawString("<- ->", 75, 284, 1);
    tft.setTextColor(0x888888);
    tft.drawString("| OK: ", 110, 284, 1);
    tft.setTextColor(0x00BCD4);
    tft.drawString("Select", 140, 284, 1);
    tft.setTextColor(0x888888);
    tft.drawString("| B: ", 180, 284, 1);
    tft.setTextColor(0x00BCD4);
    tft.drawString("Back", 205, 284, 1);
    
    lastActivityTime = millis();
}

bool controlMenuOpen = false;
int controlModeSelect = 0;

void drawControlMode() {
    controlMenuOpen = false;
    screenState = 4;
    drawControlMain();
}

void drawControlMain() {
    bool isActive = (remoteModeActive || airMouseMode || keyboardMode || gamepadMode);
    const char* modeNames[] = {"REMOTE", "MOUSE", "KEYB", "GAME"};
    int currentMode = 0;
    if (airMouseMode) currentMode = 1;
    else if (keyboardMode) currentMode = 2;
    else if (gamepadMode) currentMode = 3;
    
    tft.fillScreen(BGCOLOR);
    drawHeader("CONTROL");
    
    tft.fillRect(0, 28, 240, 2, blendColor(FGCOLOR, BGCOLOR, 150));
    
    tft.setTextColor(0x8410); tft.setTextSize(1);
    tft.drawString("MODE:", 10, 35, 1);
    tft.setTextColor(FGCOLOR); tft.setTextSize(2);
    tft.drawString(modeNames[currentMode], 60, 33, 1);
    tft.setTextColor(isActive ? 0x07E0 : ERRCOLOR);
    tft.drawString(isActive ? "[ON]" : "[OFF]", 150, 33, 1);
    
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("MENU", 10, 55, 1);
    tft.setTextColor(TXTCOLOR);
    tft.drawString("= Change Mode", 50, 55, 1);
    
    tft.setTextColor(0x7BEF);
    tft.drawString("<- ->", 10, 68, 1);
    tft.setTextColor(TXTCOLOR);
    tft.drawString("= Switch Mode", 50, 68, 1);
    
    if (airMouseMode) {
        airMouseTab = 0;
        drawAirMouseMode();
    } else if (keyboardMode) {
        int cx = 120, cy = 150;
        tft.fillCircle(cx, cy, 40, 0x0841);
        
        tft.fillRoundRect(cx - 12, cy - 45, 24, 28, 3, currentHover == 0 ? SELECTED : FGCOLOR);
        tft.fillRoundRect(cx - 12, cy + 17, 24, 28, 3, currentHover == 1 ? SELECTED : FGCOLOR);
        tft.fillRoundRect(cx - 45, cy - 12, 28, 24, 3, currentHover == 2 ? SELECTED : FGCOLOR);
        tft.fillRoundRect(cx + 17, cy - 12, 28, 24, 3, currentHover == 3 ? SELECTED : FGCOLOR);
        tft.fillCircle(cx, cy, 18, currentHover == 4 ? ERRCOLOR : SELECTED);
        
        tft.setTextColor(BGCOLOR); tft.setTextSize(2);
        tft.drawCentreString("^", cx, cy - 42, 1);
        tft.drawCentreString("v", cx, cy + 20, 1);
        tft.drawCentreString("<", cx - 40, cy - 7, 1);
        tft.drawCentreString(">", cx + 35, cy - 7, 1);
        tft.setTextSize(1);
        tft.drawCentreString("OK", cx, cy - 4, 1);
        
        tft.setTextColor(0x8410); tft.setTextSize(1);
        tft.drawCentreString("ARROWS: MOVE | A: TYPE | B: BSPC", 120, 200, 1);
        tft.drawCentreString("START: SEND | SELECT: SHIFT", 120, 215, 1);
    } else if (gamepadMode) {
        int cx = 120, cy = 150;
        tft.fillCircle(cx, cy, 50, 0x0841);
        
        tft.fillRoundRect(cx - 10, cy - 35, 20, 20, 3, gamepadBtn[0] ? SELECTED : ERRCOLOR);
        tft.fillRoundRect(cx - 10, cy + 15, 20, 20, 3, gamepadBtn[1] ? SELECTED : ERRCOLOR);
        tft.fillRoundRect(cx - 35, cy - 10, 20, 20, 3, gamepadBtn[2] ? SELECTED : ERRCOLOR);
        tft.fillRoundRect(cx + 15, cy - 10, 20, 20, 3, gamepadBtn[3] ? SELECTED : ERRCOLOR);
        
        tft.fillRoundRect(cx - 55, cy - 15, 12, 30, 3, gamepadBtn[4] ? SELECTED : 0x18C3);
        tft.fillRoundRect(cx + 43, cy - 15, 12, 30, 3, gamepadBtn[5] ? SELECTED : 0x18C3);
        
        tft.fillCircle(cx - 35, cy + 25, 10, gamepadBtn[6] ? SELECTED : 0x18C3);
        tft.fillCircle(cx + 35, cy + 25, 10, gamepadBtn[7] ? SELECTED : 0x18C3);
        
        tft.setTextColor(0x8410); tft.setTextSize(1);
        tft.drawCentreString("D-PAD: MOVE | A/B/X/Y: BTN", 120, 205, 1);
        tft.drawCentreString("L/R: TRIGGER | START: ESC", 120, 220, 1);
    } else {
        tft.setTextColor(0x8410); tft.setTextSize(1);
        tft.drawCentreString("NO MODE ENABLED", 120, 130, 1);
        tft.drawCentreString("PRESS <- OR -> TO SWITCH", 120, 150, 1);
        tft.drawCentreString("OR PRESS MENU FOR OPTIONS", 120, 165, 1);
    }
    
    lastActivityTime = millis();
}

void drawAirMouseMode() {
    screenState = 17;
    tft.fillScreen(BGCOLOR);
    
    tft.fillRect(0, 0, 240, 28, 0x0010);
    for (int i = 0; i < 3; i++) {
        tft.drawRect(i, i, 240 - i*2, 28 - i*2, blendColor(FGCOLOR, BGCOLOR, 100 + i * 30));
    }
    tft.setTextColor(FGCOLOR);
    tft.setTextSize(2);
    tft.drawCentreString("MOUSE MODE", 120, 5, 1);
    
    const char* tabNames[] = {"MOUSE", "MEDIA", "KEYS"};
    int tabW = 70;
    int tabStartX = (240 - tabW * 3) / 2;
    
    for (int i = 0; i < 3; i++) {
        int tx = tabStartX + i * (tabW + 5);
        uint16_t bg = (i == airMouseTab) ? FGCOLOR : BUTTON_BG;
        uint16_t txt = (i == airMouseTab) ? BGCOLOR : TXTCOLOR;
        tft.fillRoundRect(tx, 32, tabW, 20, 3, bg);
        tft.setTextSize(1);
        tft.setTextColor(txt);
        tft.drawCentreString(tabNames[i], tx + tabW/2, 36, 1);
    }
    
    tft.setTextColor(0x8410);
    tft.setTextSize(1);
    tft.drawString("<- -> Switch Tab", 10, 57, 1);
    
    if (airMouseTab == 0) {
        int cx = 120, cy = 150;
        tft.fillCircle(cx, cy, 40, 0x0841);
        
        tft.fillRoundRect(cx - 12, cy - 45, 24, 28, 3, currentHover == 0 ? SELECTED : FGCOLOR);
        tft.fillRoundRect(cx - 12, cy + 17, 24, 28, 3, currentHover == 1 ? SELECTED : FGCOLOR);
        tft.fillRoundRect(cx - 45, cy - 12, 28, 24, 3, currentHover == 2 ? SELECTED : FGCOLOR);
        tft.fillRoundRect(cx + 17, cy - 12, 28, 24, 3, currentHover == 3 ? SELECTED : FGCOLOR);
        tft.fillCircle(cx, cy, 18, currentHover == 4 ? ERRCOLOR : SELECTED);
        
        tft.setTextColor(BGCOLOR); tft.setTextSize(2);
        tft.drawCentreString("^", cx, cy - 42, 1);
        tft.drawCentreString("v", cx, cy + 20, 1);
        tft.drawCentreString("<", cx - 40, cy - 7, 1);
        tft.drawCentreString(">", cx + 35, cy - 7, 1);
        tft.setTextSize(1);
        tft.drawCentreString("OK", cx, cy - 4, 1);
        
        tft.setTextColor(0x8410);
        tft.drawCentreString("D-PAD: MOVE", 120, 200, 1);
        tft.drawCentreString("A: L-CLICK | B: R-CLICK", 120, 215, 1);
        tft.drawCentreString("A+B: HOME | OK+ARROW: SCROLL", 120, 230, 1);
        
    } else if (airMouseTab == 1) {
        int btnW = 60, btnH = 35;
        int startX = (240 - btnW * 3) / 2;
        int startY = 80;
        
        drawCyberButton(startX, startY, btnW, btnH, ERRCOLOR, remoteBtnPressed[0], "PWR", 1);
        drawCyberButton(startX + btnW + 10, startY, btnW, btnH, 0x10A2, remoteBtnPressed[1], "HOME", 1);
        drawCyberButton(startX + (btnW + 10) * 2, startY, btnW, btnH, BUTTON_BG, remoteBtnPressed[2], "BACK", 1);
        
        drawCyberButton(startX, startY + btnH + 10, btnW, btnH, 0x07E0, remoteBtnPressed[3], "PREV", 1);
        drawCyberButton(startX + btnW + 10, startY + btnH + 10, btnW, btnH, 0xF8B8, remoteBtnPressed[4], "PLAY", 1);
        drawCyberButton(startX + (btnW + 10) * 2, startY + btnH + 10, btnW, btnH, 0x07E0, remoteBtnPressed[5], "NEXT", 1);
        
        drawCyberButton(startX, startY + (btnH + 10) * 2, 100, 25, ERRCOLOR, remoteBtnPressed[6], "VOL -", 1);
        drawCyberButton(startX + 130, startY + (btnH + 10) * 2, 100, 25, ERRCOLOR, remoteBtnPressed[7], "VOL +", 1);
        
        tft.setTextColor(0x8410);
        tft.setTextSize(1);
        tft.drawCentreString("A: Select | <- ->: Tab", 120, 230, 1);
        
    } else if (airMouseTab == 2) {
        int btnW = 65, btnH = 28;
        int startX = (240 - btnW * 3) / 2;
        int startY = 80;
        
        drawCyberButton(startX, startY, btnW, btnH, 0x1082, remoteBtnPressed[0], "ESC", 1);
        drawCyberButton(startX + btnW + 10, startY, btnW, btnH, 0x1082, remoteBtnPressed[1], "TAB", 1);
        drawCyberButton(startX + (btnW + 10) * 2, startY, btnW, btnH, 0x1082, remoteBtnPressed[2], "ALT", 1);
        
        drawCyberButton(startX, startY + btnH + 10, btnW, btnH, 0x1082, remoteBtnPressed[3], "CTRL", 1);
        drawCyberButton(startX + btnW + 10, startY + btnH + 10, btnW, btnH, 0x1082, remoteBtnPressed[4], "SHIFT", 1);
        drawCyberButton(startX + (btnW + 10) * 2, startY + btnH + 10, btnW, btnH, 0x1082, remoteBtnPressed[5], "DEL", 1);
        
        drawCyberButton(startX, startY + (btnH + 10) * 2, btnW, btnH, 0x1082, remoteBtnPressed[6], "F1", 1);
        drawCyberButton(startX + btnW + 10, startY + (btnH + 10) * 2, btnW, btnH, 0x1082, remoteBtnPressed[7], "F2", 1);
        drawCyberButton(startX + (btnW + 10) * 2, startY + (btnH + 10) * 2, btnW, btnH, 0x1082, remoteBtnPressed[8], "F3", 1);
        
        drawCyberButton(startX, startY + (btnH + 10) * 3, btnW, btnH, 0x1082, remoteBtnPressed[9], "F4", 1);
        drawCyberButton(startX + btnW + 10, startY + (btnH + 10) * 3, btnW, btnH, 0x1082, remoteBtnPressed[10], "F5", 1);
        drawCyberButton(startX + (btnW + 10) * 2, startY + (btnH + 10) * 3, btnW, btnH, 0x1082, remoteBtnPressed[11], "F6", 1);
        
        tft.setTextColor(0x8410);
        tft.setTextSize(1);
        tft.drawCentreString("A: Send Key | <- ->: Tab", 120, 230, 1);
    }
    
    lastActivityTime = millis();
}

void drawControlMenu() {
    controlMenuOpen = true;
    screenState = 16;
    
    tft.fillRoundRect(20, 80, 200, 160, 8, 0x0010);
    tft.drawRoundRect(20, 80, 200, 160, 8, blendColor(FGCOLOR, BGCOLOR, 100));
    tft.drawRoundRect(21, 81, 198, 158, 8, blendColor(FGCOLOR, BGCOLOR, 50));
    
    tft.setTextSize(2); tft.setTextColor(FGCOLOR);
    tft.drawCentreString("SELECT MODE", 120, 90, 1);
    
    const char* modes[] = {"REMOTE", "MOUSE", "KEYBOARD", "GAME"};
    bool modeActive[] = {remoteModeActive, airMouseMode, keyboardMode, gamepadMode};
    
    for (int i = 0; i < 4; i++) {
        int y = 115 + i * 32;
        uint16_t bg = (i == controlModeSelect) ? SELECTED : BUTTON_BG;
        uint16_t txt = (i == controlModeSelect) ? BGCOLOR : TXTCOLOR;
        if (modeActive[i]) bg = FGCOLOR, txt = BGCOLOR;
        
        tft.fillRoundRect(30, y, 180, 26, 4, bg);
        tft.setTextSize(1); tft.setTextColor(txt);
        tft.drawString(modes[i], 40, y + 7, 1);
        if (modeActive[i]) {
            tft.setTextColor(txt);
            tft.drawString("*", 170, y + 7, 1);
        }
    }
    
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawCentreString("OK: Select | <- ->: Navigate | BACK: Close", 120, 235, 1);
    
    lastActivityTime = millis();
}

void drawKeyboardMode() {
    screenState = 14;
    tft.fillScreen(BGCOLOR);
    drawHeader(keyboardModeType == 0 ? "KEYBOARD - QWERTY" : (keyboardModeType == 1 ? "KEYBOARD - NUM" : "KEYBOARD - SYM"));
    
    tft.fillRoundRect(5, 32, 230, 30, 4, BUTTON_BG);
    tft.setTextSize(2); tft.setTextColor(TXTCOLOR);
    tft.drawString(">", 8, 38, 1);
    int cursorX = 20 + strlen(tempName) * 12;
    tft.fillRect(cursorX, 38, 2, 20, FGCOLOR);
    
    uint16_t kbBg = BUTTON_BG;
    uint16_t kbSel = SELECTED;
    uint16_t kbTxt = TXTCOLOR;
    uint16_t kbTxtSel = BGCOLOR;
    
    int y1 = 70, y2 = 100, y3 = 130, y4 = 160;
    int w = 21, h = 24, g = 2;
    
    if (keyboardModeType == 0) {
        const char* rows[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
        int startX[3] = {7, 16, 25};
        int cols[3] = {10, 9, 7};
        for (int r = 0; r < 3; r++) {
            for (int i = 0; i < cols[r]; i++) {
                int x = startX[r] + i * (w + g);
                bool sel = (kbSelectedRow == r && kbSelectedCol == i);
                char c[2] = {kbUpperCase ? toupper(rows[r][i]) : rows[r][i], 0};
                tft.fillRoundRect(x, y1 + r * 30, w, h, 2, sel ? kbSel : kbBg);
                tft.setTextSize(1); tft.setTextColor(sel ? kbTxtSel : kbTxt);
                tft.drawCentreString(c, x + w/2, y1 + r * 30 + 7, 1);
            }
        }
    } else if (keyboardModeType == 1) {
        const char* rows[3] = {"1234567890", "-/:;()$&@\"", ".,?!'#%"};
        int startX[3] = {7, 7, 25};
        for (int r = 0; r < 3; r++) {
            int len = strlen(rows[r]);
            for (int i = 0; i < len; i++) {
                int x = startX[r] + i * (w + g);
                bool sel = (kbSelectedRow == r && kbSelectedCol == i);
                char c[2] = {rows[r][i], 0};
                tft.fillRoundRect(x, y1 + r * 30, w, h, 2, sel ? kbSel : kbBg);
                tft.setTextSize(1); tft.setTextColor(sel ? kbTxtSel : kbTxt);
                tft.drawCentreString(c, x + w/2, y1 + r * 30 + 7, 1);
            }
        }
    } else {
        const char* rows[3] = {"]\\[{}|", "*+=_()", "0123456"};
        int startX[3] = {15, 15, 15};
        for (int r = 0; r < 3; r++) {
            int len = strlen(rows[r]);
            for (int i = 0; i < len; i++) {
                int x = startX[r] + i * (w + g);
                bool sel = (kbSelectedRow == r && kbSelectedCol == i);
                char c[2] = {rows[r][i], 0};
                tft.fillRoundRect(x, y1 + r * 30, w, h, 2, sel ? kbSel : kbBg);
                tft.setTextSize(1); tft.setTextColor(sel ? kbTxtSel : kbTxt);
                tft.drawCentreString(c, x + w/2, y1 + r * 30 + 7, 1);
            }
        }
    }
    
    bool selShift = (kbSelectedRow == 3 && kbSelectedCol == 0);
    tft.fillRoundRect(5, y4, 30, h, 2, (selShift || kbShiftHeld) ? FGCOLOR : kbBg);
    tft.setTextSize(1); tft.setTextColor((selShift || kbShiftHeld) ? BGCOLOR : kbTxt);
    tft.drawCentreString("S", 20, y4 + 7, 1);
    
    bool selDel = (kbSelectedRow == 3 && kbSelectedCol == 1);
    tft.fillRoundRect(38, y4, 30, h, 2, selDel ? kbSel : ERRCOLOR);
    tft.setTextColor(selDel ? BGCOLOR : kbTxt);
    tft.drawCentreString("DEL", 53, y4 + 7, 1);
    
    bool sel123 = (kbSelectedRow == 3 && kbSelectedCol == 2);
    tft.fillRoundRect(71, y4, 30, h, 2, sel123 ? kbSel : kbBg);
    tft.setTextColor(sel123 ? BGCOLOR : kbTxt);
    tft.drawCentreString("123", 86, y4 + 7, 1);
    
    bool selABC = (kbSelectedRow == 3 && kbSelectedCol == 3);
    tft.fillRoundRect(104, y4, 30, h, 2, selABC ? kbSel : (kbUpperCase ? FGCOLOR : kbBg));
    tft.setTextColor(selABC ? BGCOLOR : kbTxt);
    tft.drawCentreString("ABC", 119, y4 + 7, 1);
    
    bool selSpace = (kbSelectedRow == 3 && kbSelectedCol == 4);
    tft.fillRoundRect(137, y4, 55, h, 2, selSpace ? kbSel : kbBg);
    tft.setTextColor(selSpace ? BGCOLOR : kbTxt);
    tft.drawCentreString("SPACE", 164, y4 + 7, 1);
    
    bool selEnt = (kbSelectedRow == 3 && kbSelectedCol == 5);
    tft.fillRoundRect(195, y4, 40, h, 2, selEnt ? kbSel : FGCOLOR);
    tft.setTextColor(selEnt ? BGCOLOR : BGCOLOR);
    tft.drawCentreString("ENT", 215, y4 + 7, 1);
    
    tft.setTextColor(0x8410);
    tft.setTextSize(1);
    String kbInfo = "SHIFT:" + String(kbShiftHeld ? "ON" : "OFF");
    kbInfo += " | MODE:" + String(keyboardModeType == 0 ? "ABC" : (keyboardModeType == 1 ? "123" : "SYM"));
    tft.drawCentreString(kbInfo, 120, 230, 1);
}

void drawGamepadMode() {
    screenState = 15;
    tft.fillScreen(BGCOLOR);
    drawHeader("GAMEPAD MODE");
    
    tft.fillRoundRect(5, 35, 110, 30, 5, gamepadModeType == 0 ? FGCOLOR : BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(gamepadModeType == 0 ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString("ARROWS", 60, 45, 1);
    
    tft.fillRoundRect(125, 35, 110, 30, 5, gamepadModeType == 1 ? FGCOLOR : BUTTON_BG);
    tft.setTextColor(gamepadModeType == 1 ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString("WASD", 180, 45, 1);
    
    int cx = 80, cy = 140;
    tft.fillCircle(cx, cy, 35, 0x0841);
    tft.fillCircle(cx - 20, cy - 20, 12, currentHover == 0 ? SELECTED : ERRCOLOR);
    tft.fillCircle(cx, cy - 25, 12, currentHover == 1 ? SELECTED : FGCOLOR);
    tft.fillCircle(cx + 20, cy - 20, 12, currentHover == 2 ? SELECTED : FGCOLOR);
    tft.fillCircle(cx - 25, cy, 12, currentHover == 3 ? SELECTED : FGCOLOR);
    tft.fillCircle(cx + 25, cy, 12, currentHover == 4 ? SELECTED : FGCOLOR);
    tft.fillCircle(cx, cy + 10, 15, currentHover == 5 ? SELECTED : SELECTED);
    
    int cx2 = 180, cy2 = 140;
    tft.fillCircle(cx2, cy2, 35, 0x0841);
    tft.fillRoundRect(cx2 - 20, cy2 - 25, 16, 16, 2, gamepadBtn[0] ? SELECTED : ERRCOLOR);
    tft.fillRoundRect(cx2, cy2 - 25, 16, 16, 2, gamepadBtn[1] ? SELECTED : FGCOLOR);
    tft.fillRoundRect(cx2 - 20, cy2, 16, 16, 2, gamepadBtn[2] ? SELECTED : FGCOLOR);
    tft.fillRoundRect(cx2, cy2, 16, 16, 2, gamepadBtn[3] ? SELECTED : FGCOLOR);
    
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("LEFT STICK: MOVE", 10, 220, 1);
    tft.drawString("A/B/X/Y: BUTTONS", 130, 220, 1);
}

void drawKeyboard() {
    screenState = 5;
    tft.fillScreen(BGCOLOR);
    drawHeader("KEYBOARD");
    tft.fillRoundRect(10, 40, 220, 100, 8, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(TXTCOLOR);
    tft.drawCentreString("BLE Keyboard Mode", 120, 55, 1);
    tft.drawCentreString("Commands: TEXT:<msg>", 120, 70, 1);
    tft.drawCentreString("KEY:<char> - Type", 120, 85, 1);
    tft.drawCentreString("MOUSE_ON/OFF - Enable", 120, 100, 1);
    tft.setTextColor(0x8410);
    tft.drawCentreString("BACK: Menu", 120, 228, 1);
}

const char kbRow1_qwerty[] = "qwertyuiop";
const char kbRow2_qwerty[] = "asdfghjkl";
const char kbRow3_qwerty[] = "zxcvbnm";
const char kbRow1_num[] = "1234567890";
const char kbRow2_num[] = "!@#$%^&*()";
const char kbRow3_num[] = "-=_+[]{}\\|";
const char kbRow4_num[] = ":;\"'<>,.?";

void drawKeyboardUI() {
    tft.fillScreen(BGCOLOR);
    if (kbPage == 0) {
        drawHeader(kbUpperCase ? "KEYBOARD - ABC" : "KEYBOARD - abc");
    } else if (kbPage == 1) {
        drawHeader("KEYBOARD - 123");
    } else {
        drawHeader("KEYBOARD - SYM");
    }
    
    tft.fillRoundRect(5, 32, 230, 28, 4, BUTTON_BG);
    tft.setTextSize(2); tft.setTextColor(TXTCOLOR);
    tft.drawString(tempName, 10, 38, 1);
    int cursorX = 10 + strlen(tempName) * 12;
    tft.fillRect(cursorX, 38, 2, 20, FGCOLOR);
    
    int y1 = 68, y2 = 96, y3 = 124, y4 = 152;
    int w = 20, h = 22, g = 2;
    
    if (kbPage == 0) {
        for (int i = 0; i < 10; i++) {
            int x = 8 + i * (w + g);
            bool sel = (kbSelectedRow == 0 && kbSelectedCol == i);
            char c[2] = {kbUpperCase ? toupper(kbRow1_qwerty[i]) : kbRow1_qwerty[i], 0};
            tft.fillRoundRect(x, y1, w, h, 2, sel ? SELECTED : BUTTON_BG);
            tft.setTextSize(1); tft.setTextColor(sel ? BGCOLOR : TXTCOLOR);
            tft.drawCentreString(c, x + w/2, y1 + 5, 1);
        }
        for (int i = 0; i < 9; i++) {
            int x = 18 + i * (w + g);
            bool sel = (kbSelectedRow == 1 && kbSelectedCol == i);
            char c[2] = {kbUpperCase ? toupper(kbRow2_qwerty[i]) : kbRow2_qwerty[i], 0};
            tft.fillRoundRect(x, y2, w, h, 2, sel ? SELECTED : BUTTON_BG);
            tft.setTextSize(1); tft.setTextColor(sel ? BGCOLOR : TXTCOLOR);
            tft.drawCentreString(c, x + w/2, y2 + 5, 1);
        }
        for (int i = 0; i < 7; i++) {
            int x = 28 + i * (w + g);
            bool sel = (kbSelectedRow == 2 && kbSelectedCol == i);
            char c[2] = {kbUpperCase ? toupper(kbRow3_qwerty[i]) : kbRow3_qwerty[i], 0};
            tft.fillRoundRect(x, y3, w, h, 2, sel ? SELECTED : BUTTON_BG);
            tft.setTextSize(1); tft.setTextColor(sel ? BGCOLOR : TXTCOLOR);
            tft.drawCentreString(c, x + w/2, y3 + 5, 1);
        }
    } else if (kbPage == 1) {
        for (int i = 0; i < 10; i++) {
            int x = 8 + i * (w + g);
            bool sel = (kbSelectedRow == 0 && kbSelectedCol == i);
            char c[2] = {kbRow1_num[i], 0};
            tft.fillRoundRect(x, y1, w, h, 2, sel ? SELECTED : BUTTON_BG);
            tft.setTextSize(1); tft.setTextColor(sel ? BGCOLOR : TXTCOLOR);
            tft.drawCentreString(c, x + w/2, y1 + 5, 1);
        }
        for (int i = 0; i < 10; i++) {
            int x = 8 + i * (w + g);
            bool sel = (kbSelectedRow == 1 && kbSelectedCol == i);
            char c[2] = {kbRow2_num[i], 0};
            tft.fillRoundRect(x, y2, w, h, 2, sel ? SELECTED : BUTTON_BG);
            tft.setTextSize(1); tft.setTextColor(sel ? BGCOLOR : TXTCOLOR);
            tft.drawCentreString(c, x + w/2, y2 + 5, 1);
        }
        for (int i = 0; i < 9; i++) {
            int x = 18 + i * (w + g);
            bool sel = (kbSelectedRow == 2 && kbSelectedCol == i);
            char c[2] = {kbRow3_num[i], 0};
            tft.fillRoundRect(x, y3, w, h, 2, sel ? SELECTED : BUTTON_BG);
            tft.setTextSize(1); tft.setTextColor(sel ? BGCOLOR : TXTCOLOR);
            tft.drawCentreString(c, x + w/2, y3 + 5, 1);
        }
    } else {
        for (int i = 0; i < 9; i++) {
            int x = 18 + i * (w + g);
            bool sel = (kbSelectedRow == 0 && kbSelectedCol == i);
            char c[2] = {kbRow4_num[i], 0};
            tft.fillRoundRect(x, y1, w, h, 2, sel ? SELECTED : BUTTON_BG);
            tft.setTextSize(1); tft.setTextColor(sel ? BGCOLOR : TXTCOLOR);
            tft.drawCentreString(c, x + w/2, y1 + 5, 1);
        }
        tft.setTextSize(1); tft.setTextColor(0x8410);
        tft.drawString("/`~", 18, y2 + 5, 1);
    }
    
    bool selDel = (kbSelectedRow == 3 && kbSelectedCol == 0);
    tft.fillRoundRect(5, y4, 35, h, 2, selDel ? SELECTED : ERRCOLOR);
    tft.setTextSize(1); tft.setTextColor(selDel ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString("DEL", 22, y4 + 5, 1);
    
    bool sel123 = (kbSelectedRow == 3 && kbSelectedCol == 1);
    tft.fillRoundRect(43, y4, 35, h, 2, sel123 ? SELECTED : BUTTON_BG);
    tft.setTextColor(sel123 ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString("123", 60, y4 + 5, 1);
    
    bool selABC = (kbSelectedRow == 3 && kbSelectedCol == 2);
    tft.fillRoundRect(81, y4, 35, h, 2, selABC ? SELECTED : (kbUpperCase ? FGCOLOR : BUTTON_BG));
    tft.setTextColor(selABC ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString("ABC", 98, y4 + 5, 1);
    
    bool selSYM = (kbSelectedRow == 3 && kbSelectedCol == 3);
    tft.fillRoundRect(119, y4, 35, h, 2, selSYM ? SELECTED : BUTTON_BG);
    tft.setTextColor(selSYM ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString("SYM", 136, y4 + 5, 1);
    
    bool selSpace = (kbSelectedRow == 3 && kbSelectedCol == 4);
    tft.fillRoundRect(157, y4, 45, h, 2, selSpace ? SELECTED : BUTTON_BG);
    tft.setTextColor(selSpace ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString("SPACE", 179, y4 + 5, 1);
    
    bool selOK = (kbSelectedRow == 3 && kbSelectedCol == 5);
    tft.fillRoundRect(205, y4, 32, h, 2, selOK ? SELECTED : FGCOLOR);
    tft.setTextColor(selOK ? BGCOLOR : BGCOLOR);
    tft.drawCentreString("OK", 221, y4 + 5, 1);
    
    tft.setTextColor(0x8410);
    tft.setTextSize(1);
    tft.drawCentreString("DEL|123|ABC|SYM|SPACE|OK", 120, 230, 1);
}

void startBLEScan() {
    foundDevices = 0;
    scanning = true;
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(3, true);
    scanning = false;
}

void drawDeviceScan() {
    screenState = 7;
    tft.fillScreen(BGCOLOR);
    drawHeader("BLE SCAN");
    
    if (scanning) {
        tft.setTextSize(2); tft.setTextColor(FGCOLOR);
        tft.drawCentreString("Scanning...", 120, 100, 1);
    } else {
        tft.fillRect(0, 28, 240, 2, FGCOLOR);
        
        tft.setTextSize(1); tft.setTextColor(0x8410);
        tft.drawString("Found:", 10, 33);
        tft.setTextColor(FGCOLOR);
        tft.drawString(String(foundDevices), 60, 33, 1);
        
        if (foundDevices > 0) {
            int maxShow = 5;
            if (selectedDevice >= scanOffset + maxShow) scanOffset = selectedDevice - maxShow + 1;
            if (selectedDevice < scanOffset) scanOffset = selectedDevice;
            
            int visibleStart = scanOffset;
            int visibleEnd = min(scanOffset + maxShow, foundDevices);
            
            for (int i = visibleStart; i < visibleEnd; i++) {
                int y = 40 + (i - scanOffset) * 35;
                uint16_t bg = (i == selectedDevice) ? SELECTED : BUTTON_BG;
                tft.fillRoundRect(5, y, 230, 32, 4, bg);
                tft.setTextSize(1);
                tft.setTextColor(i == selectedDevice ? BGCOLOR : TXTCOLOR);
                String disp = devNames[i].substring(0, 14);
                tft.drawString(disp, 10, y + 4, 1);
                tft.setTextColor(i == selectedDevice ? BGCOLOR : (devRSSI[i] > -60 ? FGCOLOR : ERRCOLOR));
                tft.drawString(String(devRSSI[i]) + "dBm", 150, y + 4, 1);
                tft.setTextSize(1);
                tft.setTextColor(i == selectedDevice ? BGCOLOR : 0x8410);
                tft.drawString(devAddrs[i].substring(0, 17), 10, y + 18, 1);
            }
            
            if (foundDevices > maxShow) {
                int barHeight = 160;
                int barPos = map(scanOffset, 0, foundDevices - maxShow, 0, barHeight);
                tft.fillRoundRect(230, 40, 6, barHeight, 2, 0x2104);
                tft.fillRoundRect(230, 40 + barPos, 6, 10, 2, FGCOLOR);
            }
        }
    }
    
    tft.setTextColor(0x8410);
    tft.drawCentreString("UP/DOWN: Select | OK: Connect | A: Scan", 120, 225, 1);
}

void drawSettings() {
    screenState = 6;
    tft.fillScreen(BGCOLOR);
    drawHeader("SETTINGS");
    if (editingName) { drawKeyboardUI(); return; }
    
    const char* labels[] = {"Remote Mode", "Mouse Mode", "Keyboard", "Gamepad", "Click Sound", "Move Speed", "Mouse Speed", "Backlight", "Device Name"};
    for (int i = 0; i < 9; i++) {
        int y = 32 + i * 22;
        uint16_t bgColor = (i == settingIndex) ? SELECTED : BUTTON_BG;
        tft.fillRoundRect(5, y, 230, 20, 4, bgColor);
        tft.setTextSize(1);
        tft.setTextColor(i == settingIndex ? BGCOLOR : TXTCOLOR);
        tft.drawString(labels[i], 15, y + 4, 1);
        
        if (i < 8) {
            uint16_t valColor = FGCOLOR;
            if (i == 0 && !remoteModeActive) valColor = ERRCOLOR;
            if (i == 1 && !airMouseMode) valColor = ERRCOLOR;
            if (i == 2 && !keyboardMode) valColor = ERRCOLOR;
            if (i == 3 && !gamepadMode) valColor = ERRCOLOR;
            if (i == 4 && !clickSound) valColor = 0x8410;
            if (i == 7 && !digitalRead(TFT_BL)) valColor = ERRCOLOR;
            tft.setTextColor(i == settingIndex ? BGCOLOR : valColor);
            if (i == 0) tft.drawString(remoteModeActive ? "ON" : "OFF", 185, y + 4, 1);
            else if (i == 1) tft.drawString(airMouseMode ? "ON" : "OFF", 185, y + 4, 1);
            else if (i == 2) tft.drawString(keyboardMode ? "ON" : "OFF", 185, y + 4, 1);
            else if (i == 3) tft.drawString(gamepadMode ? "ON" : "OFF", 185, y + 4, 1);
            else if (i == 4) tft.drawString(clickSound ? "ON" : "OFF", 185, y + 4, 1);
            else if (i == 5) tft.drawString(continuousMove ? "FAST" : "STEP", 185, y + 4, 1);
            else if (i == 6) tft.drawString(String(mouseSpeed), 185, y + 4, 1);
            else if (i == 7) tft.drawString(digitalRead(TFT_BL) ? "ON" : "OFF", 185, y + 4, 1);
        } else {
            tft.setTextColor(i == settingIndex ? BGCOLOR : 0xF8B8);
            tft.drawString(deviceName, 140, y + 4, 1);
        }
    }
    tft.setTextColor(0x8410);
    tft.drawCentreString("OK: Toggle | UP/DOWN: Navigate", 120, 228, 1);
}

void drawClientMenu() {
    screenState = 8;
    tft.fillScreen(BGCOLOR);
    drawHeader("BLE CLIENT");
    
    tft.fillRoundRect(5, 35, 110, 35, 5, clientConnected ? 0x10A2 : ERRCOLOR);
    tft.setTextSize(1); tft.setTextColor(clientConnected ? FGCOLOR : ERRCOLOR);
    tft.drawCentreString("STATUS", 60, 40, 1);
    tft.setTextSize(2);
    tft.drawCentreString(clientConnected ? "CONNECTED" : "DISCONNECTED", 60, 52, 1);
    
    if (clientConnected) {
        tft.fillRoundRect(125, 35, 110, 35, 5, BUTTON_BG);
        tft.setTextSize(1); tft.setTextColor(0x8410);
        tft.drawCentreString("DEVICE", 180, 40, 1);
        tft.setTextSize(1); tft.setTextColor(TXTCOLOR);
        tft.drawCentreString(clientConnectedName.substring(0, 12), 180, 52, 1);
    }
    
    tft.fillRoundRect(5, 78, 230, 60, 5, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("Receive Buffer:", 10, 82, 1);
    tft.setTextSize(1); tft.setTextColor(TXTCOLOR);
    String rxData = clientAvailable() > 0 ? clientRead() : "(empty)";
    if (rxData.length() > 30) rxData = rxData.substring(0, 30) + "...";
    tft.drawString(rxData, 10, 95, 1);
    tft.setTextColor(FGCOLOR);
    tft.drawString("Bytes: " + String(clientAvailable()), 10, 110, 1);
    
    tft.fillRoundRect(5, 145, 72, 25, 4, SELECTED);
    tft.setTextSize(1); tft.setTextColor(BGCOLOR);
    tft.drawCentreString("SCAN", 41, 152, 1);
    
    tft.fillRoundRect(84, 145, 72, 25, 4, clientConnected ? ERRCOLOR : BUTTON_BG);
    tft.setTextColor(clientConnected ? BGCOLOR : TXTCOLOR);
    tft.drawCentreString("DISCONNECT", 120, 152, 1);
    
    tft.fillRoundRect(163, 145, 72, 25, 4, BUTTON_BG);
    tft.setTextColor(TXTCOLOR);
    tft.drawCentreString("SEND", 199, 152, 1);
    
    tft.setTextColor(0x8410);
    tft.drawCentreString("A: Scan by UUID | B: Back", 120, 225, 1);
}

void drawClientScan() {
    screenState = 9;
    tft.fillScreen(BGCOLOR);
    drawHeader("CLIENT SCAN");
    
    if (clientScanning) {
        tft.setTextSize(2); tft.setTextColor(FGCOLOR);
        tft.drawCentreString("Scanning...", 120, 100, 1);
    } else {
        tft.fillRect(0, 28, 240, 2, FGCOLOR);
        
        tft.setTextSize(1); tft.setTextColor(0x8410);
        tft.drawString("Found:", 10, 33);
        tft.setTextColor(FGCOLOR);
        tft.drawString(String(clientFoundDevices), 60, 33, 1);
        
        if (clientFilterByUUID) {
            tft.setTextColor(0xF8B8);
            tft.drawString("UUID:", 100, 33, 1);
            tft.drawString(clientTargetUUID.substring(0, 12), 135, 33, 1);
        }
        
        if (clientFoundDevices > 0) {
            int maxShow = 5;
            if (clientSelectedDevice >= clientScanOffset + maxShow) clientScanOffset = clientSelectedDevice - maxShow + 1;
            if (clientSelectedDevice < clientScanOffset) clientScanOffset = clientSelectedDevice;
            
            int visibleStart = clientScanOffset;
            int visibleEnd = min(clientScanOffset + maxShow, clientFoundDevices);
            
            for (int i = visibleStart; i < visibleEnd; i++) {
                int y = 40 + (i - clientScanOffset) * 35;
                uint16_t bg = (i == clientSelectedDevice) ? SELECTED : BUTTON_BG;
                tft.fillRoundRect(5, y, 230, 32, 4, bg);
                tft.setTextSize(1);
                tft.setTextColor(i == clientSelectedDevice ? BGCOLOR : TXTCOLOR);
                String disp = clientDevNames[i].substring(0, 14);
                tft.drawString(disp, 10, y + 4, 1);
                tft.setTextColor(i == clientSelectedDevice ? BGCOLOR : (clientDevRSSI[i] > -60 ? FGCOLOR : ERRCOLOR));
                tft.drawString(String(clientDevRSSI[i]) + "dBm", 150, y + 4, 1);
                tft.setTextSize(1);
                tft.setTextColor(i == clientSelectedDevice ? BGCOLOR : 0x8410);
                tft.drawString(clientDevAddrs[i].substring(0, 17), 10, y + 18, 1);
            }
            
            if (clientFoundDevices > maxShow) {
                int barHeight = 160;
                int barPos = map(clientScanOffset, 0, clientFoundDevices - maxShow, 0, barHeight);
                tft.fillRoundRect(230, 40, 6, barHeight, 2, 0x2104);
                tft.fillRoundRect(230, 40 + barPos, 6, 10, 2, FGCOLOR);
            }
        }
    }
    
    tft.setTextColor(0x8410);
    tft.drawCentreString("UP/DOWN: Select | OK: Connect | A: Scan | B: Back", 120, 225, 1);
}

void drawClientConnect() {
    screenState = 10;
    tft.fillScreen(BGCOLOR);
    drawHeader("CONNECTING");
    
    tft.setTextSize(2); tft.setTextColor(FGCOLOR);
    tft.drawCentreString("Connecting...", 120, 60, 1);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawCentreString(clientDevNames[clientSelectedDevice], 120, 90, 1);
    tft.drawCentreString(clientDevAddrs[clientSelectedDevice], 120, 105, 1);
    
    if (clientDevUUIDs[clientSelectedDevice].length() > 0) {
        tft.drawCentreString("UUID:", 120, 125, 1);
        tft.drawCentreString(clientDevUUIDs[clientSelectedDevice].substring(0, 24), 120, 140, 1);
    }
    
    tft.drawCentreString("Please wait...", 120, 170, 1);
}

void drawClientSend() {
    screenState = 11;
    tft.fillScreen(BGCOLOR);
    drawHeader("CLIENT SEND");
    
    tft.fillRoundRect(5, 35, 230, 30, 4, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("Connected to:", 10, 40, 1);
    tft.setTextColor(TXTCOLOR);
    tft.drawString(clientConnectedName, 90, 40, 1);
    
    tft.fillRoundRect(5, 70, 230, 80, 4, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(TXTCOLOR);
    tft.drawString("Send data or commands:", 10, 75, 1);
    tft.setTextColor(0x8410);
    tft.drawString("UP/DOWN: Navigate", 10, 100, 1);
    tft.drawString("OK: Send selected", 10, 115, 1);
    
    const char* quickCmds[] = {"HELLO", "STATUS", "PING", "GET_DATA", "RESET"};
    for (int i = 0; i < 5; i++) {
        int x = 5 + (i % 3) * 78;
        int y = 160 + (i / 3) * 25;
        uint16_t bg = (i == 0) ? SELECTED : BUTTON_BG;
        tft.fillRoundRect(x, y, 73, 22, 3, bg);
        tft.setTextSize(1); tft.setTextColor(i == 0 ? BGCOLOR : TXTCOLOR);
        tft.drawCentreString(quickCmds[i], x + 37, y + 5, 1);
    }
    
    tft.setTextColor(0x8410);
    tft.drawCentreString("B: Back to Menu", 120, 225, 1);
}

void drawServerMenu() {
    screenState = 12;
    tft.fillScreen(BGCOLOR);
    drawHeader("BLE SERVER");
    
    tft.fillRoundRect(5, 35, 110, 35, 5, deviceConnected ? 0x10A2 : ERRCOLOR);
    tft.setTextSize(1); tft.setTextColor(deviceConnected ? FGCOLOR : ERRCOLOR);
    tft.drawCentreString("STATUS", 60, 40, 1);
    tft.setTextSize(2);
    tft.drawCentreString(deviceConnected ? "CONNECTED" : "LISTENING", 60, 52, 1);
    
    tft.fillRoundRect(125, 35, 110, 35, 5, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawCentreString("MODE", 180, 40, 1);
    tft.setTextSize(2); tft.setTextColor(serverAccepting ? FGCOLOR : ERRCOLOR);
    tft.drawCentreString(serverAccepting ? "ACCEPT" : "IDLE", 180, 52, 1);
    
    tft.fillRoundRect(5, 78, 230, 45, 5, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("Service UUID:", 10, 82, 1);
    tft.setTextColor(TXTCOLOR);
    tft.drawString(SERVICE_UUID, 10, 95, 1);
    tft.drawString("HID: 1812 | BAT: 180F | UART: 6e40...", 10, 108, 1);
    
    tft.fillRoundRect(5, 130, 110, 30, 4, serverAccepting ? ERRCOLOR : FGCOLOR);
    tft.setTextSize(1); tft.setTextColor(serverAccepting ? BGCOLOR : BGCOLOR);
    tft.drawCentreString(serverAccepting ? "STOP" : "ACCEPT", 60, 138, 1);
    
    tft.fillRoundRect(125, 130, 110, 30, 4, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(TXTCOLOR);
    tft.drawCentreString("ACCEPT+UUID", 180, 138, 1);
    
    tft.fillRoundRect(5, 168, 230, 30, 4, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("Quick UUIDs:", 10, 173, 1);
    tft.setTextColor(TXTCOLOR);
    tft.drawString("HID: 1812 | UART: 6e400001-...", 10, 185, 1);
    
    tft.setTextColor(0x8410);
    tft.drawCentreString("OK: Toggle Accept | A: Accept+HID | B: Back", 120, 225, 1);
}

int serverUuidIndex = 0;
const char* serverUuids[] = {
    "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
    "00001812-0000-1000-8000-00805f9b34fb",
    "0000180f-0000-1000-8000-00805f9b34fb"
};
const char* serverUuidNames[] = {"UART", "HID", "BATTERY"};

void drawServerUUIDSelect() {
    screenState = 13;
    tft.fillScreen(BGCOLOR);
    drawHeader("SELECT UUID");
    
    tft.fillRoundRect(5, 35, 230, 40, 5, BUTTON_BG);
    tft.setTextSize(1); tft.setTextColor(0x8410);
    tft.drawString("Select service UUID:", 10, 40, 1);
    tft.setTextColor(FGCOLOR);
    tft.drawString(serverUuidNames[serverUuidIndex], 10, 52, 1);
    
    tft.fillRect(0, 80, 240, 2, FGCOLOR);
    
    for (int i = 0; i < 3; i++) {
        int y = 90 + i * 35;
        uint16_t bg = (i == serverUuidIndex) ? SELECTED : BUTTON_BG;
        tft.fillRoundRect(5, y, 230, 32, 4, bg);
        tft.setTextSize(1);
        tft.setTextColor(i == serverUuidIndex ? BGCOLOR : TXTCOLOR);
        tft.drawString(serverUuidNames[i], 10, y + 4, 1);
        tft.setTextColor(i == serverUuidIndex ? BGCOLOR : 0x8410);
        tft.drawString(serverUuids[i], 10, y + 18, 1);
    }
    
    tft.fillRoundRect(5, 205, 110, 25, 4, FGCOLOR);
    tft.setTextSize(1); tft.setTextColor(BGCOLOR);
    tft.drawCentreString("CONNECT", 60, 210, 1);
    
    tft.fillRoundRect(125, 205, 110, 25, 4, BUTTON_BG);
    tft.setTextColor(TXTCOLOR);
    tft.drawCentreString("CANCEL", 180, 210, 1);
    
    tft.setTextColor(0x8410);
    tft.drawCentreString("UP/DOWN: Select | OK: Connect", 120, 235, 1);
}

void handleServerButtonPress() {
    static int lastUp = HIGH, lastDown = HIGH, lastLeft = HIGH, lastRight = HIGH, lastOK = HIGH, lastBack = HIGH;
    
    int up = digitalRead(KEY_UP);
    int down = digitalRead(KEY_DOWN);
    int left = digitalRead(KEY_LEFT);
    int right = digitalRead(KEY_RIGHT);
    int ok = digitalRead(KEY_START);   // OK giua (Symbian map moi)
    int back = digitalRead(KEY_A);     // Back (Symbian map moi)
    
    if (screenState == 12) {
        if (ok == LOW && lastOK == HIGH) {
            playBeep(1200, 20);
            if (serverAccepting) {
                serverStopAccepting();
            } else {
                serverAcceptConnection();
            }
            drawServerMenu();
        }
        
        if (back == LOW && lastBack == HIGH) {
            playBeep(600, 20);
            currentMenu = 0;
            drawMainMenu();
        }
        
        if (left == LOW && lastLeft == HIGH) {
            playBeep(800, 15);
            if (serverAccepting) {
                serverStopAccepting();
            } else {
                serverAcceptConnectionWithUUID("00001812-0000-1000-8000-00805f9b34fb");
            }
            drawServerMenu();
        }
        
        if (right == LOW && lastRight == HIGH) {
            playBeep(800, 15);
            drawServerUUIDSelect();
        }
        
        if (up == LOW && lastUp == HIGH) {
            playBeep(800, 15);
        }
        
        if (down == LOW && lastDown == HIGH) {
            playBeep(800, 15);
        }
    }
    
    if (screenState == 13) {
        if (ok == LOW && lastOK == HIGH) {
            playBeep(1200, 20);
            serverAcceptConnectionWithUUID(serverUuids[serverUuidIndex]);
            drawServerMenu();
        }
        
        if (back == LOW && lastBack == HIGH) {
            playBeep(600, 20);
            drawServerMenu();
        }
        
        if (up == LOW && lastUp == HIGH) {
            playBeep(800, 15);
            serverUuidIndex = (serverUuidIndex > 0) ? serverUuidIndex - 1 : 2;
            drawServerUUIDSelect();
        }
        
        if (down == LOW && lastDown == HIGH) {
            playBeep(800, 15);
            serverUuidIndex = (serverUuidIndex < 2) ? serverUuidIndex + 1 : 0;
            drawServerUUIDSelect();
        }
    }
    
    lastUp = up; lastDown = down; lastLeft = left; lastRight = right; lastOK = ok; lastBack = back;
}

void handleClientButtonPress() {
    static int lastUp = HIGH, lastDown = HIGH, lastLeft = HIGH, lastRight = HIGH;
    static int lastOK = HIGH, lastBack = HIGH;
    
    int up = digitalRead(KEY_UP);
    int down = digitalRead(KEY_DOWN);
    int left = digitalRead(KEY_LEFT);
    int right = digitalRead(KEY_RIGHT);
    int ok = digitalRead(KEY_START);   // OK giua (Symbian map moi)
    int back = digitalRead(KEY_A);     // Back (Symbian map moi)
    
    if (ok == LOW && lastOK == HIGH) {
        playBeep(1200, 20);
        if (screenState == 8) {
            if (clientConnected) {
                drawClientSend();
            }
        } else if (screenState == 9) {
            if (clientFoundDevices > 0) {
                drawClientConnect();
                delay(100);
                String targetUUID = clientDevUUIDs[clientSelectedDevice];
                if (clientConnectByAddress(clientDevAddrs[clientSelectedDevice], targetUUID)) {
                    clientConnectedName = clientDevNames[clientSelectedDevice];
                    drawClientMenu();
                } else {
                    tft.fillScreen(BGCOLOR);
                    drawHeader("ERROR");
                    tft.setTextSize(2); tft.setTextColor(ERRCOLOR);
                    tft.drawCentreString("Connection Failed", 120, 100, 1);
                    delay(2000);
                    drawClientScan();
                }
            }
        }
    }
    
    if (back == LOW && lastBack == HIGH) {
        playBeep(600, 20);
        if (screenState == 9 || screenState == 10 || screenState == 11) {
            currentMenu = 0;
            drawMainMenu();
        } else if (screenState == 8) {
            currentMenu = 0;
            drawMainMenu();
        }
    }
    
    if (ok == LOW && lastOK == HIGH) {
        if (screenState == 8) {
            if (!clientConnected) {
                startClientScan();
                drawClientScan();
            }
        } else if (screenState == 9) {
            if (clientScanning) return;
            startClientScan();
            clientSelectedDevice = 0;
            clientScanOffset = 0;
            drawClientScan();
        }
    }
    
    if (back == LOW && lastBack == HIGH) {
        if (screenState == 8) {
            if (clientConnected) {
                clientDisconnect();
                drawClientMenu();
            }
        } else if (screenState == 9) {
            startClientScan();
            clientSelectedDevice = 0;
            clientScanOffset = 0;
            drawClientScan();
        }
    }
    
    if (screenState == 9 && !clientScanning && clientFoundDevices > 0) {
        if (up == LOW && lastUp == HIGH) { playBeep(800, 15); clientSelectedDevice = (clientSelectedDevice > 0) ? clientSelectedDevice - 1 : clientFoundDevices - 1; drawClientScan(); }
        if (down == LOW && lastDown == HIGH) { playBeep(800, 15); clientSelectedDevice = (clientSelectedDevice < clientFoundDevices - 1) ? clientSelectedDevice + 1 : 0; drawClientScan(); }
    }
    
    lastUp = up; lastDown = down; lastLeft = left; lastRight = right; lastOK = ok; lastBack = back;
}

void handleButtonPress() {
    static int lastUp = HIGH, lastDown = HIGH, lastLeft = HIGH, lastRight = HIGH;
    static int lastOK = HIGH, lastBack = HIGH;
    static int lastKeyMenu = HIGH;
    static unsigned long lastMoveTime = 0;
    static bool rightBtnDown = false, okBtnDown = false;
    static unsigned long lastTikTokTime = 0;
    
    int up = digitalRead(KEY_UP);
    int down = digitalRead(KEY_DOWN);
    int left = digitalRead(KEY_LEFT);
    int right = digitalRead(KEY_RIGHT);
    int ok = digitalRead(KEY_START);   // OK giua (Symbian map moi)
    int back = digitalRead(KEY_A);     // Back (Symbian map moi)
    
    int keySelect = digitalRead(KEY_SELECT);
    int keyStart = digitalRead(KEY_START);
    int keyMenu = digitalRead(KEY_MENU);
    int keyOption = digitalRead(KEY_OPTION);
    
    bool btnSelect = (keySelect == LOW);
    bool btnStart = (keyStart == LOW);
    bool btnMenu = (keyMenu == LOW);
    bool btnOption = (keyOption == LOW);
    
    currentHover = -1;
    if (screenState == 2) {
        if (back == LOW) remoteButtonId = 0;
        else if (ok == LOW) remoteButtonId = 2;
        else if (btnMenu) remoteButtonId = 1;
        else if (btnOption) remoteButtonId = 3;
        else if (btnSelect) remoteButtonId = 4;
        else if (btnStart) remoteButtonId = 5;
        else if (up == LOW) remoteButtonId = 6;
        else if (down == LOW) remoteButtonId = 7;
        else if (left == LOW) remoteButtonId = 8;
        else if (right == LOW) remoteButtonId = 9;
        else remoteButtonId = -1;
        
        if (remoteButtonId != -1) {
            spawnParticles(120, 130, 0x00BCD4, 5);
            remoteCounter++;
            if (remoteCounter > 99) remoteCounter = 42;
        }
        
        if (remoteModeActive || airMouseMode) {
            if (up == LOW) currentHover = 0;
            else if (down == LOW) currentHover = 1;
            else if (left == LOW) currentHover = 2;
            else if (right == LOW) currentHover = 3;
            else if (ok == LOW) currentHover = 7;
        }
    }
    if (screenState == 4) {
        if (up == LOW) currentHover = 0;
        else if (down == LOW) currentHover = 1;
        else if (left == LOW) currentHover = 2;
        else if (right == LOW) currentHover = 3;
    }
    
    if (back == LOW && ok == LOW && bLongPress == 0) {
        bLongPress = millis();
    } else if (bLongPress > 0 && (back == HIGH || ok == HIGH)) {
        if (millis() - bLongPress >= 1500 && deviceConnected) {
            sendMediaKey(0x40);
            playBeep(2000, 50);
        }
        bLongPress = 0;
    }
    
    static bool lastBackState = HIGH;
    if (back == LOW && ok == HIGH && lastBackState == HIGH) {
        playBeep(600, 20);
        if (screenState == 7) { currentMenu = 0; drawMainMenu(); }
        else if (screenState == 14) { keyboardMode = false; drawControlMode(); }
        else if (screenState == 16) { drawControlMain(); }
        else if (screenState == 15) { gamepadMode = false; drawControlMode(); }
        else if (screenState == 17) { airMouseMode = false; drawControlMain(); }
        else if (editingName) { editingName = false; drawSettings(); }
        else if (screenState != 0) { currentMenu = 0; drawMainMenu(); }
    }
    lastBackState = back;
    
    if (ok == LOW && lastOK == HIGH) {
        playBeep(1200, 20);
        if (screenState == 0) {
            if (currentMenu == 0) drawHome();
            else if (currentMenu == 1) { 
                if (airMouseMode) { airMouseTab = 0; drawAirMouseMode(); }
                else drawControlMode(); 
            }
            else if (currentMenu == 2) { startBLEScan(); drawDeviceScan(); }
            else if (currentMenu == 3) { drawClientMenu(); }
            else if (currentMenu == 4) { drawServerMenu(); }
            else if (currentMenu == 5) { settingIndex = 0; drawSettings(); }
        } else if (screenState == 4) {
            if (keyboardMode) { kbSelectedRow = 0; kbSelectedCol = 0; kbUpperCase = true; drawKeyboardMode(); }
            else if (gamepadMode) { drawGamepadMode(); }
        } else if (screenState == 14) {
            drawControlMode();
        } else if (screenState == 15) {
            drawControlMode();
        } else if (screenState == 17) {
            if (airMouseTab == 0 && deviceConnected) {
                sendMouseReport(0, 0, 0, 1);
                spawnParticles(120, 150, 0x07E0, 8);
                triggerScreenFlash(0x07E0, 30);
            } else if (airMouseTab == 1 && deviceConnected) {
                sendMediaKey(0x00);
                spawnParticles(97, 160, 0xF8B8, 10);
                triggerScreenFlash(0xF8B8, 40);
            } else if (airMouseTab == 2 && deviceConnected) {
                sendKeyReport(0, USB_HID_KEY_ESC, 0, 0, 0, 0, 0);
                spawnParticles(120, 100, 0x1082, 8);
                triggerScreenFlash(0x1082, 30);
            }
            drawAirMouseMode();
        } else if (screenState == 18) {
            if (up == LOW && lastUp == HIGH) {
                playBeep(800, 15);
                settingsSelect = (settingsSelect > 0) ? settingsSelect - 1 : 5;
                drawGamepadSettings();
            }
            if (down == LOW && lastDown == HIGH) {
                playBeep(800, 15);
                settingsSelect = (settingsSelect < 5) ? settingsSelect + 1 : 0;
                drawGamepadSettings();
            }
            if (left == LOW && lastLeft == HIGH) {
                playBeep(800, 15);
                if (settingsSelect == 0) { gamepadBrightness = max(10, gamepadBrightness - 10); }
                else if (settingsSelect == 1) { gamepadSound = !gamepadSound; }
                else if (settingsSelect == 2) { gamepadVibration = !gamepadVibration; }
                else if (settingsSelect == 3) { sleepTimer = max(10, sleepTimer - 10); }
                else if (settingsSelect == 4) { btPowerIndex = (btPowerIndex > 0) ? btPowerIndex - 1 : 2; }
                drawGamepadSettings();
            }
            if (right == LOW && lastRight == HIGH) {
                playBeep(800, 15);
                if (settingsSelect == 0) { gamepadBrightness = min(100, gamepadBrightness + 10); }
                else if (settingsSelect == 1) { gamepadSound = !gamepadSound; }
                else if (settingsSelect == 2) { gamepadVibration = !gamepadVibration; }
                else if (settingsSelect == 3) { sleepTimer = min(120, sleepTimer + 10); }
                else if (settingsSelect == 4) { btPowerIndex = (btPowerIndex < 2) ? btPowerIndex + 1 : 0; }
                drawGamepadSettings();
            }
            if (ok == LOW && lastOK == HIGH) {
                playBeep(1200, 20);
                if (settingsSelect == 0) { gamepadBrightness = min(100, gamepadBrightness + 10); }
                else if (settingsSelect == 1) { gamepadSound = !gamepadSound; }
                else if (settingsSelect == 2) { gamepadVibration = !gamepadVibration; }
                else if (settingsSelect == 3) { sleepTimer = min(120, sleepTimer + 10); }
                else if (settingsSelect == 4) { btPowerIndex = (btPowerIndex < 2) ? btPowerIndex + 1 : 0; }
                drawGamepadSettings();
            }
            if (back == LOW && lastBack == HIGH) {
                playBeep(600, 20);
                drawMainRemote();
            }
        } else if (screenState == 2 && ok == LOW && lastOK == HIGH) {
            if (gamepadMode) {
                playBeep(1200, 20);
                settingsSelect = 0;
                drawGamepadSettings();
            } else if (deviceConnected) {
                sendMediaKey(0x00);
                spawnParticles(120, 130, 0x07E0, 8);
                triggerScreenFlash(0x07E0, 30);
            }
            playClickSound();
            drawMediaRemote();
        } else if (screenState == 2 && back == LOW && lastBack == HIGH) {
            playBeep(600, 20);
            if (gamepadMode) {
                settingsSelect = 0;
                drawGamepadSettings();
            } else {
                currentMenu = 0;
                drawMainMenu();
            }
        } else if (screenState == 2 && btnMenu == LOW && lastKeyMenu == HIGH) {
            playBeep(1000, 20);
            if (gamepadMode) {
                gamepadMode = false;
                remoteModeActive = true;
            } else {
                remoteModeActive = false;
                gamepadMode = true;
            }
            drawMainRemote();
        } else if (screenState == 2 && left == LOW && lastLeft == HIGH) {
            playBeep(800, 15);
            remoteCounter = max(42, remoteCounter - 1);
            drawMainRemote();
        } else if (screenState == 2 && right == LOW && lastRight == HIGH) {
            playBeep(800, 15);
            remoteCounter = min(99, remoteCounter + 1);
            drawMainRemote();
        } else if (screenState == 7) {
            if (foundDevices > 0) {
                tft.fillScreen(BGCOLOR);
                drawHeader("CONNECTING");
                tft.setTextSize(2); tft.setTextColor(FGCOLOR);
                tft.drawCentreString("Connecting to", 120, 60, 1);
                tft.drawCentreString(devNames[selectedDevice].substring(0, 12), 120, 90, 1);
                playBeep(1500, 100);
                tft.setTextColor(0x8410);
                tft.drawCentreString("This feature requires", 120, 140, 1);
                tft.drawCentreString("BLE Client support", 120, 160, 1);
                delay(2000);
                drawDeviceScan();
            } else {
                startBLEScan();
                drawDeviceScan();
            }
            startBLEScan();
            drawDeviceScan();
        } else if (screenState == 6) {
            if (settingIndex == 0) { remoteModeActive = !remoteModeActive; drawSettings(); }
            else if (settingIndex == 1) { airMouseMode = !airMouseMode; drawSettings(); }
            else if (settingIndex == 2) { keyboardMode = !keyboardMode; if (keyboardMode) { keyboardModeType = 0; kbSelectedRow = 0; kbSelectedCol = 0; kbUpperCase = true; drawKeyboardMode(); } else drawSettings(); }
            else if (settingIndex == 3) { gamepadMode = !gamepadMode; if (gamepadMode) { gamepadModeType = 0; for (int i = 0; i < 8; i++) gamepadBtn[i] = false; gamepadX = 127; gamepadY = 127; drawGamepadMode(); } else drawSettings(); }
            else if (settingIndex == 4) { clickSound = !clickSound; drawSettings(); }
            else if (settingIndex == 5) { continuousMove = !continuousMove; drawSettings(); }
            else if (settingIndex == 6) { mouseSpeed = (mouseSpeed < 20) ? mouseSpeed + 2 : 5; drawSettings(); }
            else if (settingIndex == 7) { digitalWrite(TFT_BL, digitalRead(TFT_BL) ? LOW : HIGH); drawSettings(); }
            else if (settingIndex == 8) { editingName = true; kbSelectedRow = 0; kbSelectedCol = 0; kbPage = 0; kbUpperCase = false; strcpy(tempName, deviceName); drawKeyboardUI(); }
        } else if (editingName) {
            if (kbSelectedRow == 3) {
                if (kbSelectedCol == 0) {
                    int len = strlen(tempName); if (len > 0) tempName[len - 1] = 0;
                    drawKeyboardUI();
                } else if (kbSelectedCol == 1) {
                    kbPage = 1;
                    kbSelectedRow = 0; kbSelectedCol = 0;
                    drawKeyboardUI();
                } else if (kbSelectedCol == 2) {
                    kbPage = 0;
                    kbUpperCase = !kbUpperCase;
                    kbSelectedRow = 0; kbSelectedCol = 0;
                    drawKeyboardUI();
                } else if (kbSelectedCol == 3) {
                    kbPage = 2;
                    kbSelectedRow = 0; kbSelectedCol = 0;
                    drawKeyboardUI();
                } else if (kbSelectedCol == 4) {
                    int len = strlen(tempName); if (len < 31) { tempName[len] = ' '; tempName[len + 1] = 0; }
                    drawKeyboardUI();
                } else if (kbSelectedCol == 5) {
                    strcpy(deviceName, tempName); saveSettings();
                    if (clickSound) { playBeep(1500, 100); delay(150); playBeep(2000, 100); }
                    editingName = false; drawSettings();
                }
            } else {
                char rowChar = 0;
                if (kbPage == 0) {
                    if (kbSelectedRow == 0 && kbSelectedCol < 10) rowChar = kbUpperCase ? kbRow1_qwerty[kbSelectedCol] - 32 : kbRow1_qwerty[kbSelectedCol];
                    else if (kbSelectedRow == 1 && kbSelectedCol < 9) rowChar = kbUpperCase ? kbRow2_qwerty[kbSelectedCol] - 32 : kbRow2_qwerty[kbSelectedCol];
                    else if (kbSelectedRow == 2 && kbSelectedCol < 7) rowChar = kbUpperCase ? kbRow3_qwerty[kbSelectedCol] - 32 : kbRow3_qwerty[kbSelectedCol];
                } else if (kbPage == 1) {
                    if (kbSelectedRow == 0 && kbSelectedCol < 10) rowChar = kbRow1_num[kbSelectedCol];
                    else if (kbSelectedRow == 1 && kbSelectedCol < 10) rowChar = kbRow2_num[kbSelectedCol];
                    else if (kbSelectedRow == 2 && kbSelectedCol < 9) rowChar = kbRow3_num[kbSelectedCol];
                } else if (kbPage == 2) {
                    if (kbSelectedRow == 0 && kbSelectedCol < 9) rowChar = kbRow4_num[kbSelectedCol];
                }
                if (rowChar != 0) { int len = strlen(tempName); if (len < 31) { tempName[len] = rowChar; tempName[len + 1] = 0; } }
                drawKeyboardUI();
            }
        }
    }
    
    if (screenState == 0) {
        if (up == LOW && lastUp == HIGH) { playBeep(800, 15); currentMenu = (currentMenu > 0) ? currentMenu - 1 : 5; drawMainMenu(); }
        if (down == LOW && lastDown == HIGH) { playBeep(800, 15); currentMenu = (currentMenu < 5) ? currentMenu + 1 : 0; drawMainMenu(); }
    } else if (screenState == 16) {
        if (up == LOW && lastUp == HIGH) {
            playBeep(800, 15);
            controlModeSelect = (controlModeSelect > 0) ? controlModeSelect - 1 : 3;
            drawControlMenu();
        }
        if (down == LOW && lastDown == HIGH) {
            playBeep(800, 15);
            controlModeSelect = (controlModeSelect < 3) ? controlModeSelect + 1 : 0;
            drawControlMenu();
        }
        if (left == LOW && lastLeft == HIGH) {
            playBeep(800, 15);
            controlModeSelect = (controlModeSelect > 0) ? controlModeSelect - 1 : 3;
            drawControlMenu();
        }
        if (right == LOW && lastRight == HIGH) {
            playBeep(800, 15);
            controlModeSelect = (controlModeSelect < 3) ? controlModeSelect + 1 : 0;
            drawControlMenu();
        }
        if (ok == LOW && lastOK == HIGH) {
            playBeep(1200, 20);
            remoteModeActive = (controlModeSelect == 0);
            airMouseMode = (controlModeSelect == 1);
            keyboardMode = (controlModeSelect == 2);
            gamepadMode = (controlModeSelect == 3);
            if (keyboardMode) { keyboardModeType = 0; kbSelectedRow = 0; kbSelectedCol = 0; kbUpperCase = true; }
            if (gamepadMode) { gamepadModeType = 0; for (int i = 0; i < 8; i++) gamepadBtn[i] = false; gamepadX = 127; gamepadY = 127; }
            if (airMouseMode) { airMouseTab = 0; drawAirMouseMode(); }
            else drawControlMain();
        }
        if (back == LOW && lastBack == HIGH) {
            playBeep(600, 20);
            drawControlMain();
        }
    } else if (screenState == 4) {
        if (keyMenu == LOW && lastKeyMenu == HIGH) {
            playBeep(1000, 20);
            spawnParticles(120, 90, FGCOLOR, 8);
            triggerScreenFlash(FGCOLOR, 30);
            drawControlMenu();
        }
        if (left == LOW && lastLeft == HIGH) {
            playBeep(800, 15);
            if (remoteModeActive) { remoteModeActive = false; airMouseMode = true; airMouseTab = 0; drawAirMouseMode(); }
            else if (airMouseMode) { airMouseMode = false; keyboardMode = true; keyboardModeType = 0; kbSelectedRow = 0; kbSelectedCol = 0; kbUpperCase = true; drawKeyboardMode(); }
            else if (keyboardMode) { keyboardMode = false; gamepadMode = true; drawGamepadMode(); }
            else { gamepadMode = false; remoteModeActive = true; drawControlMode(); }
        }
        if (right == LOW && lastRight == HIGH) {
            playBeep(800, 15);
            if (remoteModeActive) { remoteModeActive = false; gamepadMode = true; drawGamepadMode(); }
            else if (gamepadMode) { gamepadMode = false; keyboardMode = true; keyboardModeType = 0; kbSelectedRow = 0; kbSelectedCol = 0; kbUpperCase = true; drawKeyboardMode(); }
            else if (keyboardMode) { keyboardMode = false; airMouseMode = true; airMouseTab = 0; drawAirMouseMode(); }
            else { airMouseMode = false; remoteModeActive = true; drawControlMode(); }
        }
    }
    
    if (screenState == 17) {
        if (left == LOW && lastLeft == HIGH) {
            playBeep(800, 15);
            airMouseTab = (airMouseTab > 0) ? airMouseTab - 1 : 2;
            drawAirMouseMode();
        }
        if (right == LOW && lastRight == HIGH) {
            playBeep(800, 15);
            airMouseTab = (airMouseTab < 2) ? airMouseTab + 1 : 0;
            drawAirMouseMode();
        }
    } else if (screenState == 2 && deviceConnected) {
        if (millis() - lastTikTokTime > 400) {
            lastTikTokTime = millis();
            if (up == LOW) { sendMediaKey(0x15); spawnParticles(120, 120, 0x07E0, 6); triggerScreenFlash(0x07E0, 25); }
            if (down == LOW) { sendMediaKey(0x16); spawnParticles(120, 120, 0x07E0, 6); triggerScreenFlash(0x07E0, 25); }
            if (left == LOW) { sendMediaKey(0x16); spawnParticles(80, 150, 0xF800, 6); triggerScreenFlash(0xF800, 25); }
            if (right == LOW) { sendMediaKey(0x15); spawnParticles(160, 150, 0xF800, 6); triggerScreenFlash(0xF800, 25); }
        }
        
        static int lastSelectState = HIGH;
        static int lastStartState = HIGH;
        static int lastOptionState = HIGH;
        
        if (btnSelect && lastSelectState == HIGH) {
            playBeep(1200, 20);
            sendMediaKey(0x00);
            spawnParticles(97, 242, 0xF8B8, 10);
            triggerScreenFlash(0xF8B8, 40);
            drawMediaRemote();
        }
        lastSelectState = btnSelect ? LOW : HIGH;
        
        if (btnStart && lastStartState == HIGH) {
            playBeep(1000, 20);
            sendMediaKey(0x15);
            spawnParticles(157, 242, 0x07E0, 8);
            triggerScreenFlash(0x07E0, 30);
            drawMediaRemote();
        }
        lastStartState = btnStart ? LOW : HIGH;
        
        if (btnOption && lastOptionState == HIGH) {
            playBeep(1000, 20);
            sendMediaKey(0x16);
            spawnParticles(45, 242, 0x07E0, 8);
            triggerScreenFlash(0x07E0, 30);
            drawMediaRemote();
        }
        lastOptionState = btnOption ? LOW : HIGH;
    } else if (screenState == 6 && !editingName) {
        if (up == LOW && lastUp == HIGH) { playBeep(800, 15); settingIndex = (settingIndex > 0) ? settingIndex - 1 : 8; drawSettings(); }
        if (down == LOW && lastDown == HIGH) { playBeep(800, 15); settingIndex = (settingIndex < 8) ? settingIndex + 1 : 0; drawSettings(); }
    } else if (screenState == 7 && !scanning && foundDevices > 0) {
        if (up == LOW && lastUp == HIGH) { playBeep(800, 15); selectedDevice = (selectedDevice > 0) ? selectedDevice - 1 : foundDevices - 1; drawDeviceScan(); }
        if (down == LOW && lastDown == HIGH) { playBeep(800, 15); selectedDevice = (selectedDevice < foundDevices - 1) ? selectedDevice + 1 : 0; drawDeviceScan(); }
    } else if (editingName) {
        int maxCols[3] = {10, 10, 9};
        if (kbPage == 0) maxCols[1] = 9, maxCols[2] = 7;
        else if (kbPage == 1) maxCols[2] = 9;
        else if (kbPage == 2) maxCols[0] = 9, maxCols[1] = 0, maxCols[2] = 0;
        
        if (up == LOW && lastUp == HIGH) {
            playBeep(800, 15);
            kbSelectedRow = (kbSelectedRow > 0) ? kbSelectedRow - 1 : 3;
            if (kbSelectedRow < 3 && kbSelectedCol >= maxCols[kbSelectedRow]) kbSelectedCol = maxCols[kbSelectedRow] - 1;
            drawKeyboardUI();
        }
        if (down == LOW && lastDown == HIGH) {
            playBeep(800, 15);
            kbSelectedRow = (kbSelectedRow < 3) ? kbSelectedRow + 1 : 0;
            if (kbSelectedRow < 3 && kbSelectedCol >= maxCols[kbSelectedRow]) kbSelectedCol = maxCols[kbSelectedRow] - 1;
            drawKeyboardUI();
        }
        if (left == LOW && lastLeft == HIGH) {
            playBeep(800, 15);
            if (kbSelectedRow == 3) {
                kbSelectedCol = (kbSelectedCol > 0) ? kbSelectedCol - 1 : 5;
            } else if (kbSelectedRow < 3) {
                int maxC = (kbSelectedRow == 3) ? 5 : maxCols[kbSelectedRow];
                kbSelectedCol = (kbSelectedCol > 0) ? kbSelectedCol - 1 : maxCols[kbSelectedRow] - 1;
            }
            drawKeyboardUI();
        }
        if (right == LOW && lastRight == HIGH) {
            playBeep(800, 15);
            if (kbSelectedRow == 3) {
                kbSelectedCol = (kbSelectedCol < 5) ? kbSelectedCol + 1 : 0;
            } else if (kbSelectedRow < 3) {
                kbSelectedCol = (kbSelectedCol < maxCols[kbSelectedRow] - 1) ? kbSelectedCol + 1 : 0;
            }
            drawKeyboardUI();
        }
    }
    
    bool mouseEnabled = (remoteModeActive || airMouseMode);
    
    if (mouseEnabled && deviceConnected && (screenState == 4 || screenState == 14 || screenState == 16 || screenState == 17)) {
        if (millis() - lastMoveTime > 80) {
            lastMoveTime = millis();
            bool okHeld = (ok == LOW);
            bool backHeld = (back == LOW);
            
            if (keyboardMode && (screenState == 14)) {
                int maxCols[3] = {10, 10, 9};
                if (keyboardModeType == 0) maxCols[1] = 9, maxCols[2] = 7;
                else if (keyboardModeType == 1) maxCols[2] = 9;
                else if (keyboardModeType == 2) maxCols[0] = 9, maxCols[1] = 0, maxCols[2] = 0;
                
                if (up == LOW && lastUp == HIGH) {
                    playBeep(800, 15);
                    kbSelectedRow = (kbSelectedRow > 0) ? kbSelectedRow - 1 : 3;
                    if (kbSelectedRow < 3 && kbSelectedCol >= maxCols[kbSelectedRow]) kbSelectedCol = maxCols[kbSelectedRow] - 1;
                    drawKeyboardMode();
                }
                if (down == LOW && lastDown == HIGH) {
                    playBeep(800, 15);
                    kbSelectedRow = (kbSelectedRow < 3) ? kbSelectedRow + 1 : 0;
                    if (kbSelectedRow < 3 && kbSelectedCol >= maxCols[kbSelectedRow]) kbSelectedCol = maxCols[kbSelectedRow] - 1;
                    drawKeyboardMode();
                }
                if (left == LOW && lastLeft == HIGH) {
                    playBeep(800, 15);
                    if (kbSelectedRow == 3) {
                        kbSelectedCol = (kbSelectedCol > 0) ? kbSelectedCol - 1 : 5;
                    } else if (kbSelectedRow < 3) {
                        kbSelectedCol = (kbSelectedCol > 0) ? kbSelectedCol - 1 : maxCols[kbSelectedRow] - 1;
                    }
                    drawKeyboardMode();
                }
                if (right == LOW && lastRight == HIGH) {
                    playBeep(800, 15);
                    if (kbSelectedRow == 3) {
                        kbSelectedCol = (kbSelectedCol < 5) ? kbSelectedCol + 1 : 0;
                    } else if (kbSelectedRow < 3) {
                        kbSelectedCol = (kbSelectedCol < maxCols[kbSelectedRow] - 1) ? kbSelectedCol + 1 : 0;
                    }
                    drawKeyboardMode();
                }
                if (ok == LOW && lastOK == HIGH) {
                    playBeep(1200, 20);
                    if (kbSelectedRow == 3) {
                        if (kbSelectedCol == 0) { kbShiftHeld = !kbShiftHeld; }
                        else if (kbSelectedCol == 1) { sendKeyReport(0, USB_HID_KEY_BACKSPACE, 0, 0, 0, 0, 0); }
                        else if (kbSelectedCol == 2) { keyboardModeType = 1; kbSelectedRow = 0; kbSelectedCol = 0; }
                        else if (kbSelectedCol == 3) { keyboardModeType = 0; kbSelectedRow = 0; kbSelectedCol = 0; kbUpperCase = !kbUpperCase; }
                        else if (kbSelectedCol == 4) { sendKeyReport(0, USB_HID_KEY_SPACE, 0, 0, 0, 0, 0); }
                        else if (kbSelectedCol == 5) { sendKeyReport(0, USB_HID_KEY_ENTER, 0, 0, 0, 0, 0); }
                    } else {
                        char rowChar = 0;
                        if (keyboardModeType == 0) {
                            const char* rows[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
                            if (kbSelectedRow >= 0 && kbSelectedRow < 3 && kbSelectedCol < strlen(rows[kbSelectedRow])) {
                                rowChar = kbShiftHeld ? toupper(rows[kbSelectedRow][kbSelectedCol]) : rows[kbSelectedRow][kbSelectedCol];
                            }
                        } else if (keyboardModeType == 1) {
                            const char* rows[3] = {"1234567890", "-/:;()$&@\"", ".,?!'#%"};
                            if (kbSelectedRow >= 0 && kbSelectedRow < 3 && kbSelectedCol < strlen(rows[kbSelectedRow])) {
                                rowChar = rows[kbSelectedRow][kbSelectedCol];
                            }
                        } else {
                            const char* rows[3] = {"]\\[{}|", "*+=_()", "0123456"};
                            if (kbSelectedRow >= 0 && kbSelectedRow < 3 && kbSelectedCol < strlen(rows[kbSelectedRow])) {
                                rowChar = rows[kbSelectedRow][kbSelectedCol];
                            }
                        }
                        if (rowChar != 0) {
                            uint8_t modifier = kbShiftHeld ? USB_HID_MODIFIER_LEFTSHIFT : 0;
                            sendKeyReport(modifier, charToHIDKey(rowChar), 0, 0, 0, 0, 0);
                        }
                    }
                    drawKeyboardMode();
                }
            } else if (okHeld && (up == LOW || down == LOW || left == LOW || right == LOW)) {
                if (up == LOW) sendConsumerKey(0x6A);
                if (down == LOW) sendConsumerKey(0x6B);
                if (left == LOW) sendConsumerKey(0x6B);
                if (right == LOW) sendConsumerKey(0x6A);
            } else if ((screenState != 17) || (screenState == 17 && airMouseTab == 0)) {
                if (up == LOW) { sendMouseReport(0, -mouseSpeed, 0, 0); }
                if (down == LOW) { sendMouseReport(0, mouseSpeed, 0, 0); }
                if (left == LOW) { sendMouseReport(-mouseSpeed, 0, 0, 0); }
                if (right == LOW) { sendMouseReport(mouseSpeed, 0, 0, 0); }
            }
        }
    }
    
    if (gamepadMode && deviceConnected && screenState == 15) {
        if (millis() - lastMoveTime > 50) {
            lastMoveTime = millis();
            int8_t gx = 0, gy = 0;
            if (up == LOW) gy = -127;
            if (down == LOW) gy = 127;
            if (left == LOW) gx = -127;
            if (right == LOW) gx = 127;
            
            if (gx != gamepadX || gy != gamepadY) {
                gamepadX = gx;
                gamepadY = gy;
            }
            
            if (ok == LOW && !gamepadBtn[0]) gamepadBtn[0] = true;
            if (ok == HIGH && gamepadBtn[0]) gamepadBtn[0] = false;
            if (back == LOW && !gamepadBtn[1]) gamepadBtn[1] = true;
            if (back == HIGH && gamepadBtn[1]) gamepadBtn[1] = false;
            
            uint8_t gamepadReport[5] = {(uint8_t)(gamepadX + 127), (uint8_t)(gamepadY + 127), 
                                         (uint8_t)(gamepadX + 127), (uint8_t)(gamepadY + 127), 0};
            if (gamepadBtn[0]) gamepadReport[4] |= 0x01;
            if (gamepadBtn[1]) gamepadReport[4] |= 0x02;
            if (gamepadBtn[2]) gamepadReport[4] |= 0x04;
            if (gamepadBtn[3]) gamepadReport[4] |= 0x08;
            pHIDReport->setValue(gamepadReport, 5);
            pHIDReport->notify();
        }
    }
    if (mouseEnabled && screenState == 4) {
        static int lastHover = -1;
        if (currentHover != lastHover) {
            lastHover = currentHover;
            drawControlMode();
        }
    }
    
    if (mouseEnabled && back == LOW && !rightBtnDown && (screenState == 4 || screenState == 14 || screenState == 16)) {
        if (deviceConnected) {
            if (millis() - mouseRightPressTime < 300) {
                sendMouseReport(0, 0, 0, 2);
                delay(10);
            }
            sendMouseReport(0, 0, 0, 2);
            playClickSound();
        }
        rightBtnDown = true;
        mouseRightPressTime = millis();
    } else if (back == HIGH && rightBtnDown) {
        if (deviceConnected) sendMouseReport(0, 0, 0, 0);
        rightBtnDown = false;
    }
    
    if (mouseEnabled && ok == LOW && !okBtnDown && (screenState == 4 || screenState == 14 || screenState == 16)) {
        if (deviceConnected) {
            unsigned long now = millis();
            if (now - lastLeftClickTime < 300) {
                sendMouseReport(0, 0, 0, 1);
                delay(10);
                sendMouseReport(0, 0, 0, 0);
                sendMouseReport(0, 0, 0, 1);
                sendMouseReport(0, 0, 0, 0);
                doubleClickEnabled = true;
            } else {
                sendMouseReport(0, 0, 0, 1);
            }
            playClickSound();
            lastLeftClickTime = now;
        }
        okBtnDown = true;
        mouseLeftPressTime = millis();
    } else if (ok == HIGH && okBtnDown) {
        if (deviceConnected && !doubleClickEnabled) {
            if (millis() - mouseLeftPressTime > 500) {
                mouseDragging = true;
                mouseDragStartX = 0;
                mouseDragStartY = 0;
            }
            sendMouseReport(0, 0, 0, 0);
        }
        okBtnDown = false;
        doubleClickEnabled = false;
        mouseDragging = false;
    }
    
    if (mouseEnabled && mouseDragging && deviceConnected && (screenState == 4 || screenState == 17) && airMouseTab == 0) {
        if (up == LOW) { sendMouseReport(0, -mouseSpeed, 0, 1); }
        if (down == LOW) { sendMouseReport(0, mouseSpeed, 0, 1); }
        if (left == LOW) { sendMouseReport(-mouseSpeed, 0, 0, 1); }
        if (right == LOW) { sendMouseReport(mouseSpeed, 0, 0, 1); }
    }
    
    lastUp = up; lastDown = down; lastLeft = left; lastRight = right; lastOK = ok; lastBack = back; lastKeyMenu = keyMenu;
}

void sendBLEData(String data) {
    if (deviceConnected && pUART) { pUART->setValue(data.c_str()); pUART->notify(); }
}

void processBLECommand(String cmd) {
    cmd.trim(); cmd.toUpperCase();
    if (cmd == "BUZZER") { sendBLEData("BUZZER_NOT_AVAILABLE"); }
    else if (cmd == "BL_ON") { digitalWrite(TFT_BL, HIGH); drawSettings(); sendBLEData("BACKLIGHT_ON"); }
    else if (cmd == "BL_OFF") { digitalWrite(TFT_BL, LOW); drawSettings(); sendBLEData("BACKLIGHT_OFF"); }
    else if (cmd == "STATUS") {
        String status = "BAT:" + String(analogRead(PIN_BAT)) +
                       "|REMOTE:" + String(remoteModeActive ? "ON" : "OFF") + "|MOUSE:" + String(airMouseMode ? "ON" : "OFF") +
                       "|NAME:" + String(deviceName) + "|CLIENT:" + String(clientIsConnected() ? "ON" : "OFF");
        sendBLEData(status);
    }
    else if (cmd.startsWith("NAME:")) { String newName = cmd.substring(5); newName.trim(); if (newName.length() > 0 && newName.length() < 32) { strcpy(deviceName, newName.c_str()); saveSettings(); sendBLEData("NAME_CHANGED:" + String(deviceName)); } else sendBLEData("NAME_ERROR"); }
    else if (cmd == "REMOTE_ON") { remoteModeActive = true; sendBLEData("REMOTE_ON"); }
    else if (cmd == "REMOTE_OFF") { remoteModeActive = false; sendBLEData("REMOTE_OFF"); }
    else if (cmd == "MOUSE_ON") { airMouseMode = true; sendBLEData("MOUSE_ON"); }
    else if (cmd == "MOUSE_OFF") { airMouseMode = false; sendBLEData("MOUSE_OFF"); }
    else if (cmd == "KEYBOARD_ON") { keyboardMode = true; sendBLEData("KEYBOARD_ON"); }
    else if (cmd == "KEYBOARD_OFF") { keyboardMode = false; sendBLEData("KEYBOARD_OFF"); }
    else if (cmd == "GAMEPAD_ON") { gamepadMode = true; sendBLEData("GAMEPAD_ON"); }
    else if (cmd == "GAMEPAD_OFF") { gamepadMode = false; sendBLEData("GAMEPAD_OFF"); }
    else if (cmd.startsWith("TYPE:")) {
        String text = cmd.substring(5);
        if (keyboardMode || airMouseMode) {
            sendString(text);
            sendBLEData("TYPE_OK");
        } else sendBLEData("TYPE_ERR");
    }
    else if (cmd.startsWith("KEY:")) {
        String key = cmd.substring(4);
        if (keyboardMode || airMouseMode) {
            key.toUpperCase();
            if (key.length() == 1) sendChar(key[0], false);
            else if (key == "ENTER") sendKeyReport(0, USB_HID_KEY_ENTER, 0, 0, 0, 0, 0);
            else if (key == "ESC") sendKeyReport(0, USB_HID_KEY_ESC, 0, 0, 0, 0, 0);
            else if (key == "TAB") sendKeyReport(0, USB_HID_KEY_TAB, 0, 0, 0, 0, 0);
            else if (key == "BACKSPACE") sendKeyReport(0, USB_HID_KEY_BACKSPACE, 0, 0, 0, 0, 0);
            else if (key == "SPACE") sendKeyReport(0, USB_HID_KEY_SPACE, 0, 0, 0, 0, 0);
            else if (key == "UP") sendKeyReport(0, USB_HID_KEY_UP, 0, 0, 0, 0, 0);
            else if (key == "DOWN") sendKeyReport(0, USB_HID_KEY_DOWN, 0, 0, 0, 0, 0);
            else if (key == "LEFT") sendKeyReport(0, USB_HID_KEY_LEFT, 0, 0, 0, 0, 0);
            else if (key == "RIGHT") sendKeyReport(0, USB_HID_KEY_RIGHT, 0, 0, 0, 0, 0);
            else if (key == "F1") sendKeyReport(0, USB_HID_KEY_F1, 0, 0, 0, 0, 0);
            else if (key == "F2") sendKeyReport(0, USB_HID_KEY_F2, 0, 0, 0, 0, 0);
            else if (key == "F3") sendKeyReport(0, USB_HID_KEY_F3, 0, 0, 0, 0, 0);
            else if (key == "F4") sendKeyReport(0, USB_HID_KEY_F4, 0, 0, 0, 0, 0);
            else if (key == "F5") sendKeyReport(0, USB_HID_KEY_F5, 0, 0, 0, 0, 0);
            else if (key == "F6") sendKeyReport(0, USB_HID_KEY_F6, 0, 0, 0, 0, 0);
            else if (key == "F7") sendKeyReport(0, USB_HID_KEY_F7, 0, 0, 0, 0, 0);
            else if (key == "F8") sendKeyReport(0, USB_HID_KEY_F8, 0, 0, 0, 0, 0);
            else if (key == "F9") sendKeyReport(0, USB_HID_KEY_F9, 0, 0, 0, 0, 0);
            else if (key == "F10") sendKeyReport(0, USB_HID_KEY_F10, 0, 0, 0, 0, 0);
            else if (key == "F11") sendKeyReport(0, USB_HID_KEY_F11, 0, 0, 0, 0, 0);
            else if (key == "F12") sendKeyReport(0, USB_HID_KEY_F12, 0, 0, 0, 0, 0);
            else if (key == "CTRL") sendKeyReport(USB_HID_MODIFIER_LEFTCTRL, 0, 0, 0, 0, 0, 0);
            else if (key == "SHIFT") sendKeyReport(USB_HID_MODIFIER_LEFTSHIFT, 0, 0, 0, 0, 0, 0);
            else if (key == "ALT") sendKeyReport(USB_HID_MODIFIER_LEFTALT, 0, 0, 0, 0, 0, 0);
            else if (key == "GUI") sendKeyReport(USB_HID_MODIFIER_LEFTGUI, 0, 0, 0, 0, 0, 0);
            sendBLEData("KEY_OK");
        } else sendBLEData("KEY_ERR");
    }
    else if (cmd.startsWith("KEY_MOD:")) {
        String key = cmd.substring(8);
        uint8_t modifier = 0;
        if (key.startsWith("CTRL")) modifier |= USB_HID_MODIFIER_LEFTCTRL;
        if (key.startsWith("SHIFT")) modifier |= USB_HID_MODIFIER_LEFTSHIFT;
        if (key.indexOf("ALT") >= 0) modifier |= USB_HID_MODIFIER_LEFTALT;
        if (key.indexOf("GUI") >= 0) modifier |= USB_HID_MODIFIER_LEFTGUI;
        int colonPos = key.indexOf(':');
        if (colonPos > 0) {
            String actualKey = key.substring(colonPos + 1);
            actualKey.trim();
            if (actualKey.length() == 1) {
                sendKeyReport(modifier, charToHIDKey(actualKey[0]), 0, 0, 0, 0, 0);
                sendBLEData("KEY_MOD_OK");
            }
        }
    }
    else if (cmd == "SCROLL_UP" && (remoteModeActive || airMouseMode) && deviceConnected) { sendConsumerKey(0x6A); sendBLEData("SCROLL_OK"); }
    else if (cmd == "SCROLL_DOWN" && (remoteModeActive || airMouseMode) && deviceConnected) { sendConsumerKey(0x6B); sendBLEData("SCROLL_OK"); }
    else if (cmd == "MEDIA_PLAY" || cmd == "MEDIA_PAUSE") { if (deviceConnected) sendMediaKey(0x00); sendBLEData("PLAY"); }
    else if (cmd == "MEDIA_PREV") { if (deviceConnected) sendMediaKey(0x16); sendBLEData("PREV"); }
    else if (cmd == "MEDIA_NEXT") { if (deviceConnected) sendMediaKey(0x15); sendBLEData("NEXT"); }
    else if (cmd == "MEDIA_VOL_UP") { if (deviceConnected) sendMediaKey(0x19); sendBLEData("VOL_UP"); }
    else if (cmd == "MEDIA_VOL_DOWN") { if (deviceConnected) sendMediaKey(0x18); sendBLEData("VOL_DOWN"); }
    else if (cmd.startsWith("MOVE:") && (remoteModeActive || airMouseMode) && deviceConnected) {
        String coords = cmd.substring(5); int comma = coords.indexOf(',');
        if (comma > 0) { int8_t x = coords.substring(0, comma).toInt(); int8_t y = coords.substring(comma + 1).toInt(); sendMouseReport(x, y, 0, 0); }
        sendBLEData("MOVE_OK");
    }
    else if (cmd == "CLICK" && (remoteModeActive || airMouseMode)) { sendMouseReport(0, 0, 0, 1); sendBLEData("CLICK_OK"); }
    else if (cmd == "CLICK_R" && (remoteModeActive || airMouseMode)) { sendMouseReport(0, 0, 0, 2); sendBLEData("CLICK_R_OK"); }
    else if (cmd.startsWith("CLIENT_CONN:")) { 
        String target = cmd.substring(12);
        int colonIdx = target.indexOf(':');
        String addr = target.substring(0, colonIdx);
        String uuid = colonIdx > 0 ? target.substring(colonIdx + 1) : "";
        if (clientConnectByAddress(addr, uuid)) sendBLEData("CLIENT_CONN_OK");
        else sendBLEData("CLIENT_CONN_FAIL");
    }
    else if (cmd.startsWith("CLIENT_UUID:")) {
        String uuid = cmd.substring(12);
        uuid.trim();
        if (clientConnectByUUID(uuid)) sendBLEData("CLIENT_UUID_OK");
        else sendBLEData("CLIENT_UUID_FAIL");
    }
    else if (cmd == "CLIENT_DISC") { 
        clientDisconnect(); 
        sendBLEData("CLIENT_DISC_OK"); 
    }
    else if (cmd == "CLIENT_PAIRED") {
        sendBLEData("CLIENT_PAIRED:" + String(clientIsConnected() ? "YES" : "NO"));
    }
    else if (cmd == "CLIENT_AVAIL") {
        sendBLEData("CLIENT_AVAIL:" + String(clientAvailable()));
    }
    else if (cmd == "CLIENT_READ") {
        String data = clientRead();
        if (data.length() > 0) sendBLEData("CLIENT_RX:" + data);
        else sendBLEData("CLIENT_RX:EMPTY");
    }
    else if (cmd.startsWith("CLIENT_TX:")) {
        String data = cmd.substring(10);
        clientSendData(data);
        sendBLEData("CLIENT_TX_OK");
    }
    else if (cmd == "CLIENT_SCAN") {
        startClientScan();
        sendBLEData("CLIENT_SCAN_DONE:" + String(clientFoundDevices));
    }
    else if (cmd.startsWith("CLIENT_SCAN_UUID:")) {
        String uuid = cmd.substring(16);
        uuid.trim();
        startClientScanByUUID(uuid);
        String result = "CLIENT_SCAN_UUID_DONE:" + String(clientFoundDevices);
        for (int i = 0; i < clientFoundDevices && i < 5; i++) {
            result += "|" + clientDevNames[i] + ":" + clientDevAddrs[i];
        }
        sendBLEData(result);
    }
    else if (cmd == "CLIENT_ADDRS") {
        String result = "CLIENT_ADDRS:" + String(clientFoundDevices);
        for (int i = 0; i < clientFoundDevices; i++) {
            result += "|" + clientDevNames[i] + ":" + clientDevAddrs[i] + ":" + String(clientDevRSSI[i]) + "dBm";
        }
        sendBLEData(result);
    }
    else if (cmd == "SERVER_ACCEPT") {
        serverAcceptConnection();
        sendBLEData("SERVER_ACCEPT_OK");
    }
    else if (cmd.startsWith("SERVER_ACCEPT_UUID:")) {
        String uuid = cmd.substring(18);
        uuid.trim();
        serverAcceptConnectionWithUUID(uuid);
        sendBLEData("SERVER_ACCEPT_UUID_OK:" + uuid);
    }
    else if (cmd == "SERVER_STOP") {
        serverStopAccepting();
        sendBLEData("SERVER_STOP_OK");
    }
    else if (cmd == "SERVER_STATUS") {
        String status = "SERVER_CONN:" + String(serverIsConnected() ? "YES" : "NO") + 
                       "|SERVER_ACCEPT:" + String(serverIsAccepting() ? "YES" : "NO") +
                       "|SERVER_UUID:" + (serverAcceptUUID.length() > 0 ? serverAcceptUUID : SERVICE_UUID);
        sendBLEData(status);
    }
    else if (cmd == "SERVER_DISCONNECT") {
        if (deviceConnected) {
            BLEDevice::stopAdvertising();
            delay(100);
            BLEDevice::startAdvertising();
            sendBLEData("SERVER_DISCONNECT_OK");
        } else {
            sendBLEData("SERVER_DISCONNECT_NONE");
        }
    }
    else sendBLEData("UNKNOWN_CMD");
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        serverWaitingConnection = false;
        if (clickSound) { playBeep(1500, 80); delay(100); playBeep(2000, 120); }
        if (screenState == 12 || screenState == 13) {
            drawServerMenu();
        } else {
            drawHome();
        }
        Serial.println("Server: Client connected");
    }
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false; 
        airMouseMode = false; 
        remoteModeActive = false;
        keyboardMode = false;
        gamepadMode = false;
        if (clickSound) { playBeep(800, 150); delay(200); playBeep(600, 150); }
        if (serverAccepting) {
            serverStartAdvertising();
        }
        if (screenState == 12) {
            drawServerMenu();
        } else {
            currentMenu = 0; drawMainMenu();
        }
        Serial.println("Server: Client disconnected");
    }
};

class UARTCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String inputString = pCharacteristic->getValue().c_str();
        if (inputString.length() > 0) { Serial.println("RX: " + inputString); processBLECommand(inputString); }
    }
};

class BatteryLevelCallbacks: public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* pCharacteristic) { uint8_t level = (analogRead(PIN_BAT) * 100) / 4095; pCharacteristic->setValue(&level, 1); }
};

void setup() {
    Serial.begin(115200); delay(500);
    randomSeed(analogRead(0));
    loadSettings();
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    SPI.begin(TFT_SCL, -1, TFT_SDA, -1);
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(0x0000);
    pinMode(KEY_UP, INPUT_PULLUP); pinMode(KEY_DOWN, INPUT_PULLUP); pinMode(KEY_LEFT, INPUT_PULLUP);
    pinMode(KEY_RIGHT, INPUT_PULLUP); pinMode(KEY_A, INPUT_PULLUP); pinMode(KEY_B, INPUT_PULLUP);
    pinMode(KEY_MENU, INPUT_PULLUP); pinMode(KEY_OPTION, INPUT_PULLUP);
    pinMode(KEY_SELECT, INPUT_PULLUP); pinMode(KEY_START, INPUT_PULLUP);
    BLEDevice::init(deviceName);
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pHIDService = pServer->createService(HID_SERVICE_UUID);
    
    BLECharacteristic* pHIDInfo = pHIDService->createCharacteristic("2A4A", BLECharacteristic::PROPERTY_READ);
    uint8_t hidInfo[] = {0x11, 0x01, 0x00, 0x01};
    pHIDInfo->setValue(hidInfo, 4);
    
    BLECharacteristic* pReportMap = pHIDService->createCharacteristic("2A4B", BLECharacteristic::PROPERTY_READ);
    uint8_t reportMap[] = {
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
        0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
        0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
        0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
        0xC0, 0xC0
    };
    pReportMap->setValue(reportMap, sizeof(reportMap));
    
    pHIDReport = pHIDService->createCharacteristic(HID_REPORT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR);
    pHIDReport->addDescriptor(new BLE2902());
    pHIDReport->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x2908)));
    pHIDReport->setValue(mouseReport, 4);
    
    BLECharacteristic* pControl = pHIDService->createCharacteristic("2A4C", BLECharacteristic::PROPERTY_WRITE_NR);
    uint8_t ctrl[1] = {0x00};
    pControl->setValue(ctrl, 1);
    
    pHIDService->start();
    BLEService *pBatteryService = pServer->createService(BATTERY_SERVICE_UUID);
    BLECharacteristic* pBatteryLevel = pBatteryService->createCharacteristic(BATTERY_LEVEL_UUID, BLECharacteristic::PROPERTY_READ);
    pBatteryLevel->setCallbacks(new BatteryLevelCallbacks()); pBatteryService->start();
    BLEService *pUARTService = pServer->createService(SERVICE_UUID);
    pUART = pUARTService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    pUART->setCallbacks(new UARTCallbacks()); pUART->addDescriptor(new BLE2902()); pUARTService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(HID_SERVICE_UUID);
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->setAppearance(0x05C0);
    pAdvertising->start();
    drawMainMenu(); playBeep(1000, 80);
    Serial.println("ESP32-Box BLE Started");
}

void loop() {
    unsigned long now = millis();
    
    if (screenState >= 8 && screenState <= 11) {
        handleClientButtonPress();
    } else if (screenState == 2) {
        handleButtonPress();
        
        if (!screenSleeping) {
            unsigned long inactiveTime = now - lastActivityTime;
            if (inactiveTime > 30000) {
                screenSleeping = true;
                currentBrightness = 255;
            } else if (inactiveTime > 25000) {
                int dimLevel = map(inactiveTime, 25000, 30000, 255, 30);
                if (dimLevel < 30) dimLevel = 30;
                currentBrightness = dimLevel;
            }
        }
    } else if (screenState == 4 || screenState == 14 || screenState == 15 || screenState == 16 || screenState == 17 || screenState == 18) {
        handleButtonPress();
    } else {
        handleButtonPress();
    }
    
    if (screenState == 12 || screenState == 13) {
        handleServerButtonPress();
    }
    if (!deviceConnected && oldDeviceConnected) {
        BLEDevice::startAdvertising();
        oldDeviceConnected = false;
    }
    if (deviceConnected && !oldDeviceConnected) { oldDeviceConnected = true; }
    
    if (screenSleeping) {
        int anyBtn = digitalRead(KEY_UP) | digitalRead(KEY_DOWN) | digitalRead(KEY_LEFT) | 
                     digitalRead(KEY_RIGHT) | digitalRead(KEY_A) | digitalRead(KEY_B) |
                     digitalRead(KEY_MENU) | digitalRead(KEY_OPTION) | digitalRead(KEY_SELECT) | digitalRead(KEY_START);
        if (anyBtn == LOW) {
            screenSleeping = false;
            currentBrightness = 255;
            lastActivityTime = now;
            drawMediaRemote();
        }
    }
    
    if (screenState == 2) {
        animationFrame++;
        
        if (animationFrame % 2 == 0) {
            drawScreenFlash();
        }
        
        if (animationFrame % 30 == 0 && !screenSleeping) {
            drawMediaRemote();
        }
        
        if (animationFrame % 60 == 0) {
            updateParticles();
        }
    }
    
    delay(10);
}
