#include "crypto.h"
#include "../memory.h"
static const uint32_t constants[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t rotate(uint32_t v,unsigned n){return (v>>n)|(v<<(32-n));}
static void block(struct sha256 *s) {
    uint32_t w[64];
    for(unsigned i=0;i<16;++i)w[i]=(uint32_t)s->block[4*i]<<24|(uint32_t)s->block[4*i+1]<<16|(uint32_t)s->block[4*i+2]<<8|s->block[4*i+3];
    for(unsigned i=16;i<64;++i){uint32_t x=w[i-15],y=w[i-2];w[i]=w[i-16]+(rotate(x,7)^rotate(x,18)^(x>>3))+w[i-7]+(rotate(y,17)^rotate(y,19)^(y>>10));}
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for(unsigned i=0;i<64;++i){uint32_t t=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[i]+w[i];uint32_t u=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
    crypto_wipe(w,sizeof(w));
}
void sha256_init(struct sha256 *s){static const uint32_t iv[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};memcpy(s->h,iv,32);s->bytes=0;s->used=0;}
void sha256_update(struct sha256 *s,const void *data,size_t n){const uint8_t *p=data;s->bytes+=n;while(n){unsigned take=64-s->used;if(take>n)take=(unsigned)n;memcpy(s->block+s->used,p,take);s->used+=take;p+=take;n-=take;if(s->used==64){block(s);s->used=0;}}}
void sha256_final(struct sha256 *s,uint8_t out[32]){uint64_t bits=s->bytes*8;s->block[s->used++]=128;if(s->used>56){memset(s->block+s->used,0,64-s->used);block(s);s->used=0;}memset(s->block+s->used,0,56-s->used);for(unsigned i=0;i<8;++i)s->block[63-i]=(uint8_t)(bits>>(8*i));block(s);for(unsigned i=0;i<32;++i)out[i]=(uint8_t)(s->h[i/4]>>(24-8*(i%4)));crypto_wipe(s,sizeof(*s));}
void sha256_sum(const void *p,size_t n,uint8_t out[32]){struct sha256 s;sha256_init(&s);sha256_update(&s,p,n);sha256_final(&s,out);}
void hmac256(const void *key,unsigned kn,const void *data,unsigned n,uint8_t out[32]) {
    uint8_t pad[64]={0},digest[32];struct sha256 s;
    if(kn>64)sha256_sum(key,kn,pad);else memcpy(pad,key,kn);
    for(unsigned i=0;i<64;++i)pad[i]^=0x36;
    sha256_init(&s);sha256_update(&s,pad,64);sha256_update(&s,data,n);sha256_final(&s,digest);
    for(unsigned i=0;i<64;++i)pad[i]^=0x36^0x5c;
    sha256_init(&s);sha256_update(&s,pad,64);sha256_update(&s,digest,32);sha256_final(&s,out);crypto_wipe(pad,64);crypto_wipe(digest,32);
}
int tls_prf(const void *key,unsigned kn,const char *label,const void *seed,unsigned sn,void *out,unsigned n) {
    uint8_t input[160],a[32],digest[32],*p=out;unsigned ln=0;while(label[ln])++ln;if(ln+sn>128)return -1;
    memcpy(input+32,label,ln);memcpy(input+32+ln,seed,sn);unsigned size=ln+sn;
    hmac256(key,kn,input+32,size,a);
    while(n){memcpy(input,a,32);hmac256(key,kn,input,32+size,digest);unsigned take=n<32?n:32;memcpy(p,digest,take);p+=take;n-=take;hmac256(key,kn,a,32,a);}
    crypto_wipe(input,sizeof(input));crypto_wipe(a,32);crypto_wipe(digest,32);return 0;
}
int crypto_equal(const void *a,const void *b,unsigned n){const uint8_t *x=a,*y=b;uint32_t diff=0;for(unsigned i=0;i<n;++i)diff|=x[i]^y[i];return diff==0;}
void crypto_wipe(void *p,unsigned n){volatile uint8_t *v=p;while(n--)*v++=0;}
