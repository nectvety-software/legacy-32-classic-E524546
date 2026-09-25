#pragma once
#include "Arduino.h"
typedef uint8_t wifi_auth_mode_t;
static const wifi_auth_mode_t WIFI_AUTH_OPEN=0;
static const int WIFI_STA=1, WIFI_OFF=0, WL_CONNECTED=3, WL_NO_SSID_AVAIL=1, WL_CONNECT_FAILED=4;
class WiFiClass { public:
 int status() const{return 0;} void mode(int){} int getMode()const{return WIFI_STA;} void setAutoReconnect(bool){} void setHostname(const char*){}
 int scanNetworks(bool=false,bool=false){return 0;} int scanComplete(){return 0;} String SSID(int=0)const{return String("ssid");} int32_t RSSI(int=0)const{return -50;} wifi_auth_mode_t encryptionType(int)const{return WIFI_AUTH_OPEN;} void scanDelete(){}
 void persistent(bool){} void begin(const char*){} void begin(const char*,const char*){} void disconnect(){} IPAddress localIP()const{return IPAddress();} IPAddress subnetMask()const{return IPAddress();} IPAddress gatewayIP()const{return IPAddress();} IPAddress dnsIP(int=0)const{return IPAddress();} String macAddress()const{return String("00:11:22:33:44:55");}
 int hostByName(const char*, IPAddress &ip){ip=IPAddress();return 1;}
};
extern WiFiClass WiFi;
