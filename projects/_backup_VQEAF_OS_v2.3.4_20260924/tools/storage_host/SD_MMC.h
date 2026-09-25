#define CARD_NONE 0
#pragma once
#include "FS.h"
class SDMMCFS : public fs::FS {
 public:
  bool failTempFinalize = false;
  bool inserted = true;
  bool mountSucceeds = true;
  int cardType() { return inserted ? 1 : CARD_NONE; }
  void end() {}
  void setPins(int,int,int,int=-1,int=-1,int=-1){}
  bool begin(const char*,bool,bool,int,int){return inserted && mountSucceeds;}
  bool rename(const String &src,const String &dst){
   if(failTempFinalize && src.endsWith(".tmp")) {failTempFinalize=false;return false;}
   return fs::FS::rename(src,dst);
  }
  uint64_t totalBytes() const{return 8ull*1024*1024*1024;}
  uint64_t usedBytes() const{return 100ull*1024*1024;}
  uint64_t cardSize() const{return inserted ? totalBytes() : 0;}
};
extern SDMMCFS SD_MMC;
