// Real QEAPP2 inspect against POSIX SD shim; deterministic tamper corpus.
#define main unused_installer_main
#include "../qeapp_host/test_installer.cpp"
#undef main
#include <filesystem>
#include <random>
int main(int argc,char **argv){
  assert(argc==3);
  namespace fx=std::filesystem;
  std::string root=argv[1];
  fx::create_directories(root+"/System/Apps/Inbox");
  fx::create_directories(root+"/System/Apps/Installed");
  fs::FS card(root);gFS=&card;StorageService storage;assert(storage.begin());
  AppInstallerService apps;apps.begin(storage);
  auto good=readBytes(argv[2]);assert(good.size()>100);
  const std::string pkg="/System/Apps/Inbox/tampered.qeapp";
  Qeapp::Meta m;String error;
  writeBytes(root+pkg,good);
  assert(apps.inspect(pkg.c_str(),m,error));
  std::mt19937 gen(0x2430ff);
  for(int i=0;i<1500;++i){
    auto b=good;
    size_t pos=gen()%b.size();
    b[pos]^=(uint8_t)(1u<<(gen()%8));
    writeBytes(root+pkg,b);
    assert(!apps.inspect(pkg.c_str(),m,error)); // all bytes are signed or hash protected
  }
  auto shortPkg=good;shortPkg.resize(5);
  writeBytes(root+pkg,shortPkg);assert(!apps.inspect(pkg.c_str(),m,error));
  assert(!apps.inspect("/System/Apps/../Inbox/tampered.qeapp",m,error));
  assert(!apps.inspect("/System/Apps/Inbox/tampered.exe",m,error));
  puts("PASS 1500 seeded signed-package single-bit mutations + truncation/path validation ASan/UBSan");
}
