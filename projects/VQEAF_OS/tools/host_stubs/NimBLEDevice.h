#pragma once
#include "Arduino.h"
class NimBLEAddress { public: std::string toString()const{return "00:00:00:00:00:00";} };
class NimBLEAdvertisedDevice { public: bool haveName()const{return false;} std::string getName()const{return "";} NimBLEAddress getAddress()const{return NimBLEAddress();} int getRSSI()const{return -50;} };
class NimBLEScanResults { public: int getCount()const{return 0;} const NimBLEAdvertisedDevice* getDevice(int)const{static NimBLEAdvertisedDevice d; return &d;} };
class NimBLEScan { public: void setActiveScan(bool){} void setInterval(int){} void setWindow(int){} NimBLEScanResults getResults(int,bool){return NimBLEScanResults();} void clearResults(){} };
class NimBLEDevice { public: static void init(const char*){} static bool deinit(bool=true){return true;} static NimBLEScan* getScan(){static NimBLEScan s;return &s;} };
