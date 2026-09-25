#pragma once
#include "Arduino.h"
#include "WiFiClient.h"
#include "WiFiClientSecure.h"
static const int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
static const int HTTPC_STRICT_FOLLOW_REDIRECTS=1;
class HTTPClient { WiFiClient stream; public:
  void useHTTP10(bool){} void setReuse(bool){} void collectHeaders(const char*[],size_t){}
  void setConnectTimeout(int){} void setTimeout(int){} void setFollowRedirects(int){} void setUserAgent(const char*){}
  bool begin(WiFiClient&, const char*){return true;} bool begin(WiFiClientSecure&, const char*){return true;}
  void addHeader(const char*, const char*){} int GET(){return 200;} int getSize()const{return 42;} String header(const char*)const{return String("text/html");}
  String getLocation()const{return String();} WiFiClient* getStreamPtr(){return &stream;} bool connected()const{return false;} int writeToStream(Stream*){return 42;} void end(){}
};
