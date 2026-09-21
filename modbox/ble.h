#ifndef BLE_H
#define BLE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEClient.h>

//////////////// BLE UUIDs ////////////////
#define BLE_SERVICE_HID       "1812"
#define BLE_SERVICE_BATTERY   "180F"
#define BLE_SERVICE_UART      "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_UART_RX      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_UART_TX      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_CHAR_BATTERY      "2A19"

//////////////// BUZZER ////////////////
#ifndef BUZZER
#define BUZZER 41
#endif

//////////////// BLE State ////////////////
enum BLEMode { BLE_IDLE, BLE_SERVER, BLE_CLIENT };
enum ControlMode { MODE_REMOTE, MODE_MOUSE, MODE_BOTH };
enum BLEMenuState { BLE_MAIN, BLE_CONTROL, BLE_SCAN, BLE_CLIENT_MENU, BLE_SERVER_MENU, BLE_SETTINGS, BLE_KEYBOARD, BLE_VMOUSE };

//////////////// BLE Global Variables ////////////////
extern BLEMode bleMode;
extern ControlMode controlMode;
extern BLEMenuState bleMenuState;
extern int bleCursor;
extern bool bleConnected;
extern bool bleAdvertising;
extern bool clientConnected;
extern int mouseSpeed;
extern bool clickSound;
extern bool backlightState;
extern bool airMouseMode;
extern bool remoteModeActive;
extern bool continuousMove;
extern String deviceName;
extern String scanResult;
extern String clientData;
extern String serverLog;

#define MAX_FOUND_DEVICES 20

extern int foundDevices;
extern int selectedDevice;
extern bool scanning;
extern String devNames[MAX_FOUND_DEVICES];
extern int devRSSI[MAX_FOUND_DEVICES];
extern String devAddrs[MAX_FOUND_DEVICES];

//////////////// BLE Functions ////////////////
void bleInit();
void bleStartServer(const char* serviceUUID);
void bleStartClient();
bool bleConnectToDevice(const String& address, const char* serviceUUID);
bool bleClientConnectByAddress(const String& address, const String& serviceUUID);
void bleClientDisconnect();
bool bleClientIsConnected();
void bleSendData(const String& data);
void bleStopAll();
void bleScan(int duration);
String bleGetConnectedDevice();
void sendMouseReport(int8_t x, int8_t y, int8_t wheel, uint8_t buttons);
void sendMediaKey(uint8_t key);
void sendConsumerKey(uint16_t key);
void bleProcessCommand(String cmd);
void bleMouseControl();
void bleShowNotification(String msg);
void bleUpdateNotification();

////////////// Virtual Mouse Functions ////////////////
void bleSendVMouseMove(int16_t x, int16_t y);
void bleSendVMouseClick(uint8_t button);
void bleSendVMouseScroll(int8_t scroll);
void bleSendVMouseShow();
void bleSendVMouseHide();
void bleSendVMousePos(int16_t x, int16_t y);

//////////////// BLE UI Functions ////////////////
void bleDrawMain();
void bleDrawControl();
void bleDrawScan();
void bleDrawClient();
void bleDrawServer();
void bleDrawSettings();
void bleDrawKeyboard();
void bleInputHandler();
void appBLE();

#endif
