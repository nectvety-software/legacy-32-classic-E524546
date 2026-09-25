#pragma once
#include "Arduino.h"
#include "FakeHttp.h"
class WiFiClient : public Stream {
public:
 bool connect(const char*,int,int=0){return true;}
 bool connected()const{return gFakeHttp.stuck||gFakeHttp.offset<gFakeHttp.body.size();}
 size_t available(){return gFakeHttp.body.size()-gFakeHttp.offset;}
 int read(){return available()?(unsigned char)gFakeHttp.body[gFakeHttp.offset++]:-1;}
 int readBytes(char *dst,size_t n){
  size_t take=min(n,available());memcpy(dst,gFakeHttp.body.data()+gFakeHttp.offset,take);gFakeHttp.offset+=take;return (int)take;
 }
 int readBytes(uint8_t *dst,size_t n){return readBytes((char*)dst,n);}
 void stop(){}
 template<class T> void print(const T&){}
};
