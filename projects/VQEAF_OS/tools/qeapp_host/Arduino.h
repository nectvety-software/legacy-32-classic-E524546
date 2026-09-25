#pragma once
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#define FILE_READ "r"
#define FILE_WRITE "w"
class String {
  std::string s_;
public:
  String(){} String(const char *p):s_(p?p:""){} String(const std::string &s):s_(s){}
  String(const String &s)=default;String &operator=(const String &s)=default;
  String &operator=(const char *p){s_=p?p:"";return *this;}
  size_t length()const{return s_.size();}const char *c_str()const{return s_.c_str();}
  char operator[](size_t n)const{return n<s_.size()?s_[n]:0;}
  bool startsWith(const char *p)const{return s_.rfind(p?p:"",0)==0;}
  bool endsWith(const char *p)const{std::string t=p?p:"";return s_.size()>=t.size()&&s_.compare(s_.size()-t.size(),t.size(),t)==0;}
  void toLowerCase(){std::transform(s_.begin(),s_.end(),s_.begin(),[](unsigned char c){return (char)std::tolower(c);});}
  int lastIndexOf(char c)const{auto i=s_.find_last_of(c);return i==s_.npos?-1:int(i);}
  String substring(size_t i)const{return i<s_.size()?String(s_.substr(i)):String();}
  bool operator==(const String &v)const{return s_==v.s_;}
  bool operator==(const char *v)const{return s_==(v?v:"");}
  bool operator!=(const String &v)const{return s_!=v.s_;}
  bool operator!=(const char *v)const{return s_!=(v?v:"");}
  friend String operator+(const String &a,const String &b){return String(a.s_+b.s_);}
};
