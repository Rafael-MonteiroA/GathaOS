#ifndef GATHA_X509_H
#define GATHA_X509_H
#include "../crypto/crypto.h"
struct der { const uint8_t *p; unsigned n; };
struct public_key {
    struct rsa_key rsa;
    unsigned pss_only,pss_hash,pss_min_salt;
};
struct certificate {
    struct der encoded,tbs,issuer,subject,signature,san;
    struct public_key key;
    uint32_t before,after;
    unsigned hash,pss,salt_size,ca,path_limit,usage,has_usage,server,has_eku;
};
int x509_parse(struct der data,struct certificate *cert);
int x509_name(const struct certificate *cert,const char *host);
int x509_chain(const struct der *chain,unsigned count,const struct der *roots,unsigned root_count,const char *host,uint32_t now,struct public_key *leaf);
int x509_signature(const struct public_key *key,unsigned pss,unsigned hash,unsigned salt_size,const uint8_t *digest,struct der signature);
#endif
