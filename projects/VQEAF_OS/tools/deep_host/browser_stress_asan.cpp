// Host-only deterministic long-navigation stress using the actual browser implementation.
#include "../../src/services/BrowserService.h"
#include "../v19_browser_host/FakeHttp.h"
#include "WiFi.h"
#include <cassert>
#include <cstdio>
uint32_t gFakeNow=0;
WiFiClass WiFi;
FakeHttpState gFakeHttp;
int main(){
  BrowserService browser;
  assert(browser.begin(nullptr));
  for(int i=0;i<2000;++i){
    char url[120];std::snprintf(url,sizeof url,"https://example.org/page/%d",i);
    gFakeHttp.transferEncoding="";
    gFakeHttp.reset(200,4,"test");
    assert(browser.load(url));
    assert(browser.lineCount() <= BrowserService::MAX_LINES);
    assert(browser.linkCount() <= BrowserService::MAX_LINKS);
    assert(!browser.hasPendingRetry());
    if(i%5==0){gFakeHttp.reset(200,4,"test");assert(browser.reload());}
    if(i%127==0){assert(browser.begin(nullptr));} // repeated begin must reuse pools
  }
  puts("PASS 2000 HTTP navigation + 400 reloads + 16 begin/resets, ASan+UBSan leak-check");
}
