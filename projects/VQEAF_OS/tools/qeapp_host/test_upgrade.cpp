// Genuine QEAPP/2 installer / rollback / app-data source against POSIX FS mock.
#define main qeapp_basic_unused
#include "test_installer.cpp"
#undef main
#include "../../src/services/QeappDataService.h"
#include "../../src/services/QeappVersion.h"
#include <filesystem>

bool StorageService::ensureDir(const String &p){
  if(gFS->exists(p)) {File f=gFS->open(p);return f&&f.isDirectory();}
  return gFS->mkdir(p);
}
bool StorageService::writeAtomic(const String &path,const uint8_t *data,size_t len){
  if(!len)return false;
  String tmp=path+".tmp",bak=path+".bak";
  File to=gFS->open(tmp,FILE_WRITE);
  if(!to)return false;
  bool wrote=to.write(data,len)==len;to.flush();to.close();
  if(!wrote){gFS->remove(tmp);return false;}
  File check=gFS->open(tmp,FILE_READ);
  std::vector<uint8_t> read(len);
  bool ok=check&&check.read(read.data(),len)==(int)len&&memcmp(read.data(),data,len)==0;
  if(check)check.close();
  if(!ok){gFS->remove(tmp);return false;}
  bool had=gFS->exists(path);
  if(had&&!gFS->rename(path,bak))return false;
  if(!gFS->rename(tmp,path)){
    if(had)gFS->rename(bak,path);
    return false;
  }
  if(had)gFS->remove(bak);
  return true;
}

