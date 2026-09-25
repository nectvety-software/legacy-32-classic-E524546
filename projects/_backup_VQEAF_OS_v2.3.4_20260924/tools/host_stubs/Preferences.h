#pragma once
#include "Arduino.h"
class Preferences { public:
 bool begin(const char*,bool=false){return true;} bool isKey(const char*)const{return false;}
 String getString(const char*,const String& d=String())const{return d;}
 bool getBool(const char*,bool d=false)const{return d;}
 uint32_t getUInt(const char*,uint32_t d=0)const{return d;}
 uint8_t getUChar(const char*,uint8_t d=0)const{return d;}
 uint16_t getUShort(const char*,uint16_t d=0)const{return d;}
 void putString(const char*,const String&){} void putBool(const char*,bool){} void putUInt(const char*,uint32_t){}
 void putUChar(const char*,uint8_t){} void putUShort(const char*,uint16_t){} void remove(const char*){}
};
