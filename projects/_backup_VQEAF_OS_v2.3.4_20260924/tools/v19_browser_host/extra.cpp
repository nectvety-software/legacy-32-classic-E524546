#include "../../src/services/StorageService.h"
#include "SD_MMC.h"
SDMMCFS SD_MMC;
bool StorageService::recoverAtomicFile(const String&){return false;}
bool StorageService::writeAtomic(const String&,const uint8_t*,size_t){return false;}
int StorageService::pruneFlatDirectory(const String&,int,uint64_t){return -1;}
fs::FS &StorageService::fs(){return SD_MMC;}
bool StorageService::ensureDir(const String&){return false;}
bool StorageService::exists(const String&) const{return false;}
