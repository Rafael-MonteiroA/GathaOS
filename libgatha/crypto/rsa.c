#include "crypto.h"
#include "../memory.h"
/* Only public-key operations. Montgomery multiplication uses a full product
 * then word-by-word reduction. The signature must be strictly below n. */
static int compare(const uint32_t *a,const uint32_t *b,unsigned n){for(unsigned i=n;i--;)if(a[i]!=b[i])return a[i]>b[i]?1:-1;return 0;}
static void subtract(uint32_t *a,const uint32_t *b,unsigned n){uint64_t borrow=0;for(unsigned i=0;i<n;++i){uint64_t v=(uint64_t)a[i]-b[i]-borrow;a[i]=(uint32_t)v;borrow=v>>63;}}
static void twice(uint32_t *a,const uint32_t *mod,unsigned n){uint32_t carry=0;for(unsigned i=0;i<n;++i){uint32_t next=a[i]>>31;a[i]=(a[i]<<1)|carry;carry=next;}if(carry || compare(a,mod,n)>=0)subtract(a,mod,n);}
static void montgomery(uint32_t *out,const uint32_t *a,const uint32_t *b,const uint32_t *mod,unsigned n,uint32_t inverse) {
    uint32_t t[258]={0};
    for(unsigned i=0;i<n;++i){uint64_t carry=0;for(unsigned j=0;j<n;++j){uint64_t v=(uint64_t)a[i]*b[j]+t[i+j]+carry;t[i+j]=(uint32_t)v;carry=v>>32;}t[i+n]=(uint32_t)carry;}
    for(unsigned i=0;i<n;++i){uint32_t m=t[i]*inverse;uint64_t carry=0;for(unsigned j=0;j<n;++j){uint64_t v=(uint64_t)m*mod[j]+t[i+j]+carry;t[i+j]=(uint32_t)v;carry=v>>32;}unsigned j=i+n;do{uint64_t v=(uint64_t)t[j]+carry;t[j++]=(uint32_t)v;carry=v>>32;}while(carry);}
    memcpy(out,t+n,n*4);if(t[2*n] || compare(out,mod,n)>=0)subtract(out,mod,n);
}
static int recover(const struct rsa_key *key,const uint8_t *sig,unsigned size,uint8_t encoded[512]) {
    if(size!=key->size || size<256 || size>512 || size%4 || !(key->modulus[0]&128) || !(key->modulus[size-1]&1) || key->exponent!=65537)return -1;
    unsigned n=size/4;uint32_t mod[128],base[128],r2[128]={1},one[128]={1},value[128];
    for(unsigned i=0;i<n;++i){mod[i]=base[i]=0;for(unsigned j=0;j<4;++j){mod[i]|=(uint32_t)key->modulus[size-1-4*i-j]<<(8*j);base[i]|=(uint32_t)sig[size-1-4*i-j]<<(8*j);}}
    if(compare(base,mod,n)>=0)return -1;
    uint32_t inverse=1;for(unsigned i=0;i<5;++i)inverse*=2-mod[0]*inverse;inverse=0u-inverse;
    for(unsigned i=0;i<64*n;++i)twice(r2,mod,n);
    montgomery(base,base,r2,mod,n,inverse);memcpy(value,base,4*n);
    for(unsigned i=0;i<16;++i)montgomery(value,value,value,mod,n,inverse);
    montgomery(value,value,base,mod,n,inverse);montgomery(value,value,one,mod,n,inverse);
    for(unsigned i=0;i<size;++i)encoded[size-1-i]=(uint8_t)(value[i/4]>>(8*(i%4)));
    return 0;
}
int rsa_verify(const struct rsa_key *key,unsigned hash,const uint8_t *digest,const uint8_t *sig,unsigned size) {
    unsigned dn=hash==4?32:hash==5?48:hash==6?64:0;if(!dn)return -1;
    uint8_t encoded[512],expected[512];if(recover(key,sig,size,encoded))return -1;
    memset(expected,255,size);expected[0]=0;expected[1]=1;unsigned at=size-dn-19-1;expected[at++]=0;
    uint8_t prefix[19]={0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20};
    prefix[1]=(uint8_t)(dn+17);prefix[14]=(uint8_t)(hash==4?1:hash==5?2:3);prefix[18]=(uint8_t)dn;memcpy(expected+at,prefix,19);memcpy(expected+at+19,digest,dn);
    return crypto_equal(encoded,expected,size)?0:-1;
}

/* EMSA-PSS-VERIFY, RFC 8017. The supported moduli have a full top bit, so
 * emBits=modBits-1 always leaves precisely one unused bit. Public data only. */
int rsa_pss_verify(const struct rsa_key *key,unsigned hash,unsigned salt_size,const uint8_t *digest,const uint8_t *sig,unsigned size) {
    unsigned hn=hash==4?32:hash==5?48:hash==6?64:0;
    if(!hn || size<hn+2 || salt_size>size-hn-2)return -1;
    uint8_t em[512],input[72+512],mask[64],expected[64];
    if(recover(key,sig,size,em) || em[size-1]!=0xbc || (em[0]&128))return -1;
    unsigned db_size=size-hn-1;
    memcpy(input,em+db_size,hn);
    for(unsigned offset=0,counter=0;offset<db_size;offset+=hn,++counter){
        input[hn]=(uint8_t)(counter>>24);input[hn+1]=(uint8_t)(counter>>16);
        input[hn+2]=(uint8_t)(counter>>8);input[hn+3]=(uint8_t)counter;
        crypto_hash(hash,input,hn+4,mask);
        unsigned count=db_size-offset;if(count>hn)count=hn;
        for(unsigned j=0;j<count;++j)em[offset+j]^=mask[j];
    }
    em[0]&=127;
    unsigned delimiter=db_size-salt_size-1;
    for(unsigned i=0;i<delimiter;++i)if(em[i])return -1;
    if(em[delimiter]!=1)return -1;
    memset(input,0,8);memcpy(input+8,digest,hn);memcpy(input+8+hn,em+delimiter+1,salt_size);
    crypto_hash(hash,input,8+hn+salt_size,expected);
    return crypto_equal(expected,em+db_size,hn)?0:-1;
}
