#pragma once
#include "Arduino.h"
#include "WiFiClient.h"
#include "WiFiClientSecure.h"
#include "FakeHttp.h"
static const int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
class HTTPClient {
 WiFiClient stream;
public:
 void setConnectTimeout(int){} void setTimeout(int){} void setFollowRedirects(int){}
 void setUserAgent(const char*){} void collectHeaders(const char*[],size_t){}
 bool begin(WiFiClient&,const char*){return true;}
 bool begin(WiFiClientSecure&,const char*){return true;}
 void addHeader(const char*,const char*){}
 int GET(){return gFakeHttp.status;}
 int getSize()const{return gFakeHttp.size;}
 String getLocation()const{return String();}
 String header(const char*)const{return String("text/html");}
 WiFiClient *getStreamPtr(){return &stream;}
 bool connected()const{return stream.connected();}
 void end(){}
};
