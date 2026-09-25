#include "../../src/services/BrowserService.h"
#include "FakeHttp.h"
#include "WiFi.h"
#include <cassert>
#include <cstdio>
uint32_t gFakeNow=0;
WiFiClass WiFi;
FakeHttpState gFakeHttp;
int main() {
  BrowserService browser;
  assert(browser.begin(nullptr));
  // Chunked page with an extension and a bounded, ignored trailer.
  gFakeHttp.reset(200,-1,"4;x=y\r\n<h1>\r\n4\r\nOK</\r\n4\r\nh1>!\r\n0\r\nX-Test: x\r\n\r\n");
  gFakeHttp.transferEncoding="chunked";
  assert(browser.load("https://example.org/chunked"));
  assert(!browser.hasPendingRetry() && browser.lineCount()>0);
  assert(String(browser.url())=="https://example.org/chunked");
  // Verify `getSize() == -1` with no Transfer-Encoding still loads raw HTML.
  gFakeHttp.transferEncoding="";
  gFakeHttp.reset(200,-1,"<html><body>Plain response</body></html>");
  assert(browser.load("https://example.org/raw"));
  // A clicked link lives in the browser's fixed pool; opening it must copy
  // its URL before resetting/reusing that same pool for the destination page.
  gFakeHttp.reset(200,-1,"<html><body><a href=\"https://example.org/linked\">Link</a></body></html>");
  assert(browser.load("https://example.org/source"));
  assert(browser.linkCount() > 0);
  gFakeHttp.reset(200,4,"next");
  assert(browser.openLink(0));
  assert(gFakeHttp.lastUrl == "https://example.org/linked");
  // Failure keeps the exact request (old code Reload reopened last success).
  gFakeHttp.reset(-1,0,"");
  assert(!browser.load("https://example.org/retry"));
  assert(browser.hasPendingRetry());
  assert(String(browser.url())=="https://example.org/retry");
  gFakeHttp.reset(200,3,"yay");
  assert(browser.reload());
  assert(gFakeHttp.lastUrl=="https://example.org/retry");
  assert(!browser.hasPendingRetry());
  // Malformed and oversized chunked responses fail closed, not rendered.
  gFakeHttp.transferEncoding="chunked";
  gFakeHttp.reset(200,-1,"5\r\nfoo\r\n0\r\n\r\n");
  assert(!browser.load("https://example.org/broken-chunk"));
  assert(String(browser.error())=="Malformed chunked response");
  gFakeHttp.reset(200,-1,"FFFF\r\n");
  assert(!browser.load("https://example.org/big-chunk"));
  assert(String(browser.error())=="Page exceeds 32 KB limit");
  puts("PASS browser: chunked, close-delimited, retry, malformed, bounds");
}
