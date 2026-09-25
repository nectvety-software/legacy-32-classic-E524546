#pragma once
#include <Arduino.h>
#include <map>
#include <string>
static inline std::map<std::string,std::string> &hostPrefs(){static std::map<std::string,std::string> s;return s;}
class Preferences {
 std::string prefix;
 std::string k(const char* key)const{return prefix+":"+key;}
public:
 bool begin(const char* ns,bool=false){prefix=ns;return true;}
 bool isKey(const char* key)const{return hostPrefs().count(k(key))>0;}
 String getString(const char* key,const String& d=String())const{
   auto it=hostPrefs().find(k(key));return it==hostPrefs().end()?d:String(it->second);
 }
 bool getBool(const char* key,bool d=false)const{
   auto it=hostPrefs().find(k(key));return it==hostPrefs().end()?d:it->second=="1";
 }
 uint32_t getUInt(const char* key,uint32_t d=0)const{
   auto it=hostPrefs().find(k(key));return it==hostPrefs().end()?d:std::stoul(it->second);
 }
 uint8_t getUChar(const char* key,uint8_t d=0)const {return uint8_t(getUInt(key,d));}
 uint16_t getUShort(const char* key,uint16_t d=0)const{return uint16_t(getUInt(key,d));}
 void putString(const char* key,const String& v){hostPrefs()[k(key)]=v.c_str();}
 void putBool(const char* key,bool v){hostPrefs()[k(key)]=v?"1":"0";}
 void putUInt(const char* key,uint32_t v){hostPrefs()[k(key)]=std::to_string(v);}
 void putUChar(const char* key,uint8_t v){putUInt(key,v);}
 void putUShort(const char* key,uint16_t v){putUInt(key,v);}
 void remove(const char* key){hostPrefs().erase(k(key));}
};
