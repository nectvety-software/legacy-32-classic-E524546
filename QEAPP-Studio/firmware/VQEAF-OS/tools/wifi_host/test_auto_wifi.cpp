#include <cassert>
#include <cstdio>
#include <vector>
#include "../../src/services/WiFiConnectionService.h"
uint32_t gWifiFakeMs = 200;
WiFiClass WiFi;
static void tick(WiFiConnectionService &s, uint32_t ms) { gWifiFakeMs += ms; s.poll(); }
static void resetRF() { WiFi=WiFiClass(); gWifiFakeMs=200; }
static void discovered(WiFiConnectionService &s) { s.poll(); tick(s, 1050); }
static void finishJoin(WiFiConnectionService &s) {
  WiFi.current=WL_CONNECTED; WiFi.activeName=WiFi.attemptedName;
  s.poll();
}
int main() {
  // Strongest saved AP may appear beyond the 16 networks retained for UI.
  resetRF();
  WiFiProfileStore save; save.begin();
  save.saveProfile("Weak", "weakpass", false);
  save.saveProfile("Strong", "strongpass", false);
  save.setLastSSID("Weak");
  for (int i=0;i<22;++i) WiFi.items.push_back({String("Noise")+String(i), -20+(i%3), 1});
  WiFi.items.push_back({"Weak",-79,1});
  WiFi.items.push_back({"Strong",-43,1});
  WiFiConnectionService svc; svc.begin(save); svc.configureAuto(true);
  assert(svc.autoPhase()==WiFiConnectionService::AutoPhase::Scheduled);
  discovered(svc);
  assert(svc.autoPhase()==WiFiConnectionService::AutoPhase::Connecting);
  assert(WiFi.attemptedName=="Strong");
  assert(WiFi.attemptedPassword=="strongpass");
  assert(svc.autoCandidateCount()==2);
  assert(WiFi.disconnectCalls==0);
  finishJoin(svc);
  assert(svc.autoPhase()==WiFiConnectionService::AutoPhase::Online);
  assert(save.lastSSID()=="Strong");
  assert(svc.phase()==WiFiConnectionService::Phase::Connected);
  bool result; assert(!svc.takeConnectionResult(result));
  printf("PASS: strongest of entire boot scan wins; no duplicate success result / NVS rewrite\n");

  // Drop for >=5s: rescan, try strongest candidate and then fallback if rejected.
  WiFi.current=0; WiFi.activeName="";
  tick(svc,100); tick(svc,5100);
  assert(svc.autoPhase()==WiFiConnectionService::AutoPhase::Scanning);
  WiFi.items.erase(WiFi.items.end()-1); // remove Strong => Weak only among saved
  discovered(svc);
  assert(WiFi.attemptedName=="Weak");
  finishJoin(svc);
  assert(save.lastSSID()=="Weak");
  printf("PASS: dropout triggers rescan after debounce and changes AP when stronger is absent\n");

  resetRF();
  WiFiProfileStore prefs; prefs.begin();
  prefs.saveProfile("Second", "secondpass", false);
  prefs.saveProfile("First", "firstpass", false);
  prefs.setLastSSID("Second");
  WiFi.items={{"First",-28,1},{"Second",-64,1}};
  WiFiConnectionService failover; failover.begin(prefs); failover.configureAuto(true);
  discovered(failover);
  assert(WiFi.attemptedName=="First");
  assert(prefs.lastSSID()=="Second"); // first attempt not yet verified
  WiFi.current=WL_CONNECT_FAILED;
  tick(failover,3100);
  assert(WiFi.attemptedName=="Second");
  assert(prefs.lastSSID()=="Second");
  finishJoin(failover);
  assert(failover.autoPhase()==WiFiConnectionService::AutoPhase::Online);
  printf("PASS: failed strongest WPA association falls back; unsuccessful SSID not persisted\n");

  // Last-good SSID has the deterministic tie-break but no preference over RSSI.
  resetRF();
  WiFiProfileStore ties; ties.begin();
  ties.saveProfile("B", "12345678", false);
  ties.saveProfile("A", "12345678", false);
  ties.setLastSSID("B");
  WiFi.items={{"A",-57,1},{"B",-57,1}};
  WiFiConnectionService tie; tie.begin(ties); tie.configureAuto(true);
  discovered(tie); assert(WiFi.attemptedName=="B");
  printf("PASS: equal signal tie prefers last known successful AP\n");

  // A saved security type must match the scanned AP variant exactly.
  resetRF();
  WiFiProfileStore auth; auth.begin();
  auth.saveProfile("Secure", "abcdefgh", false);
  WiFi.items={{"Secure",-15,WIFI_AUTH_OPEN}};
  WiFiConnectionService type; type.begin(auth); type.configureAuto(true);
  discovered(type);
  assert(type.autoCandidateCount()==0);
  assert(WiFi.beginCalls==0);
  assert(type.retrySeconds()>=29 && type.retrySeconds()<=30);
  printf("PASS: wrong auth variant is not auto-joined\n");

  // Empty or out of-range scan backs off 30, then 60, 120 seconds.
  WiFi.items.clear();
  tick(type,30001);tick(type,1100);
  assert(type.autoPhase()==WiFiConnectionService::AutoPhase::Waiting);
  assert(type.retrySeconds()>=59 && type.retrySeconds()<=60);
  type.configureAuto(false);
  assert(type.autoPhase()==WiFiConnectionService::AutoPhase::Disabled);
  const int scansBefore=WiFi.scanCalls;
  tick(type,120000);assert(WiFi.scanCalls==scansBefore);
  printf("PASS: bounded retry/backoff; Settings off cancels scheduled scans\n");

  // Opening WiFi Settings during a background scan hands it to the UI;
  // deliberate disconnect must not start another automatic scan.
  resetRF();
  WiFiProfileStore manualSaved; manualSaved.begin(); manualSaved.saveProfile("AP", "password123", false);
  WiFi.items={{"AP",-44,1}};
  WiFiConnectionService manual; manual.begin(manualSaved); manual.configureAuto(true);
  manual.poll();
  assert(manual.autoPhase()==WiFiConnectionService::AutoPhase::Scanning);
  assert(manual.scan());
  assert(manual.autoPhase()==WiFiConnectionService::AutoPhase::Paused);
  tick(manual,1100);
  assert(manual.count()==1);
  assert(WiFi.beginCalls==0);
  // Returning from the WiFi list resumes auto selection unless the user
  // explicitly joined/disconnected an AP.
  manual.resumeAutoAfterBrowsing();
  assert(manual.autoPhase()==WiFiConnectionService::AutoPhase::Scheduled);
  tick(manual,1100); // launches scan
  tick(manual,1100); // scans, then joins AP
  assert(WiFi.attemptedName=="AP");
  manual.disconnect();
  const int scanCount=WiFi.scanCalls, beginCount=WiFi.beginCalls;
  tick(manual,120000);
  assert(WiFi.scanCalls==scanCount && WiFi.beginCalls==beginCount);
  printf("PASS: view-only WiFi scan resumes on exit; explicit disconnect blocks retry\n");

  // Already-associated browser session is respected, not torn down for ranking.
  resetRF();
  WiFi.current=WL_CONNECTED;WiFi.activeName="AP";
  WiFiConnectionService keep; keep.begin(manualSaved);keep.configureAuto(true);keep.poll();
  assert(keep.autoPhase()==WiFiConnectionService::AutoPhase::Online);
  assert(WiFi.scanCalls==0 && WiFi.disconnectCalls==0);
  printf("PASS: existing browser WLAN survives boot auto manager startup\n");

  // A direct Shell 'wifi off/scan/connect' must stop an in-flight boot scan
  // before it takes control of the same WiFi driver.
  resetRF();
  WiFiConnectionService shell; shell.begin(manualSaved); shell.configureAuto(true);
  shell.poll();assert(shell.phase()==WiFiConnectionService::Phase::Scanning);
  shell.externalOverride();
  assert(shell.autoPhase()==WiFiConnectionService::AutoPhase::Paused);
  assert(shell.phase()==WiFiConnectionService::Phase::Idle);
  const int shellScans=WiFi.scanCalls;
  tick(shell,300000);
  assert(WiFi.scanCalls==shellScans && WiFi.beginCalls==0);
  printf("PASS: Shell shared-radio override stops competing boot scan\n");

  // Safe Mode: configuring disabled leaves network completely untouched.
  resetRF();
  WiFiConnectionService safe; safe.begin(manualSaved);safe.configureAuto(false);
  tick(safe,300000);
  assert(!WiFi.scanCalls && !WiFi.beginCalls);
  printf("PASS: Safe Mode / disabled preference does not scan or auto-join\n");
  printf("PASS: 10 auto WiFi integration scenarios\n");
}
