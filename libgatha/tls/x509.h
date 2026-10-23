#ifndef GATHA_X509_H
#define GATHA_X509_H
#include "../crypto/crypto.h"
struct der { const uint8_t *p; unsigned n; };
struct certificate {
    struct der encoded,tbs,issuer,subject,signature,san;
    struct rsa_key key;
    uint32_t before,after;
    unsigned hash,ca,path_limit,usage,has_usage,server,has_eku;
};
int x509_parse(struct der data,struct certificate *cert);
int x509_name(const struct certificate *cert,const char *host);
int x509_chain(const struct der *chain,unsigned count,const struct der *roots,unsigned root_count,const char *host,uint32_t now,struct rsa_key *leaf);
#endif
