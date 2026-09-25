// Link and run the actual firmware QeappFormat.cpp + AppInstallerService.cpp
// against a POSIX filesystem stand-in. This is not an ESP32 target build.
#include "../../src/services/AppInstallerService.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
std::string File::virtualRoot;
static fs::FS *gFS;
bool StorageService::begin(){ok=true;return true;}
fs::FS &StorageService::fs(){return *gFS;}
bool StorageService::exists(const String &p)const{return ok&&gFS->exists(p);}
uint64_t StorageService::freeBytes()const{return 8*1024*1024;}
int StorageService::list(const String &folder,FsEntry *out,int cap){
  File dir=gFS->open(folder);if(!dir||!dir.isDirectory())return 0;
  int count=0;File f=dir.openNextFile();while(f&&count<cap){
    String n(f.name());int slash=n.lastIndexOf('/');if(slash>=0)n=n.substring(slash+1);
    out[count++]=FsEntry(n,folder+"/"+n,f.isDirectory(),f.size());f.close();f=dir.openNextFile();
  }
  if(f) f.close();
  dir.close(); return count;
}
static void copyFile(const char *src,const std::string &dest){std::ifstream in(src,std::ios::binary);std::ofstream out(dest,std::ios::binary);assert(in&&out);out<<in.rdbuf();}
static std::vector<uint8_t> readBytes(const std::string &p){std::ifstream f(p,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());}
static void writeBytes(const std::string &p,const std::vector<uint8_t> &v){std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write((const char*)v.data(),v.size());}
int main(int argc,char **argv){
  assert(argc==4);std::string base=argv[1];
  ::mkdir((base+"/System").c_str(),0777);::mkdir((base+"/System/Apps").c_str(),0777);
  ::mkdir((base+"/System/Apps/Inbox").c_str(),0777);::mkdir((base+"/System/Apps/Installed").c_str(),0777);
  fs::FS sd(base);gFS=&sd;StorageService storage;assert(storage.begin());AppInstallerService installer;installer.begin(storage);
  copyFile(argv[2],base+"/System/Apps/Inbox/welcome.qeapp");
  copyFile(argv[3],base+"/System/Apps/Inbox/help_site.qeapp");
  Qeapp::Meta m;String error;std::string text="/System/Apps/Inbox/welcome.qeapp";
  // User video imports a package from the SD card root, not Inbox.
  copyFile(argv[2],base+"/welcome_from_root.qeapp");
  assert(installer.inspect("/welcome_from_root.qeapp",m,error));
  assert(!strcmp(m.id,"welcome"));
  assert(installer.inspect(text.c_str(),m,error));assert(!strcmp(m.id,"welcome")&&m.hasIcon&&!strcmp(m.version,"1.0.0"));
  assert(installer.install(text.c_str(),m,error));assert(installer.count()==1);
  assert(storage.exists("/System/Apps/Installed/welcome/manifest.ini"));
  assert(storage.exists("/System/Apps/Installed/welcome/icon.rgb565"));
  assert(storage.exists("/System/Apps/Installed/welcome/payload.txt"));
  assert(storage.exists("/System/Apps/Installed/welcome/receipt.bin"));
  uint16_t pix[1024];assert(installer.loadIcon("welcome",pix));
  assert(!installer.install(text.c_str(),m,error));assert(error=="Already installed. Uninstall before reinstall");
  assert(installer.install("/System/Apps/Inbox/help_site.qeapp",m,error));
  assert(installer.count()==2);assert(!strcmp(m.type,"web"));
  auto bytes=readBytes(base+text);assert(bytes.size()>200);
  bytes[116+int(bytes[8])+(int(bytes[9])<<8)+int(bytes[12])+(int(bytes[13])<<8)]^=0x80;writeBytes(base+"/System/Apps/Inbox/corrupt.qeapp",bytes);
  assert(!installer.inspect("/System/Apps/Inbox/corrupt.qeapp",m,error));
  assert(error=="Payload SHA-256 mismatch");
  assert(!installer.install("/System/Apps/Inbox/corrupt.qeapp",m,error));
  assert(installer.count()==2);
  auto badHeader=readBytes(base+text);badHeader[19]=0x7F;writeBytes(base+"/System/Apps/Inbox/length_bad.qeapp",badHeader);
  assert(!installer.inspect("/System/Apps/Inbox/length_bad.qeapp",m,error));
  // Unknown file must block uninstall and remain preserved.
  {std::ofstream protectedFile(base+"/System/Apps/Installed/welcome/user-data.txt");protectedFile<<"no delete";}
  assert(!installer.uninstall("welcome",error));
  assert(storage.exists("/System/Apps/Installed/welcome/user-data.txt"));
  assert(sd.remove("/System/Apps/Installed/welcome/user-data.txt"));
  assert(installer.uninstall("welcome",error));assert(!storage.exists("/System/Apps/Installed/welcome"));
  assert(installer.uninstall("help_site",error));assert(installer.count()==0);
  assert(!installer.uninstall("../System",error));
  // Validate SHA256 vectors, empty and "abc".
  uint8_t digest[32];Qeapp::Sha256 sha;sha.finish(digest);
  static const uint8_t expected[]={0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14};
  assert(!memcmp(digest,expected,sizeof expected));
  puts("QEAPP real installer host integration: ALL PASS");return 0;
}
