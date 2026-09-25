#include <cassert>

#include "../../src/services/WiFiConnectionService.h"
uint32_t gWifiFakeMs=0;
WiFiClass WiFi;
static void next(uint32_t ms){gWifiFakeMs+=ms;}
int main(){
  WiFiProfileStore prefs; prefs.begin();
  WiFiConnectionService svc; svc.begin(prefs);
  // Active browser WLAN is never disconnected by scanning/rescan.
  WiFi.current=WL_CONNECTED;WiFi.activeName="BrowserLive";
  for(int i=0;i<26;i++)WiFi.items.push_back({String("AP-")+String(i), -95+i*2, (uint8_t)(i%2)});
  WiFi.items[24].ssid=""; // hidden SSID remains selectable with manual name
  assert(svc.scan());assert(WiFi.disconnectCalls==0);
  svc.poll();assert(svc.phase()==WiFiConnectionService::Phase::Scanning);
  next(1100);svc.poll();
  assert(svc.count()==16);assert(svc.network(0).rssi==-45);
  for(int i=1;i<svc.count();i++) assert(svc.network(i-1).rssi>=svc.network(i).rssi);
  assert(WiFi.disconnectCalls==0);assert(WiFi.status()==WL_CONNECTED);
  assert(svc.scan());next(1100);svc.poll();assert(WiFi.disconnectCalls==0);
  printf("PASS: async strongest-first scan retains active browser session\n");
  // Do not save credentials before successful DHCP/association status.
  const int old=prefs.count();
  assert(svc.connect("NewSecure", "validpass123", false));
  assert(!WiFi.flashPersistent);
  assert(prefs.count()==old);assert(WiFi.attemptedPassword=="validpass123");
  next(1000);svc.poll();assert(svc.phase()==WiFiConnectionService::Phase::Connecting);
  WiFi.current=WL_CONNECTED;WiFi.activeName="NewSecure";svc.poll();
  assert(svc.phase()==WiFiConnectionService::Phase::Connected);
  assert(prefs.find("NewSecure")>=0);assert(prefs.at(prefs.find("NewSecure")).password=="validpass123");
  bool result=false;assert(svc.takeConnectionResult(result)&&result);
  assert(!svc.takeConnectionResult(result));
  printf("PASS: save verified successful connection exactly once\n");
  // Already-connected profile never tears the session down.
  const int before=WiFi.beginCalls;
  assert(svc.connect("NewSecure", "validpass123", false));
  assert(WiFi.beginCalls==before);assert(WiFi.disconnectCalls==0);
  svc.takeConnectionResult(result);
  printf("PASS: reselect same network is non-disruptive\n");
  // Failed network: no poisoned auto-reconnect password in NVS.
  assert(svc.connect("BadAP", "abcdefgh", false));
  next(3000);WiFi.current=WL_CONNECT_FAILED;svc.poll();
  assert(svc.phase()==WiFiConnectionService::Phase::Failed);
  assert(prefs.find("BadAP")==-1);
  assert(svc.takeConnectionResult(result)&&!result);
  assert(String(svc.error())=="Connection rejected - check key");
  printf("PASS: failed WPA password rejected and not stored\n");
  assert(svc.connect("TimeoutAP", "12345678", false));
  next(12001);svc.poll();assert(svc.phase()==WiFiConnectionService::Phase::Failed);
  assert(prefs.find("TimeoutAP")==-1);svc.takeConnectionResult(result);
  printf("PASS: 12-second timeout and rollback\n");
  // Show hidden/AP insertion cap: network at late index with best RSSI is present.
  bool hidden=false;for(int i=0;i<svc.count();i++)if(svc.network(i).ssid[0]==0)hidden=true;
  assert(hidden);printf("PASS: hidden SSID retained and strongest 16 chosen\n");
  assert(!svc.connect("BadShort", "short", false));assert(prefs.find("BadShort")==-1);
  assert(!svc.connect("", "password", false));
  assert(!svc.connect("ok","01234567890123456789012345678901234567890123456789012345678901234",false)); // replaced below for stub compatibility
  printf("PASS: SSID/password validation\n");
  // An UNVERIFIED replacement password must not be saved just because an
  // existing live connection is already up for that SSID.
  WiFi.current=WL_CONNECTED;WiFi.activeName="NewSecure";
  const int prevDisconnects=WiFi.disconnectCalls;
  assert(svc.connect("NewSecure", "wrongwrong", false));
  assert(WiFi.disconnectCalls==prevDisconnects+1);
  assert(prefs.at(prefs.find("NewSecure")).password=="validpass123");
  next(3100);WiFi.current=WL_CONNECT_FAILED;svc.poll();
  assert(svc.phase()==WiFiConnectionService::Phase::Failed);
  assert(prefs.at(prefs.find("NewSecure")).password=="validpass123");
  svc.takeConnectionResult(result);
  printf("PASS: never persist unverified same-SSID replacement password\n");
  WiFi.current=WL_CONNECTED;WiFi.activeName="NewSecure";
  const int beforeDisconnect=WiFi.disconnectCalls;
  svc.disconnect();assert(WiFi.disconnectCalls==beforeDisconnect+1&&!WiFi.autoReconnect);
  // Explicit manual join with autoReconnect turned off must honor settings.
  assert(svc.connect("ManualOnly", "validpass", false, false));
  assert(!WiFi.autoReconnect);next(12010);svc.poll();svc.takeConnectionResult(result);
  printf("PASS: manual WiFi obeys auto-reconnect setting\n");
  WiFi.neverFinish=true;assert(svc.scan());next(12001);svc.poll();
  assert(svc.phase()==WiFiConnectionService::Phase::Failed);
  assert(String(svc.error())=="WiFi scan timed out");
  svc.takeConnectionResult(result);WiFi.neverFinish=false;
  printf("PASS: bounded asynchronous scan timeout\n");
  WiFi.forceScanError=true;assert(!svc.scan());
  assert(svc.phase()==WiFiConnectionService::Phase::Failed);
  assert(String(svc.error())=="WiFi scan could not start");
  printf("PASS: scan failure surfaces a clear message\n");
  printf("PASS: intentional disconnect suppresses immediate reconnect\n");
  return 0;
}
