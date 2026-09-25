#include "QeappSignature.h"
#include <string.h>
#ifdef QEAPP_TRUST_KEY_HEADER
#include QEAPP_TRUST_KEY_HEADER
#else
#include "QeappTrustKey.h"
#endif
#if defined(QEAPP_HOST_OPENSSL)
#include <openssl/ecdsa.h>
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/obj_mac.h>
#elif defined(ARDUINO) && __has_include(<mbedtls/ecdsa.h>) && !defined(QEAPP_HOST_STUB_CRYPTO)
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/bignum.h>
#define QEAPP_MBEDTLS 1
#elif defined(ARDUINO) && !defined(QEAPP_HOST_STUB_CRYPTO)
#error "QEAPP signatures require mbedTLS ECDSA P-256 support in the ESP32 toolchain"
#endif
namespace Qeapp {
static uint32_t readKeyId(const uint8_t *b) {
 return uint32_t(b[0]) | uint32_t(b[1])<<8 | uint32_t(b[2])<<16 | uint32_t(b[3])<<24;
}
bool verifySignature(const uint8_t digest[32],const uint8_t trailer[SIGNATURE_BYTES],const char *&error){
 error="";
 if(!digest||!trailer||memcmp(trailer,"QSIGP256",8)){error="Missing/invalid QEAPP signature";return false;}
 if(readKeyId(trailer+8)!=QEAPP_TRUST_KEY_ID){error="Unknown signing key ID";return false;}
 if(QEAPP_TRUST_PUBKEY[0]!=0x04||sizeof(QEAPP_TRUST_PUBKEY)!=65){error="Invalid built-in public key";return false;}
 const uint8_t *sig=trailer+12;
#if defined(QEAPP_HOST_OPENSSL)
 EC_KEY *key=EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
 if(!key){error="Crypto initialization failed";return false;}
 const EC_GROUP *group=EC_KEY_get0_group(key);
 EC_POINT *point=EC_POINT_new(group);
 BIGNUM *r=BN_bin2bn(sig,32,nullptr), *s=BN_bin2bn(sig+32,32,nullptr);
 ECDSA_SIG *es=ECDSA_SIG_new();
 bool prepared=point&&r&&s&&es&&EC_POINT_oct2point(group,point,QEAPP_TRUST_PUBKEY,65,nullptr)==1&&EC_KEY_set_public_key(key,point)==1;
 if(prepared){prepared=(ECDSA_SIG_set0(es,r,s)==1);if(prepared){r=nullptr;s=nullptr;}}
 int verified=prepared?ECDSA_do_verify(digest,32,es,key):0;
 if(r)BN_free(r);
 if(s)BN_free(s);
 if(es)ECDSA_SIG_free(es);
 if(point)EC_POINT_free(point);
 EC_KEY_free(key);
 if(verified!=1){error="Digital signature verification failed";return false;}
 return true;
#elif defined(QEAPP_MBEDTLS)
 mbedtls_ecp_group group;mbedtls_ecp_point point;mbedtls_mpi r,s;
 mbedtls_ecp_group_init(&group);mbedtls_ecp_point_init(&point);mbedtls_mpi_init(&r);mbedtls_mpi_init(&s);
 int rc=mbedtls_ecp_group_load(&group,MBEDTLS_ECP_DP_SECP256R1);
 if(!rc)rc=mbedtls_ecp_point_read_binary(&group,&point,QEAPP_TRUST_PUBKEY,65);
 if(!rc)rc=mbedtls_ecp_check_pubkey(&group,&point);
 if(!rc)rc=mbedtls_mpi_read_binary(&r,sig,32);
 if(!rc)rc=mbedtls_mpi_read_binary(&s,sig+32,32);
 if(!rc)rc=mbedtls_ecdsa_verify(&group,digest,32,&point,&r,&s);
 mbedtls_mpi_free(&s);mbedtls_mpi_free(&r);mbedtls_ecp_point_free(&point);mbedtls_ecp_group_free(&group);
 if(rc!=0){error="Digital signature verification failed";return false;}
 return true;
#else
 (void)sig;error="Crypto unavailable in host API mock";return false;
#endif
}
}
