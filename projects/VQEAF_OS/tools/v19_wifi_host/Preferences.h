#pragma once
#include "Arduino.h"
#include <map>
#include <string>
namespace FakeNvs {
 extern std::map<std::string,String> values;
 extern int writes;
}
class Preferences {
 std::string ns;
 std::string key(const char *k)const{return ns+":"+(k?k:"");}
 public:
  bool begin(const char *s,bool=false){ns=s?s:"";return true;}
  String getString(const char *k,const String &d=String())const{
    auto it=FakeNvs::values.find(key(k));return it==FakeNvs::values.end()?d:it->second;
  }
  bool getBool(const char *k,bool d=false)const{
    auto v=getString(k,d?"1":"0");return v=="1";
  }
  uint8_t getUChar(const char *k,uint8_t d=0)const{
    auto it=FakeNvs::values.find(key(k));return it==FakeNvs::values.end()?d:(uint8_t)it->second.toInt();
  }
  void putString(const char *k,const String &v){FakeNvs::values[key(k)]=v;FakeNvs::writes++;}
  void putBool(const char *k,bool v){putString(k,v?"1":"0");}
  void putUChar(const char *k,uint8_t v){putString(k,String((int)v));}
  void remove(const char *k){FakeNvs::values.erase(key(k));FakeNvs::writes++;}
};
