#include "services/TrustedTls.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
  assert(!TrustedTls::timeValidAt((time_t)0));
  assert(!TrustedTls::timeValidAt((time_t)1704067199));
  assert(TrustedTls::timeValidAt((time_t)1704067200));
  assert(TrustedTls::caPemLength() > 5000);
  assert(strstr(TrustedTls::caPem(), "BEGIN CERTIFICATE") != nullptr);
  WiFiClientSecure client;
  String error;
  assert(TrustedTls::configure(client, error)); // Host clock must be valid.
  assert(error.length() == 0);
  assert(client.trustedPem == TrustedTls::caPem());
  assert(strstr(client.trustedPem, "END CERTIFICATE") != nullptr);
  puts("v2.0 trust store compiled, time gate and setCACert: PASS");
}
