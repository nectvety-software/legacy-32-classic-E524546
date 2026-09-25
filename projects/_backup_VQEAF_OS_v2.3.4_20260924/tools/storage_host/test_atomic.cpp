#include "../../src/services/StorageService.h"
#include "SD_MMC.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
std::string File::virtualRoot;
bool File::corruptNext=false;
SDMMCFS SD_MMC;
static void mk(const std::string &path){assert(::mkdir(path.c_str(),0777)==0);}
static std::string get(const std::string &path){std::ifstream f(path,std::ios::binary);return std::string((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());}
static void put(const std::string &path,const char *s){std::ofstream f(path,std::ios::binary);f<<s;}
int main(int argc,char **argv){
 assert(argc==2);std::string root=argv[1];
 SD_MMC.setRoot(root);mk(root+"/System");mk(root+"/System/Cache");mk(root+"/System/Cache/Web");
 StorageService storage;assert(storage.begin());
 const String name="/System/Cache/Web/test.htm";
 auto write=[&](const char *s){return storage.writeAtomic(name,(const uint8_t*)s,strlen(s));};
 assert(write("A"));assert(get(root+std::string(name.c_str()))=="A");
 assert(write("BBB"));assert(get(root+std::string(name.c_str()))=="BBB");
 assert(!SD_MMC.exists(name+".bak")&&!SD_MMC.exists(name+".tmp"));
 SD_MMC.failTempFinalize=true;
 assert(!write("CCC")); // injected failure after backup rename
 assert(get(root+std::string(name.c_str()))=="BBB");
 assert(!SD_MMC.exists(name+".bak")&&!SD_MMC.exists(name+".tmp"));
 // A card that silently corrupts a staged write must not replace the last good file.
 File::injectCorruption();
 assert(!write("CORRUPT"));
 assert(get(root+std::string(name.c_str()))=="BBB");
 assert(!SD_MMC.exists(name+".tmp"));
 assert(SD_MMC.rename(name,name+".bak")); // simulate power loss after move to backup
 put(root+std::string(name.c_str())+".tmp","part");
 assert(storage.recoverAtomicFile(name));
 assert(get(root+std::string(name.c_str()))=="BBB");
 assert(!SD_MMC.exists(name+".bak")&&!SD_MMC.exists(name+".tmp"));
 put(root+std::string(name.c_str())+".bak","OLD"); // crash just after committing new file
 assert(storage.recoverAtomicFile(name));
 assert(get(root+std::string(name.c_str()))=="BBB");
 assert(!SD_MMC.exists(name+".bak"));
 assert(!storage.recoverAtomicFile("/../oops"));
 assert(!storage.recoverAtomicFile("relative"));
 // Old implementation inspected only maxFiles+8 entries. Exercise a 36-file
 // directory and verify a full streaming count before bounded deletion.
 for(int i=0;i<35;i++){char f[60];snprintf(f,sizeof f,"/System/Cache/Web/cache%03d.htm",i);put(root+f,"hi");}
 const int removed=storage.pruneFlatDirectory(StoragePaths::CACHE_WEB,16,512*1024);
 assert(removed==20);
 File dir=SD_MMC.open(StoragePaths::CACHE_WEB,FILE_READ);
 int remaining=0;File f=dir.openNextFile();while(f){if(!f.isDirectory())remaining++;f.close();f=dir.openNextFile();}
 assert(remaining==16);
 assert(storage.pruneFlatDirectory("/Documents",1,1)==-1);
 // Streaming clear removes more than the old 32-entry stack listing.
 for(int i=0;i<29;i++){char f[64];snprintf(f,sizeof f,"/System/Cache/Web/extra%03d.htm",i);put(root+f,"hi");}
 assert(storage.clearFlatDirectory(StoragePaths::CACHE_WEB)==45);
 assert(storage.clearFlatDirectory(StoragePaths::CACHE_WEB)==0);
 assert(storage.clearFlatDirectory("/Documents")==-1);
 // Live file guards: probing/remount is never attempted while a player holds a File.
 SD_MMC.inserted=false;
 assert(storage.tick(false,16000)==StorageService::CardEvent::None);
 assert(storage.mounted());
 assert(storage.tick(true,16000)==StorageService::CardEvent::Removed);
 assert(!storage.mounted());
 assert(storage.tick(true,20000)==StorageService::CardEvent::None);
 SD_MMC.inserted=true; SD_MMC.mountSucceeds=false;
 assert(storage.tick(true,21001)==StorageService::CardEvent::None);
 SD_MMC.mountSucceeds=true;
 assert(storage.tick(true,30000)==StorageService::CardEvent::None);
 assert(storage.tick(true,37001)==StorageService::CardEvent::Mounted);
 assert(storage.mounted());
 assert(storage.mediaPresent());
 // Boot without a card may recover later without rebooting the whole OS.
 SD_MMC.inserted=false;
 StorageService late;
 assert(!late.begin());
 SD_MMC.inserted=true;
 assert(late.tick(true,15001)==StorageService::CardEvent::Mounted);
 assert(late.mounted());
 puts("v2.0 atomic readback, removal, bounded remount, no-card boot: PASS");
}
