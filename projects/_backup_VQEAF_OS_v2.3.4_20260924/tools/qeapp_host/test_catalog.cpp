// Additional POSIX integration/limit suite; reuse the standalone storage mock.
#define main qeapp_base_integration_main
#include "test_installer.cpp"
#undef main
#include <filesystem>
int main(int argc,char **argv) {
  assert(argc==15); // program, temporary microSD root, fourteen app packages
  const std::string root=argv[1];
  namespace st=std::filesystem;
  st::create_directories(root+"/System/Apps/Inbox");
  st::create_directories(root+"/System/Apps/Installed");
  fs::FS sd(root);gFS=&sd;
  StorageService storage; assert(storage.begin());
  AppInstallerService apps;apps.begin(storage);
  Qeapp::Meta meta; String error;
  // Invalid manifest and duplicate keys must not result in installed entries.
  const char badId[]="id=../x\nname=Bad\nversion=1.0\ntype=text\n";
  const char missing[]="id=bad\nname=Bad\nversion=1.0\n";
  const char duplicate[]="id=bad\nid=bad\nname=Bad\nversion=1.0\ntype=text\n";
  const char version[]="id=bad\nname=Bad\nversion=..\ntype=text\n";
  const char insecure[]="id=bad\nname=Bad\nversion=1.0\ntype=web\nentry=http://example.org/\n";
  const char *reason="";Qeapp::Meta parsed;
  assert(!Qeapp::parseManifest(badId,sizeof badId-1,parsed,reason));
  assert(!Qeapp::parseManifest(missing,sizeof missing-1,parsed,reason));
  assert(!Qeapp::parseManifest(duplicate,sizeof duplicate-1,parsed,reason));
  assert(!Qeapp::parseManifest(version,sizeof version-1,parsed,reason));
  assert(!Qeapp::parseManifest(insecure,sizeof insecure-1,parsed,reason));
  // Invalid header lengths and magic must be rejected before allocating.
  uint8_t h[Qeapp::HEADER_BYTES]={0};Qeapp::Header hdr;
  assert(!Qeapp::parseHeader(h,sizeof h,hdr,reason));
  memcpy(h,"QEAPP2\r\n",8);h[8]=0xff;h[9]=0xff;
  assert(!Qeapp::parseHeader(h,sizeof h,hdr,reason));
  // Exercise fixed catalog limit with thirteen independently valid packages.
  for(int i=0;i<12;++i) {
    std::string path="/System/Apps/Inbox/app"+std::to_string(i)+".qeapp";
    copyFile(argv[i+2],root+path);
    assert(apps.inspect(path.c_str(),meta,error));
    assert(apps.install(path.c_str(),meta,error));
    assert(apps.count()==i+1);
  }
  const std::string extra="/System/Apps/Inbox/app12.qeapp";
  copyFile(argv[14],root+extra);
  assert(!apps.install(extra.c_str(),meta,error));
  assert(error=="App catalog full (12 apps)");
  assert(!storage.exists("/System/Apps/Installed/app12"));
  assert(apps.uninstall("app0",error));
  assert(apps.install(extra.c_str(),meta,error));
  assert(apps.count()==12);
  // Do not overwrite existing app IDs (even when full).
  assert(!apps.install(extra.c_str(),meta,error));
  assert(error=="Already installed. Uninstall before reinstall");
  // Verify every catalog ID maps to an available validated launch record.
  for(int i=0;i<apps.count();++i){Qeapp::Meta entry;
    assert(apps.get(apps.at(i).info.id,entry));
    assert(entry.id[0]&&entry.name[0]&&entry.version[0]);
  }
  puts("QEAPP catalog bounds + parser negative suite: ALL PASS");
  return 0;
}
