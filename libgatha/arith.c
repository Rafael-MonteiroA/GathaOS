#include <stdint.h>
uint64_t __udivdi3(uint64_t numerator,uint64_t denominator) {
    if(!denominator)return 0;
    uint64_t quotient=0,remainder=0;
    for(int i=63;i>=0;--i){unsigned carry=(unsigned)(remainder>>63);remainder=(remainder<<1)|((numerator>>(unsigned)i)&1);if(carry || remainder>=denominator){remainder-=denominator;quotient|=UINT64_C(1)<<(unsigned)i;}}
    return quotient;
}
int64_t __divdi3(int64_t a,int64_t b) {
    uint64_t ua=a<0?0-(uint64_t)a:(uint64_t)a,ub=b<0?0-(uint64_t)b:(uint64_t)b;
    uint64_t q=__udivdi3(ua,ub);return (int64_t)((a<0)!=(b<0)?0-q:q);
}
