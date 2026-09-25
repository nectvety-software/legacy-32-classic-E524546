// Native host negative-security regression: exercises exact firmware verifier,
// SD install transaction, receipt-based launch integrity checks.
#define main qeapp_v14_basic_unused
#include "test_installer.cpp"
#undef main
#include <filesystem>

int main(int argc,char **argv) {
  assert(argc==4); // temp root, trusted signed text package, other-key signed text
  namespace st=std::filesystem;
  std::string base=argv[1];
  st::create_directories(base+"/System/Apps/Inbox");
  st::create_directories(base+"/System/Apps/Installed");
  fs::FS sd(base);gFS=&sd;StorageService storage;assert(storage.begin());
  AppInstallerService apps;apps.begin(storage);Qeapp::Meta meta;String error;
  auto good=readBytes(argv[2]), badKey=readBytes(argv[3]);
  assert(good.size()>Qeapp::HEADER_BYTES+Qeapp::SIGNATURE_BYTES+1);
  int mlen=good[8]|(good[9]<<8), ilen=good[12]|(good[13]<<8);
  auto path=[&](const char *name) {return base+"/System/Apps/Inbox/"+name;};
  auto probe=[&](const char *name,const std::vector<uint8_t> &bytes,const char *why){
    writeBytes(path(name),bytes);
    assert(!apps.inspect(String("/System/Apps/Inbox/")+name,meta,error));
    assert(std::string(error.c_str()).find(why)!=std::string::npos);
    assert(!apps.install(String("/System/Apps/Inbox/")+name,meta,error));
    assert(apps.count()==0);
  };
  // A forged header declaring fresh hashes still fails public-key verification.
  std::vector<uint8_t> mutate=good;
  mutate[116+mlen+ilen]^=0x7f;
  probe("bad_payload.qeapp",mutate,"SHA-256");
  mutate=good;
  mutate[116+mlen+ilen]^=0x7f;
  Qeapp::Sha256 sha;sha.update(mutate.data()+116+mlen+ilen,
       mutate.size()-116-mlen-ilen-Qeapp::SIGNATURE_BYTES);
  uint8_t forged[32];sha.finish(forged);memcpy(mutate.data()+84,forged,32);
  probe("forge_hash.qeapp",mutate,"Digital signature");
  mutate=good;mutate[mutate.size()-1]^=0x80;
  probe("bad_sig.qeapp",mutate,"Digital signature");
  mutate=good;mutate[mutate.size()-Qeapp::SIGNATURE_BYTES+8]^=0x20;
  probe("unknown_key.qeapp",mutate,"Unknown signing key");
  mutate=good;mutate.resize(mutate.size()-Qeapp::SIGNATURE_BYTES);
  probe("missing_sig.qeapp",mutate,"length");
  mutate=good;mutate[5]='1';mutate.resize(mutate.size()-Qeapp::SIGNATURE_BYTES);
  probe("legacy.qeapp",mutate,"Unsigned QEAPP/1");
  probe("other_publisher.qeapp",badKey,"Digital signature");
  // Verify exact bytes of installed payload at point of launch, not only install.
  const std::string goodOnSd="/System/Apps/Inbox/good.qeapp";
  writeBytes(base+goodOnSd,good);
  // Explicit File Manager imports are allowed from any absolute card path,
  // but must not accept traversal or Windows-style separator tricks.
  assert(!apps.inspect("/System/Apps/Inbox/../Inbox/good.qeapp",meta,error));
  assert(error=="Expected safe absolute .qeapp file path");
  assert(!apps.inspect("/System/Apps/Inbox/\\good.qeapp",meta,error));
  assert(error=="Expected safe absolute .qeapp file path");
  assert(apps.inspect(goodOnSd.c_str(),meta,error));
  assert(apps.install(goodOnSd.c_str(),meta,error));
  assert(apps.count()==1);
  assert(apps.get("welcome",meta));
  const std::string installed=base+"/System/Apps/Installed/welcome/";
  mutate=readBytes(installed+"payload.txt");mutate[0]^=0x01;writeBytes(installed+"payload.txt",mutate);
  assert(!apps.get("welcome",meta));
  apps.refresh();assert(apps.count()==0);
  // Restore original payload; changing receipt signature still blocks trust.
  writeBytes(installed+"payload.txt",std::vector<uint8_t>(good.begin()+116+mlen+ilen,good.end()-Qeapp::SIGNATURE_BYTES));
  mutate=readBytes(installed+"receipt.bin");assert(mutate.size()==192);mutate.back()^=0x08;
  writeBytes(installed+"receipt.bin",mutate);
  apps.refresh();assert(apps.count()==0);
  puts("QEAPP/2 signature, tampering, unsigned and installed receipt gates: PASS");
  return 0;
}
