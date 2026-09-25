#include "services/BrowserService.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
  char b[192];
  assert(BrowserService::normalizeUrl("example.org/news", b, sizeof b));
  assert(!strcmp(b, "https://example.org/news"));
  assert(BrowserService::normalizeUrl("example.org:8443/a", b, sizeof b));
  assert(!strcmp(b, "https://example.org:8443/a"));
  assert(BrowserService::normalizeUrl("HTTPS://EXAMPLE.ORG/abc", b, sizeof b));
  assert(!strcmp(b, "HTTPS://EXAMPLE.ORG/abc"));
  assert(!BrowserService::normalizeUrl("javascript:alert(1)", b, sizeof b));
  assert(!BrowserService::normalizeUrl("file:///secret", b, sizeof b));
  assert(!BrowserService::normalizeUrl("https://", b, sizeof b));
  assert(!BrowserService::normalizeUrl("https://bad.org/\nX-Evil: yes", b, sizeof b));
  String huge("https://example.org/"); for(int i=0;i<300;i++) huge+="x";
  assert(!BrowserService::normalizeUrl(huge,b,sizeof b));
  assert(!BrowserService::normalizeUrl("example.org", b, 12));

  assert(BrowserService::resolveUrl("https://example.org/a/b?x=1", "../page.html?q=2#id", b, sizeof b));
  assert(!strcmp(b,"https://example.org/page.html?q=2#id"));
  assert(BrowserService::resolveUrl("https://example.org?old=1", "?fresh=1", b, sizeof b));
  assert(!strcmp(b,"https://example.org/?fresh=1"));
  assert(BrowserService::resolveUrl("https://example.org/a/b?old=1", "?fresh=1", b, sizeof b));
  assert(!strcmp(b,"https://example.org/a/b?fresh=1"));
  assert(BrowserService::resolveUrl("https://example.org/a/b", "/root/../new", b, sizeof b));
  assert(!strcmp(b,"https://example.org/new"));
  assert(BrowserService::resolveUrl("https://example.org/a/b", "//other.org/hello", b, sizeof b));
  assert(!strcmp(b,"https://other.org/hello"));
  assert(BrowserService::resolveUrl("https://example.org/a/", "next/?at=10:30", b, sizeof b));
  assert(!strcmp(b,"https://example.org/a/next/?at=10:30"));
  assert(!BrowserService::resolveUrl("https://example.org/a/", "javascript:alert(1)", b, sizeof b));
  assert(!BrowserService::resolveUrl("https://example.org/a/", "../../../x", b, 14));
  puts("v1.9 URL normalization / redirect resolution: 20 checks PASS");
}
