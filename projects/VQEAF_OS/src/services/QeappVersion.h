#pragma once

// Compares validated, dotted-numeric QEAPP versions without converting an
// arbitrarily long component into a potentially overflowing integer.
// Trailing .0 is equivalent (1.2 == 1.2.0). Returns -1, 0, or +1.
namespace Qeapp {
int compareVersion(const char *left, const char *right);
}
