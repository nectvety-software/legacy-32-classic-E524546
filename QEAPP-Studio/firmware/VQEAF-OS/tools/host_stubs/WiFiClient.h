#pragma once
#include "Arduino.h"
class WiFiClient : public Stream { public:
  bool connect(const char*, int, int=0){return true;} bool connected() const{return false;} size_t available(){return 0;}
  int read(){return -1;} int readBytes(char*, size_t){return 0;} int readBytes(uint8_t*, size_t){return 0;} void stop(){}
  template<class T> void print(const T&){}
};
