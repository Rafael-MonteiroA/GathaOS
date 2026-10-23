#ifndef GATHA_CRYPTO_H
#define GATHA_CRYPTO_H
#include <stdint.h>
#include <stddef.h>
struct sha256 { uint32_t h[8]; uint64_t bytes; unsigned used; uint8_t block[64]; };
void sha256_init(struct sha256 *s);
void sha256_update(struct sha256 *s,const void *data,size_t size);
void sha256_final(struct sha256 *s,uint8_t out[32]);
void sha256_sum(const void *data,size_t size,uint8_t out[32]);
void hmac256(const void *key,unsigned kn,const void *data,unsigned n,uint8_t out[32]);
int tls_prf(const void *key,unsigned kn,const char *label,const void *seed,unsigned sn,void *out,unsigned n);
/* Public signature hashes: TLS hash IDs 4 (SHA-256), 5 (SHA-384), 6 (SHA-512). */
unsigned crypto_hash(unsigned id,const void *data,unsigned n,uint8_t out[64]);
int crypto_equal(const void *a,const void *b,unsigned n);
void crypto_wipe(void *p,unsigned n);
int x25519(uint8_t out[32],const uint8_t scalar[32],const uint8_t point[32]);
struct aes128 { uint8_t round[176]; };
void aes128_init(struct aes128 *a,const uint8_t key[16]);
void aes128_block(const struct aes128 *a,const uint8_t in[16],uint8_t out[16]);
/* 96-bit nonce, 128-bit tag; decrypt authenticates before releasing plaintext. */
void gcm_encrypt(const struct aes128 *a,const uint8_t nonce[12],const void *aad,unsigned an,uint8_t *data,unsigned n,uint8_t tag[16]);
int gcm_decrypt(const struct aes128 *a,const uint8_t nonce[12],const void *aad,unsigned an,uint8_t *data,unsigned n,const uint8_t tag[16]);
struct rsa_key { uint8_t modulus[512]; unsigned size; uint32_t exponent; };
int rsa_verify(const struct rsa_key *key,unsigned hash,const uint8_t *digest,const uint8_t *sig,unsigned n);
#endif
