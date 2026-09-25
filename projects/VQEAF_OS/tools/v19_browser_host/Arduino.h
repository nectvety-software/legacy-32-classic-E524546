#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>
using std::size_t;
using std::min; using std::max;
#define PROGMEM
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define FILE_READ "r"
#define FILE_WRITE "w"
#define F(x) x
#define portTICK_PERIOD_MS 1
extern uint32_t gFakeNow; inline uint32_t millis(){return gFakeNow;} inline void delay(uint32_t ms){gFakeNow+=ms;} inline void pinMode(int,int){} inline int digitalRead(int){return HIGH;} inline void digitalWrite(int,int){} inline void analogWrite(int,int){}
template <class T, class A, class B> inline T constrain(T x,A a,B b){T aa=(T)a, bb=(T)b; return x<aa?aa:(x>bb?bb:x);} inline long map(long x,long in_min,long in_max,long out_min,long out_max){return (x-in_min)*(out_max-out_min)/(in_max-in_min)+out_min;}
class String {
  std::string s;
public:
  String(){} String(const char* c):s(c?c:""){} String(const std::string& v):s(v){} String(char c):s(1,c){}
  String(int v):s(std::to_string(v)){} String(unsigned int v):s(std::to_string(v)){} String(long v):s(std::to_string(v)){} String(unsigned long v):s(std::to_string(v)){} String(long long v):s(std::to_string(v)){} String(unsigned long long v):s(std::to_string(v)){}
  size_t length() const{return s.size();} const char* c_str() const{return s.c_str();} char operator[](size_t i) const { return i<s.size()?s[i]:0; } long toInt() const { try{return std::stol(s);}catch(...){return 0;} }
  String substring(int a) const { if(a<0)a=0; if((size_t)a>s.size())a=s.size(); return String(s.substr(a)); }
  String substring(int a,int b) const { if(a<0)a=0; if(b<a)b=a; return String(s.substr((size_t)a,(size_t)(b-a))); }
  bool startsWith(const char* p) const { return s.rfind(p?p:"",0)==0; }
  bool endsWith(const char* p) const { std::string q=p?p:""; return s.size()>=q.size() && s.compare(s.size()-q.size(),q.size(),q)==0; }
  int lastIndexOf(char c) const { auto p=s.find_last_of(c); return p==std::string::npos?-1:(int)p; }
  int compareTo(const String&o) const { return s.compare(o.s); }
  void toLowerCase(){ std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);}); }
  void reserve(size_t n){s.reserve(n);}
  void trim(){ size_t a=0,b=s.size(); while(a<b && std::isspace((unsigned char)s[a]))++a; while(b>a && std::isspace((unsigned char)s[b-1]))--b; s=s.substr(a,b-a); }
  int indexOf(const char* p) const { auto n=s.find(p?p:""); return n==std::string::npos?-1:(int)n; }
  int indexOf(char c) const { auto n=s.find(c); return n==std::string::npos?-1:(int)n; }
  void remove(size_t idx){ if(idx<s.size())s.erase(idx); }
  String& operator=(const char*c){s=c?c:"";return *this;} String& operator+=(const String&o){s+=o.s;return *this;} String& operator+=(const char*c){s+=c?c:"";return *this;} String& operator+=(char c){s+=c;return *this;}
  friend String operator+(const String&a,const String&b){return String(a.s+b.s);} friend String operator+(const String&a,const char*b){return String(a.s+(b?b:""));} friend String operator+(const char*a,const String&b){return String((a?a:"")+b.s);} friend String operator+(const String&a,int b){return a+String(b);} friend String operator+(const String&a,unsigned int b){return a+String(b);} friend String operator+(const String&a,unsigned long b){return a+String(b);} 
  bool operator==(const String&o)const{return s==o.s;} bool operator==(const char*c)const{return s==(c?c:"");} bool operator!=(const String&o)const{return s!=o.s;} bool operator!=(const char*c)const{return !(*this==c);} 
};
inline bool operator==(const char*a,const String&b){return b==a;} inline bool operator!=(const char*a,const String&b){return !(b==a);}
class Stream { public: virtual ~Stream(){} };
class IPAddress { public: String toString() const {return String("0.0.0.0");} };
class SerialClass { public: void begin(int){} template<class...A> void printf(const char*,A...){ } template<class T> void print(const T&){} template<class T> void println(const T&){} void println(){} };
extern SerialClass Serial;
class ESPClass { public: uint32_t getFreeHeap() const{return 200000;} uint32_t getFreePsram() const{return 8000000;} uint32_t getMinFreeHeap() const{return 100000;} uint32_t getPsramSize() const{return 8000000;} void restart(){} };
extern ESPClass ESP;
