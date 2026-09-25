#include "QeappDataService.h"

const char *QeappDataService::filename(Slot slot) const {
  switch(slot){
    case Slot::Prefs:return "prefs.bin";
    case Slot::State:return "state.bin";
    case Slot::Draft:return "draft.bin";
  }
  return nullptr;
}
String QeappDataService::directory(const String &id) const {
  return String(StoragePaths::APPS_DATA)+"/"+id;
}
bool QeappDataService::authorized(const String &id, String &error) const {
  if(!card||!apps||!card->mounted()){error="Storage or catalog unavailable";return false;}
  Qeapp::Meta signedApp;
  if(!apps->get(id,signedApp)){error="App is not installed or signature invalid";return false;}
  return true;
}
bool QeappDataService::save(const String &id,Slot slot,const uint8_t *data,size_t size,String &error){
  if(!authorized(id,error))return false;
  const char *file=filename(slot);
  if(!file||!data||size==0||size>MAX_SLOT_BYTES){error="Invalid slot or data length";return false;}
  const String dir=directory(id), path=dir+"/"+file;
  fs::FS &fs=card->fs();
  if(!card->ensureDir(StoragePaths::APPS_DATA)||!card->ensureDir(dir)){
    error="Cannot create app data directory";return false;
  }
  File existing=fs.open(path,FILE_READ);
  uint64_t previous=0;
  if(existing){
    if(existing.isDirectory()){existing.close();error="Invalid data file";return false;}
    previous=existing.size();existing.close();
  }
  uint64_t total=0;
  for(int i=0;i<3;i++){
    File f=fs.open(dir+"/"+filename(static_cast<Slot>(i)),FILE_READ);
    if(f){
      if(f.isDirectory()){f.close();error="Invalid data layout";return false;}
      total+=f.size();f.close();
    }
  }
  if(total-previous+size>MAX_APP_BYTES){error="Application data quota exceeded";return false;}
  if(!card->writeAtomic(path,data,size)){error="App data atomic save failed";return false;}
  error="";return true;
}
bool QeappDataService::load(const String &id,Slot slot,uint8_t *buffer,size_t capacity,
                            size_t &used,String &error){
  used=0;
  if(!authorized(id,error))return false;
  const char *file=filename(slot);
  if(!file||!buffer){error="Invalid read slot/buffer";return false;}
  File f=card->fs().open(directory(id)+"/"+file,FILE_READ);
  if(!f||f.isDirectory()){
    if(f)f.close();
    error="App data slot is empty";return false;
  }
  if(f.size()>MAX_SLOT_BYTES||f.size()>capacity){f.close();error="Data exceeds read buffer";return false;}
  used=size_t(f.size());bool ok=f.read(buffer,used)==(int)used;f.close();
  if(!ok){used=0;error="App data truncated";return false;}
  error="";return true;
}
bool QeappDataService::purge(const String &id,String &error){
  // Purge is possible only while the same signed app is still installed;
  // orphan data after uninstall is intentionally left for manual file manager
  // recovery (never delete user files implicitly).
  if(!authorized(id,error))return false;
  const String dir=directory(id);
  if(!card->exists(dir)){error="";return true;}
  File folder=card->fs().open(dir,FILE_READ);
  if(!folder||!folder.isDirectory()){
    if(folder)folder.close();
    error="Invalid app data directory";return false;
  }
  File item=folder.openNextFile();bool safe=true;
  while(item){
    String name(item.name());int slash=name.lastIndexOf('/');
    if(slash>=0)name=name.substring(slash+1);
    if(item.isDirectory()||!(name=="prefs.bin"||name=="state.bin"||name=="draft.bin")){
      safe=false;item.close();break;
    }
    item.close();item=folder.openNextFile();
  }
  folder.close();
  if(!safe){error="Unknown app data present; manual cleanup required";return false;}
  const char *files[]={"prefs.bin","state.bin","draft.bin"};
  fs::FS &fs=card->fs();
  for(const char *file:files){String path=dir+"/"+file;
    if(fs.exists(path)&&!fs.remove(path)){error="Cannot remove app data file";return false;}
  }
  if(!fs.rmdir(dir)){error="Cannot remove app data directory";return false;}
  error="";return true;
}
