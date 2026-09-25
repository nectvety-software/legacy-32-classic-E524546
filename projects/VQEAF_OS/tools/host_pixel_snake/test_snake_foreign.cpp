// Default v2.4 trust key MUST reject a package signed by the separate demo key.
#define main other_installer_main
#include "../qeapp_host/test_installer.cpp"
#undef main
#include <filesystem>
#include <cassert>
#include <cstring>
int main(int argc,char **argv){
 assert(argc==3);
 namespace fx=std::filesystem;
 const std::string root=argv[1];
 fx::create_directories(root+"/System/Apps/Inbox");
 fx::create_directories(root+"/System/Apps/Installed");
 fs::FS drive(root);gFS=&drive;StorageService storage;assert(storage.begin());
 AppInstallerService apps;apps.begin(storage);
 copyFile(argv[2],root+"/System/Apps/Inbox/snake.qeapp");
 Qeapp::Meta meta;String reason;
 assert(!apps.inspect("/System/Apps/Inbox/snake.qeapp",meta,reason));
 assert(std::string(reason.c_str()).rfind("Unknown signing key ID",0)==0); // v2.4.3 adds actionable key-profile diagnostics
 assert(apps.count()==0);
 puts("PASS default firmware rejects demo-signed game (pin isolation)");
}
