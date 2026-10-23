#include "crypto.h"
#include "../memory.h"
/* Seventeen radix-2^15 limbs make 2^255 == 19 reduction explicit. All field
 * loops and scalar swaps have fixed bounds; no secret-indexed tables. */
typedef uint32_t field[17];
static void reduce(field out,uint64_t v[17]) {
    for(unsigned pass=0;pass<3;++pass){for(unsigned i=0;i<16;++i){v[i+1]+=v[i]>>15;v[i]&=32767;}uint64_t carry=v[16]>>15;v[16]&=32767;v[0]+=19*carry;}
    uint32_t sub[17],borrow=0;
    for(unsigned i=0;i<17;++i){uint32_t p=i?32767:32749;uint32_t x=(uint32_t)v[i]-p-borrow;borrow=x>>31;sub[i]=x&32767;}
    uint32_t mask=0u-(1u-borrow);for(unsigned i=0;i<17;++i)out[i]=(sub[i]&mask)|((uint32_t)v[i]&~mask);
    crypto_wipe(sub,sizeof(sub));
}
static void add(field r,const field a,const field b){uint64_t t[17];for(unsigned i=0;i<17;++i)t[i]=(uint64_t)a[i]+b[i];reduce(r,t);crypto_wipe(t,sizeof(t));}
static void subtract(field r,const field a,const field b){uint64_t t[17];for(unsigned i=0;i<17;++i)t[i]=(uint64_t)a[i]+2*(i?32767:32749)-b[i];reduce(r,t);crypto_wipe(t,sizeof(t));}
static void multiply(field r,const field a,const field b){uint64_t t[33]={0};for(unsigned i=0;i<17;++i)for(unsigned j=0;j<17;++j)t[i+j]+=(uint64_t)a[i]*b[j];for(unsigned i=17;i<33;++i)t[i-17]+=19*t[i];reduce(r,t);crypto_wipe(t,sizeof(t));}
static void swap(field a,field b,unsigned bit){uint32_t mask=0u-bit;for(unsigned i=0;i<17;++i){uint32_t d=(a[i]^b[i])&mask;a[i]^=d;b[i]^=d;}}
int x25519(uint8_t out[32],const uint8_t scalar[32],const uint8_t point[32]) {
    uint8_t k[32];memcpy(k,scalar,32);k[0]&=248;k[31]=(k[31]&127)|64;
    struct {field x,x2,z2,x3,z3,a,aa,b,bb,e,c,d,da,cb,tmp,constant,inverse;} f;memset(&f,0,sizeof(f));
    for(unsigned i=0;i<255;++i)f.x[i/15]|=((point[i/8]>>(i%8))&1u)<<(i%15);
    uint64_t normalized[17];for(unsigned i=0;i<17;++i)normalized[i]=f.x[i];reduce(f.x,normalized);
    f.x2[0]=1;memcpy(f.x3,f.x,sizeof(field));f.z3[0]=1;f.constant[0]=121665&32767;f.constant[1]=121665>>15;
    unsigned previous=0;
    for(int bit=254;bit>=0;--bit){unsigned current=(k[(unsigned)bit/8]>>((unsigned)bit%8))&1;swap(f.x2,f.x3,previous^current);swap(f.z2,f.z3,previous^current);previous=current;
        add(f.a,f.x2,f.z2);multiply(f.aa,f.a,f.a);subtract(f.b,f.x2,f.z2);multiply(f.bb,f.b,f.b);subtract(f.e,f.aa,f.bb);
        add(f.c,f.x3,f.z3);subtract(f.d,f.x3,f.z3);multiply(f.da,f.d,f.a);multiply(f.cb,f.c,f.b);
        add(f.tmp,f.da,f.cb);multiply(f.x3,f.tmp,f.tmp);subtract(f.tmp,f.da,f.cb);multiply(f.tmp,f.tmp,f.tmp);multiply(f.z3,f.x,f.tmp);
        multiply(f.x2,f.aa,f.bb);multiply(f.tmp,f.constant,f.e);add(f.tmp,f.aa,f.tmp);multiply(f.z2,f.e,f.tmp);
    }
    swap(f.x2,f.x3,previous);swap(f.z2,f.z3,previous);
    /* Fermat inverse, exponent 2^255-21; these branches are public constants. */
    f.inverse[0]=1;
    for(int bit=254;bit>=0;--bit){multiply(f.inverse,f.inverse,f.inverse);if(bit>=5 || ((11u>>(unsigned)bit)&1u))multiply(f.inverse,f.inverse,f.z2);}
    multiply(f.tmp,f.x2,f.inverse);memset(out,0,32);for(unsigned i=0;i<255;++i)out[i/8]|=(uint8_t)(((f.tmp[i/15]>>(i%15))&1)<<(i%8));
    unsigned nonzero=0;for(unsigned i=0;i<32;++i)nonzero|=out[i];crypto_wipe(k,32);crypto_wipe(&f,sizeof(f));crypto_wipe(normalized,sizeof(normalized));return nonzero?0:-1;
}
