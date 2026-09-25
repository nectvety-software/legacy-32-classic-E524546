#include "../../src/services/BrowserService.h"
#include "FakeHttp.h"
#include "WiFi.h"
#include <cassert>
#include <cstdio>
uint32_t gFakeNow=0;
WiFiClass WiFi;
FakeHttpState gFakeHttp;
int main(){
 BrowserService browser;
 assert(browser.begin(nullptr));
 // Dead remote peer after HTTP headers must no longer spin forever.
 gFakeNow=0;gFakeHttp.reset(200,-1,"",true);
 assert(!browser.load("https://example.org/a"));
 assert(gFakeNow>=8000&&gFakeNow<=8100);
 assert(String(browser.error())=="Page request timeout");
 // Non-2xx statuses may not create a valid-looking cached page.
 gFakeNow=0;gFakeHttp.reset(404,0,"");
 assert(!browser.load("https://example.org/404"));
 assert(String(browser.error())=="HTTP status 404");
 assert(gFakeNow==0);
 // Reject oversized bodies before entering stream loop or allocating more RAM.
 gFakeHttp.reset(200,60000,"");
 assert(!browser.load("https://example.org/big"));
 assert(String(browser.error())=="Page exceeds 32 KB limit");
 // Interrupted, declared-length page must not be rendered as success.
 gFakeHttp.reset(200,12,"abc");
 assert(!browser.load("https://example.org/broken"));
 assert(String(browser.error())=="Page truncated");
 // Complete response still renders correctly using the normal bounded parser.
 const std::string html="<html><title>OK</title><body>Ready</body></html>";
 gFakeHttp.reset(200,(int)html.size(),html);
 assert(browser.load("https://example.org/success"));
 assert(browser.status()==200&&browser.lineCount()>0);
 // A TLS/network failure must report an error, not silently claim success.
 gFakeHttp.reset(-1,0,"");
 assert(!browser.load("https://example.org/tlsfailure"));
 assert(String(browser.error())=="HTTPS verification or network failed");
 puts("v2.0 simulated HTTPS/network failure + v1.9 stream regression: PASS");
}
