#pragma once
#include "Arduino.h"
#define FILE_APPEND "a"
namespace fs { class FS; }
class File : public Stream { public:
 operator bool()const{return true;} bool isDirectory()const{return false;} const char* name()const{return "file";} uint64_t size()const{return 0;} File openNextFile(){return File();} void close(){} void flush(){}
 size_t read(uint8_t*,size_t){return 0;} size_t read(char*,size_t){return 0;} size_t readBytes(char*,size_t){return 0;} int read(){return -1;} bool available()const{return false;} bool seek(uint32_t){return true;} uint32_t position()const{return 0;}
 size_t write(const uint8_t*,size_t n){return n;} size_t print(const String&s){return s.length();} size_t print(const char*s){return s?strlen(s):0;}
};
namespace fs { class FS { public: File open(const String&, const char* = FILE_READ){return File();} bool exists(const String&){return false;} bool remove(const String&){return true;} bool mkdir(const String&){return true;} bool rmdir(const String&){return true;} bool rename(const String&,const String&){return true;} }; }
