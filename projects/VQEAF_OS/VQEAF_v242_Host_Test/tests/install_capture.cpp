// Compile/link the ACTUAL QEAPP/2 installer, signature and POSIX filesystem shim.
#define main base_installer_test_main
#include "test_installer.cpp"
#undef main
#include <iostream>
#include <fstream>
#include <filesystem>
int main(int argc, char **argv) {
  assert(argc==4);
  const std::string root=argv[1];
  namespace pf=std::filesystem;
  pf::create_directories(root+"/System/Apps/Inbox");
  pf::create_directories(root+"/System/Apps/Installed");
  pf::create_directories(root+"/System/Apps/Data");
  fs::FS sd(root);gFS=&sd;
  StorageService storage;assert(storage.begin());
  AppInstallerService installer;installer.begin(storage);
  Qeapp::Meta pkg;String err;
  copyFile(argv[2],root+"/System/Apps/Inbox/welcome.qeapp");
  copyFile(argv[3],root+"/System/Apps/Inbox/help_site.qeapp");
  assert(installer.inspect("/System/Apps/Inbox/welcome.qeapp",pkg,err));
  assert(std::string(pkg.id)=="welcome" && std::string(pkg.version)=="1.0.0");
  assert(installer.install("/System/Apps/Inbox/welcome.qeapp",pkg,err));
  assert(installer.inspect("/System/Apps/Inbox/help_site.qeapp",pkg,err));
  assert(installer.install("/System/Apps/Inbox/help_site.qeapp",pkg,err));
  assert(installer.count()==2);
  assert(sd.exists("/System/Apps/Installed/welcome/receipt.bin"));
  assert(sd.exists("/System/Apps/Installed/help_site/receipt.bin"));
  for (const char* id:{"welcome","help_site"}){
    Qeapp::Meta installed;String problem;
    assert(installer.get(id,installed));
    std::cout<<"VERIFIED_INSTALLED|"<<id<<"|"<<installed.name<<"|"<<installed.version<<"|"<<installed.type<<"\n";
  }
  // Reboot-like re-create the installer. Persistent SD receipt must be verified.
  AppInstallerService rebooted; rebooted.begin(storage);
  assert(rebooted.count()==2);
  std::cout<<"REBOOT_CATALOG|"<<rebooted.count()<<"\n";
  // A corrupted copy is intentionally rejected with no extra installed apps.
  auto corrupt=readBytes(root+"/System/Apps/Inbox/welcome.qeapp");
  assert(corrupt.size()>130);corrupt[119]^=1;
  writeBytes(root+"/System/Apps/Inbox/tampered.qeapp",corrupt);
  assert(!rebooted.inspect("/System/Apps/Inbox/tampered.qeapp",pkg,err));
  assert(rebooted.count()==2);
  std::cout<<"CORRUPT_REJECTED|1\n";
  return 0;
}
