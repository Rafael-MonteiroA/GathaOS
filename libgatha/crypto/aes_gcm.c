#include "crypto.h"
#include "../memory.h"
static uint8_t twice(uint8_t a){return (uint8_t)((a<<1)^(0x1b&(0u-(a>>7))));}
static uint8_t product(uint8_t a,uint8_t b){uint8_t r=0;for(unsigned i=0;i<8;++i){r^=a&(0u-(b&1));b>>=1;a=twice(a);}return r;}
static uint8_t rotate(uint8_t a,unsigned n){return (uint8_t)((a<<n)|(a>>(8-n)));}
static uint8_t substitute(uint8_t a) {
    /* Inversion by a fixed addition chain, followed by the AES affine map.
     * Deliberately avoids cache-indexed S-boxes containing secret addresses. */
    uint8_t a2=product(a,a),a3=product(a2,a),a6=product(a3,a3),a12=product(a6,a6),a15=product(a12,a3);
    uint8_t a30=product(a15,a15),a60=product(a30,a30),a120=product(a60,a60),a240=product(a120,a120);
    uint8_t x=product(product(a240,a12),a2);return x^rotate(x,1)^rotate(x,2)^rotate(x,3)^rotate(x,4)^0x63;
}
void aes128_init(struct aes128 *a,const uint8_t key[16]) {
    memcpy(a->round,key,16);uint8_t rc=1;
    for(unsigned i=16;i<176;i+=4){uint8_t t[4];memcpy(t,a->round+i-4,4);if(!(i%16)){uint8_t first=t[0];t[0]=substitute(t[1])^rc;t[1]=substitute(t[2]);t[2]=substitute(t[3]);t[3]=substitute(first);rc=twice(rc);}for(unsigned j=0;j<4;++j)a->round[i+j]=a->round[i+j-16]^t[j];}
}
void aes128_block(const struct aes128 *a,const uint8_t in[16],uint8_t out[16]) {
    uint8_t s[16],t[16];for(unsigned i=0;i<16;++i)s[i]=in[i]^a->round[i];
    for(unsigned round=1;round<=10;++round){for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)t[4*col+row]=substitute(s[4*((col+row)%4)+row]);
        if(round<10)for(unsigned col=0;col<4;++col){uint8_t *p=t+4*col,x=p[0]^p[1]^p[2]^p[3],first=p[0];p[0]^=x^twice(p[0]^p[1]);p[1]^=x^twice(p[1]^p[2]);p[2]^=x^twice(p[2]^p[3]);p[3]^=x^twice(p[3]^first);}
        for(unsigned i=0;i<16;++i)s[i]=t[i]^a->round[16*round+i];
    }
    memcpy(out,s,16);crypto_wipe(s,16);crypto_wipe(t,16);
}
static void ghash_block(uint8_t state[16],const uint8_t h[16],const uint8_t block[16]) {
    uint8_t x[16],v[16],z[16]={0};for(unsigned i=0;i<16;++i)x[i]=state[i]^block[i];memcpy(v,h,16);
    for(unsigned i=0;i<128;++i){unsigned mask=0u-((x[i/8]>>(7-i%8))&1);for(unsigned j=0;j<16;++j)z[j]^=v[j]&mask;unsigned low=v[15]&1;for(unsigned j=15;j>0;--j)v[j]=(uint8_t)((v[j]>>1)|(v[j-1]<<7));v[0]=(uint8_t)((v[0]>>1)^(0xe1&(0u-low)));}
    memcpy(state,z,16);crypto_wipe(x,16);crypto_wipe(v,16);crypto_wipe(z,16);
}
static void ghash_data(uint8_t state[16],const uint8_t h[16],const uint8_t *data,unsigned n){while(n>=16){ghash_block(state,h,data);data+=16;n-=16;}if(n){uint8_t last[16]={0};memcpy(last,data,n);ghash_block(state,h,last);}}
static void length64(uint8_t *p,uint64_t n){for(unsigned i=0;i<8;++i)p[7-i]=(uint8_t)(n>>(i*8));}
static void authenticate(const struct aes128 *a,const uint8_t nonce[12],const void *aad,unsigned an,const uint8_t *data,unsigned n,uint8_t tag[16]) {
    uint8_t h[16]={0},state[16]={0},counter[16]={0},length[16];aes128_block(a,h,h);memcpy(counter,nonce,12);counter[15]=1;
    ghash_data(state,h,aad,an);ghash_data(state,h,data,n);length64(length,(uint64_t)an*8);length64(length+8,(uint64_t)n*8);ghash_block(state,h,length);aes128_block(a,counter,tag);for(unsigned i=0;i<16;++i)tag[i]^=state[i];crypto_wipe(h,16);crypto_wipe(state,16);
}
static void crypt(const struct aes128 *a,const uint8_t nonce[12],uint8_t *data,unsigned n){uint8_t counter[16]={0},mask[16];memcpy(counter,nonce,12);uint32_t sequence=2;while(n){counter[12]=(uint8_t)(sequence>>24);counter[13]=(uint8_t)(sequence>>16);counter[14]=(uint8_t)(sequence>>8);counter[15]=(uint8_t)sequence++;aes128_block(a,counter,mask);unsigned take=n<16?n:16;for(unsigned i=0;i<take;++i)data[i]^=mask[i];data+=take;n-=take;}crypto_wipe(mask,16);}
void gcm_encrypt(const struct aes128 *a,const uint8_t nonce[12],const void *aad,unsigned an,uint8_t *data,unsigned n,uint8_t tag[16]){crypt(a,nonce,data,n);authenticate(a,nonce,aad,an,data,n,tag);}
int gcm_decrypt(const struct aes128 *a,const uint8_t nonce[12],const void *aad,unsigned an,uint8_t *data,unsigned n,const uint8_t tag[16]){uint8_t expected[16];authenticate(a,nonce,aad,an,data,n,expected);int ok=crypto_equal(expected,tag,16);crypto_wipe(expected,16);if(!ok)return -1;crypt(a,nonce,data,n);return 0;}
