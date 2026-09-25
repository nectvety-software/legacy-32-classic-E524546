// Compile ACTUAL firmware installer and actual P-256 signature verifier with
// the demonstration public key, against the existing POSIX SD-card mock.
#define main other_installer_main
#include "../qeapp_host/test_installer.cpp"
#undef main
#include <filesystem>
#include <cassert>
#include <cstring>
int main(int argc,char **argv){
 assert(argc==3);
 namespace fsys=std::filesystem;
 const std::string root=argv[1];
 fsys::create_directories(root+"/System/Apps/Inbox");
 fsys::create_directories(root+"/System/Apps/Installed");
 fs::FS fs(root);gFS=&fs;
 StorageService store;assert(store.begin());
 AppInstallerService apps;apps.begin(store);
 const char *path="/System/Apps/Inbox/snake_pixel_demo.qeapp";
 copyFile(argv[2],root+path);
 Qeapp::Meta meta;String error;
 assert(apps.inspect(path,meta,error));
 assert(!strcmp(meta.id,"snake_pixel")&&!strcmp(meta.type,"text")&&meta.hasIcon);
 assert(apps.install(path,meta,error));
 assert(apps.count()==1&&apps.get("snake_pixel",meta));
 uint16_t icon[1024];assert(apps.loadIcon("snake_pixel",icon));
 std::ifstream payload(root+"/System/Apps/Installed/snake_pixel/payload.txt",std::ios::binary);
 std::string config((std::istreambuf_iterator<char>(payload)),std::istreambuf_iterator<char>());
 assert(config.find("VQEAF-SNAKE-1\n")==0);
 // The signed installed receipt must reject a changed payload.
 std::string installed=root+"/System/Apps/Installed/snake_pixel/payload.txt";
 auto bytes=readBytes(installed);assert(bytes.size()>20);bytes[18]^=0x01;writeBytes(installed,bytes);
 assert(!apps.get("snake_pixel",meta));
 // A damaged source package must not be installed either.
 auto pkg=readBytes(root+path);pkg[pkg.size()-1]^=0x01;
 writeBytes(root+"/System/Apps/Inbox/tampered.qeapp",pkg);
 assert(!apps.inspect("/System/Apps/Inbox/tampered.qeapp",meta,error));
 puts("PASS: signed snake QEAPP/2 inspect, installed receipt, icon, tamper rejection");
}
