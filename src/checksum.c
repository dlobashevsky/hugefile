#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "common.h"
#include "checksum.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define NEW_API		0
#else
#define NEW_API		1
#endif


struct checksum_t
{
#if NEW_API
  EVP_MD_CTX* ctx;
#else
  EVP_MD_CTX ctx;
#endif
};


__attribute__((constructor)) static void checksum_constructor(void)
{
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
//  OPENSSL_config(0);
}

__attribute__((destructor)) static void checksum_destructor(void)
{
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();
  ERR_free_strings();
}

checksum_t* checksum_init(void)
{
  checksum_t* cs=calloc(1,sizeof(*cs));
  const EVP_MD *hashptr = EVP_get_digestbyname(CHECKSUM_NAME);
#if NEW_API
  cs->ctx=EVP_MD_CTX_new();
  EVP_DigestInit_ex(cs->ctx, hashptr, 0);
#else
  EVP_MD_CTX_init(&cs->ctx);
  EVP_DigestInit_ex(&cs->ctx, hashptr, 0);
#endif
  return cs;
}


void checksum_update(checksum_t* cs,uint8_t* data,size_t size)
{
  if(!cs || !data) return;
#if NEW_API
  EVP_DigestUpdate(cs->ctx, data, size);
#else
  EVP_DigestUpdate(&cs->ctx, data, size);
#endif
}


void checksum_finalize(checksum_t* cs,uint8_t* result)
{
  uint32_t l=CHECKSUM_SIZE;
#if NEW_API
  EVP_DigestFinal_ex(cs->ctx, result, &l);
  EVP_MD_CTX_free(cs->ctx);
#else
  EVP_DigestFinal_ex(&cs->ctx, result, &l);
  EVP_MD_CTX_cleanup(&cs->ctx);
#endif
  free(cs);
}