int main(int argc,char **argv){
  assert(argc==5);
  namespace st=std::filesystem;
  std::string base=argv[1];
  st::create_directories(base+"/System/Apps/Inbox");
  st::create_directories(base+"/System/Apps/Installed");
  st::create_directories(base+"/System/Apps/Data");
  assert(Qeapp::compareVersion("1.2.0","1.2")==0);
  assert(Qeapp::compareVersion("1.09.1","1.10")<0);
  assert(Qeapp::compareVersion("1.10","1.9.999")>0);
  assert(Qeapp::compareVersion("2.0","1.9999999999999999")>0);
  fs::FS sd(base);gFS=&sd;StorageService card;assert(card.begin());
  AppInstallerService installer;installer.begin(card);
  QeappDataService data;data.begin(card,installer);
  std::string path=base+"/System/Apps/Inbox/";
  copyFile(argv[2],path+"v1.qeapp");
  copyFile(argv[3],path+"v2.qeapp");
  copyFile(argv[4],path+"v3.qeapp");
  Qeapp::Meta m;String err;
  const String inbox="/System/Apps/Inbox/";
  assert(installer.install(inbox+"v1.qeapp",m,err));
  assert(std::strcmp(m.version,"1.0.0")==0);
  const uint8_t prefs[]={0,1,2,3,4};
  assert(data.save("welcome",QeappDataService::Slot::Prefs,prefs,sizeof(prefs),err));
  size_t used=0;uint8_t read[32]={};
  assert(data.load("welcome",QeappDataService::Slot::Prefs,read,sizeof(read),used,err));
  assert(used==sizeof(prefs)&&!memcmp(read,prefs,sizeof(prefs)));
  assert(!data.save("other",QeappDataService::Slot::State,prefs,sizeof(prefs),err));
  assert(!data.save("welcome",QeappDataService::Slot::State,prefs,20*1024,err));
  assert(!installer.install(inbox+"v1.qeapp",m,err));
  struct Progress {uint8_t last;int calls;bool ordered;} progress={0,0,true};
  installer.setProgressCallback([](uint8_t pct,const char*,void *ptr){
    Progress *p=static_cast<Progress*>(ptr);
    if(pct<p->last)p->ordered=false;
    p->last=pct;p->calls++;
  },&progress);
  assert(installer.install(inbox+"v2.qeapp",m,err));
  installer.setProgressCallback(nullptr,nullptr);
  assert(progress.ordered&&progress.last==100&&progress.calls>=5);
  assert(installer.count()==1&&!strcmp(m.version,"1.1.0"));
  assert(!installer.updateAvailable(m));
  assert(!installer.install(inbox+"v1.qeapp",m,err)); // rollback policy blocks downgrade
  assert(installer.get("welcome",m)&&!strcmp(m.version,"1.1.0"));
  assert(data.load("welcome",QeappDataService::Slot::Prefs,read,sizeof(read),used,err));
  assert(!memcmp(read,prefs,sizeof(prefs)));
  const std::string installed=base+"/System/Apps/Installed/welcome";
  const std::string backup=base+"/System/Apps/Installed/.backup-welcome";
  const std::string stage=base+"/System/Apps/Installed/.stage-welcome";
  st::create_directories(base+"/snapshots");
  st::copy(installed,base+"/snapshots/v2",st::copy_options::recursive);
  // SD FAT failure between moving the old app into backup and activating
  // staging: the installer must immediately restore the trusted old version.
  sd.failNextRenameFrom("/System/Apps/Installed/.stage-welcome");
  assert(!installer.install(inbox+"v3.qeapp",m,err));
  assert(installer.get("welcome",m)&&!strcmp(m.version,"1.1.0"));
  assert(!st::exists(backup)&&!st::exists(stage));
  // Simulate reset after old trusted version moved to backup but new stage
  // is partial: boot recovery must restore v2, never activate partial stage.
  st::rename(installed,backup);
  st::create_directory(stage);
  std::ofstream(stage+"/manifest.ini")<<"truncated";
  installer.refresh();
  assert(installer.recoveryStats().restored==1);
  assert(installer.recoveryStats().discardedStages==1);
  assert(!st::exists(stage)&&!st::exists(backup));
  assert(installer.get("welcome",m)&&!strcmp(m.version,"1.1.0"));
  // A valid new version plus a leftover old signed backup must be finalized
  // without uninstalling or replacing the newer trusted package.
  assert(installer.install(inbox+"v3.qeapp",m,err));
  assert(!strcmp(m.version,"1.2.0"));
  st::copy(base+"/snapshots/v2",backup,st::copy_options::recursive);
  installer.refresh();
  assert(installer.recoveryStats().finalized==1);
  assert(!st::exists(backup));
  assert(installer.get("welcome",m)&&!strcmp(m.version,"1.2.0"));
  // A corrupted activated version is replaced by a complete old signed
  // backup; a future boot cannot launch damaged bytes through the catalog.
  st::copy(base+"/snapshots/v2",backup,st::copy_options::recursive);
  auto bytes=readBytes(installed+"/payload.txt");assert(!bytes.empty());bytes[0]^=0x3f;
  writeBytes(installed+"/payload.txt",bytes);
  std::ofstream(installed+"/personal.txt")<<"keep me";
  installer.refresh();
  assert(installer.recoveryStats().blocked>=1);
  assert(st::exists(backup)&&st::exists(installed+"/personal.txt"));
  assert(!installer.get("welcome",m));
  st::remove(installed+"/personal.txt");
  installer.refresh();
  assert(installer.recoveryStats().restored==1);
  assert(installer.get("welcome",m)&&!strcmp(m.version,"1.1.0"));
  // Explicit data deletion while installed, never an implicit uninstall side effect.
  std::ofstream(base+"/System/Apps/Data/welcome/unknown.dat")<<"keep me";
  assert(!data.purge("welcome",err));
  assert(st::exists(base+"/System/Apps/Data/welcome/unknown.dat"));
  st::remove(base+"/System/Apps/Data/welcome/unknown.dat");
  assert(data.purge("welcome",err));
  assert(data.save("welcome",QeappDataService::Slot::State,prefs,sizeof(prefs),err));
  assert(installer.uninstall("welcome",err));
  assert(st::exists(base+"/System/Apps/Data/welcome/state.bin")); // uninstall preserves data
  assert(installer.install(inbox+"v3.qeapp",m,err));
  assert(data.load("welcome",QeappDataService::Slot::State,read,sizeof(read),used,err));
  assert(used==sizeof(prefs)&&!memcmp(read,prefs,sizeof(prefs)));
  assert(data.purge("welcome",err));
  assert(installer.uninstall("welcome",err));
  puts("QEAPP/2 V2.4 update/rollback/orphan stage, quota and app data: ALL PASS");
}
