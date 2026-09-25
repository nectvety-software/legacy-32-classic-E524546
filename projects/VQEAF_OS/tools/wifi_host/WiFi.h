#pragma once
#include "Arduino.h"
#include <vector>
struct FakeNetwork {String ssid;int rssi;uint8_t auth;};
static const uint8_t WIFI_AUTH_OPEN=0;
static const int WIFI_STA=1, WL_CONNECTED=3, WL_CONNECT_FAILED=4, WL_NO_SSID_AVAIL=1;
class WiFiClass {
public:
 int current=0, beginCalls=0, disconnectCalls=0, scanCalls=0, scanDeletes=0;
 bool autoReconnect=true, flashPersistent=true, forceScanError=false, neverFinish=false;
 String activeName, attemptedName, attemptedPassword;
 std::vector<FakeNetwork> items;
 uint32_t scanReadyAt=0;
 int status() const{return current;}
 String SSID() const {return activeName;}
 String SSID(int index) const{return items.at(index).ssid;}
 int RSSI(int index) const{return items.at(index).rssi;}
 uint8_t encryptionType(int index) const{return items.at(index).auth;}
 void mode(int){}
 void setAutoReconnect(bool val){autoReconnect=val;}
 void persistent(bool val){flashPersistent=val;}
 int scanNetworks(bool asynchronous=false,bool includeHidden=false){
   (void)asynchronous;(void)includeHidden;scanCalls++;
   scanReadyAt=millis()+1000;
   return forceScanError?-2:-1;
 }
 int scanComplete() const {
   if(forceScanError)return -2;
   if(neverFinish)return -1;
   return millis()<scanReadyAt?-1:(int)items.size();
 }
 void scanDelete(){scanDeletes++;}
 void begin(const char* ssid,const char* password=nullptr){
   beginCalls++; attemptedName=ssid;attemptedPassword=password?password:"";current=0;
 }
 void disconnect(){disconnectCalls++;current=0;activeName="";}
};
extern WiFiClass WiFi;
