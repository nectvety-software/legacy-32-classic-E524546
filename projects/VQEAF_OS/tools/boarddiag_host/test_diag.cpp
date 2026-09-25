#include "../../src/services/BoardDiagnostics.h"
#include "../../src/services/StorageService.h"
#include "../../src/services/TrustedTls.h"
#include "SD_MMC.h"
#include "WiFi.h"
#include <cassert>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <string>

std::string File::virtualRoot;
bool File::corruptNext = false;
SDMMCFS SD_MMC;
SerialClass Serial;
ESPClass ESP;
WiFiClass WiFi;
int main(int argc, char **argv) {
  assert(argc == 2);
  std::string root = argv[1];
  SD_MMC.setRoot(root);
  StorageService store;
  assert(store.begin());
  String info;
  assert(BoardDiagnostics::testSdReadWrite(store, info) == BoardDiagnostics::Result::Pass);
  assert(info.indexOf("4096 bytes") >= 0);
  assert(!SD_MMC.exists("/System/Temp/.s3_diag_scratch.bin"));
  // A stale collision must be reported and never overwritten/deleted.
  const std::string target = root + "/System/Temp/.s3_diag_scratch.bin";
  {std::ofstream f(target.c_str(),std::ios::binary); f << "USER DATA";}
  assert(BoardDiagnostics::testSdReadWrite(store,info)==BoardDiagnostics::Result::Inconclusive);
  {std::ifstream f(target.c_str(),std::ios::binary);std::string contents((std::istreambuf_iterator<char>(f)),{});assert(contents=="USER DATA");}
  assert(::unlink(target.c_str())==0);
  SD_MMC.inserted = false;
  assert(store.tick(true,16001)==StorageService::CardEvent::Removed);
  assert(BoardDiagnostics::testSdReadWrite(store,info)==BoardDiagnostics::Result::Inconclusive);
  assert(BoardDiagnostics::testTls("invalid host with space",true,info)==BoardDiagnostics::Result::Fail);
  // No network on this host: an inaccessible negative control cannot be
  // counted as evidence of a rejected TLS certificate.
  assert(BoardDiagnostics::testTls("self-signed.badssl.com",false,info)==BoardDiagnostics::Result::Inconclusive);
  puts("PASS: real scratch readback, data collision, offline SD, TLS no-network honesty");
}
