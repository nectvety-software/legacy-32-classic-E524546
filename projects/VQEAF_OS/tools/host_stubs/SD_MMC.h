#define CARD_NONE 0
#pragma once
#include "FS.h"
class SDMMCFS: public fs::FS { public:
 int cardType(){return 1;} void end(){}
 void setPins(int,int,int,int=-1,int=-1,int=-1){}
 bool begin(const char* mountpoint = "/sdcard", bool mode1 = true, bool mode2 = false, int freq = 20000, int maxFiles = 8){ (void)mountpoint;(void)mode1;(void)mode2;(void)freq;(void)maxFiles; return true; }
 uint64_t totalBytes()const{return 8000000000ULL;} uint64_t usedBytes()const{return 100000000ULL;} uint64_t cardSize()const{return totalBytes();}
};
extern SDMMCFS SD_MMC;
