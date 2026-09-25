#include "QeappVersion.h"
#include <string.h>

namespace Qeapp {
static void nextComponent(const char *&p, const char *&begin, size_t &len) {
  // Missing components count as zero; parser separately rejects malformed
  // version strings before compareVersion() is called by the installer.
  if (!p || !*p) { begin = "0"; len = 1; return; }
  begin = p;
  while (*p && *p != '.') ++p;
  len = size_t(p - begin);
  if (*p == '.') ++p;
  while (len > 1 && *begin == '0') { ++begin; --len; }
}
int compareVersion(const char *left, const char *right) {
  const char *a = left, *b = right;
  // QEAPP Meta.version fits in 19 bytes. At most 20 dotted components
  // are consumed even if this helper is called with untrusted input.
  for (int part = 0; part < 20; ++part) {
    if ((!a || !*a) && (!b || !*b)) return 0;
    const char *av, *bv;
    size_t na, nb;
    nextComponent(a, av, na);
    nextComponent(b, bv, nb);
    if (na != nb) return na > nb ? 1 : -1;
    int cmp = memcmp(av, bv, na);
    if (cmp) return cmp > 0 ? 1 : -1;
  }
  return 0;
}
}
