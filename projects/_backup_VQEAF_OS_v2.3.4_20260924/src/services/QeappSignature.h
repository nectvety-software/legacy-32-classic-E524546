#pragma once
#include <stddef.h>
#include <stdint.h>
// Public-key authenticity layer for QEAPP/2. No private key is compiled into firmware.
namespace Qeapp {
static const size_t SIGNATURE_BYTES = 76; // "QSIGP256" + LE key ID + raw r||s
bool verifySignature(const uint8_t digest[32], const uint8_t trailer[SIGNATURE_BYTES], const char *&error);
}
