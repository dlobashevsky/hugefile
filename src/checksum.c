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


struct checksum_t
{
  EVP_MD_CTX ctx;
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
  EVP_MD_CTX_init(&cs->ctx);
  EVP_DigestInit_ex(&cs->ctx, hashptr, 0);
  return cs;
}


void checksum_update(checksum_t* cs,uint8_t* data,size_t size)
{
  if(!cs || !data) return;
  EVP_DigestUpdate(&cs->ctx, data, size);
}


void checksum_finalize(checksum_t* cs,uint8_t* result)
{
  uint32_t l=CHECKSUM_SIZE;
  EVP_DigestFinal_ex(&cs->ctx, result, &l);
  EVP_MD_CTX_cleanup(&cs->ctx);
  free(cs);
}

