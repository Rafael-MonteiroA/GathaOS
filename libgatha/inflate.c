#include "inflate.h"

struct decoder {
    const uint8_t *input;
    uint8_t *output;
    unsigned size,at,used,capacity,base,window,bits,have,work;
    unsigned adler_a,adler_b;
    uint32_t crc,table[256];
    int error,gzip;
    int (*poll)(void *);
    void *context;
};
struct tree {
    unsigned count[16],first[16],offset[16];
    uint16_t symbols[288];
};
static void progress(struct decoder *d) {
    if(++d->work<16384 || d->error)return;
    d->work=0;
    if(d->poll){int r=d->poll(d->context);if(r)d->error=r<0?r:-125;}
}
static unsigned take(struct decoder *d,unsigned count) {
    while(d->have<count && !d->error) {
        if(d->at==d->size){d->error=-71;break;}
        d->bits|=(unsigned)d->input[d->at++]<<d->have;d->have+=8;progress(d);
    }
    if(d->error)return 0;
    unsigned value=d->bits&((1u<<count)-1);d->bits>>=count;d->have-=count;return value;
}
static void align_byte(struct decoder *d){d->bits=0;d->have=0;}
static uint32_t crc_byte(struct decoder *d,uint32_t crc,unsigned byte) {
    return (crc>>8)^d->table[(crc^byte)&255];
}
static void emit(struct decoder *d,unsigned byte) {
    if(d->error)return;
    if(d->used==d->capacity){d->error=-27;return;}
    d->output[d->used++]=(uint8_t)byte;
    if(d->gzip)d->crc=crc_byte(d,d->crc,byte);
    else {d->adler_a=(d->adler_a+byte)%65521;d->adler_b=(d->adler_b+d->adler_a)%65521;}
    progress(d);
}
/* Canonical codes are assigned by length, then by symbol number. The bit
   reader is LSB-first, but a Huffman code itself is transmitted MSB-first. */
static int tree_build(struct tree *t,const unsigned *lengths,unsigned n,int alphabet) {
    for(unsigned i=0;i<16;++i)t->count[i]=t->first[i]=t->offset[i]=0;
    unsigned total=0,max=0;
    for(unsigned i=0;i<n;++i) {
        unsigned len=lengths[i];if(len>15)return -71;
        if(len){++t->count[len];++total;if(len>max)max=len;}
    }
    int available=1;unsigned offset=0,code=0;
    for(unsigned len=1;len<=15;++len) {
        available=available*2-(int)t->count[len];if(available<0)return -71;
        code=(code+t->count[len-1])*2;t->first[len]=code;
        t->offset[len]=offset;offset+=t->count[len];
    }
    /* Empty distance trees are allowed for literal-only blocks. A singleton
       literal/distance tree uses one bit; code-length alphabets must be full. */
    if(available && (alphabet || (total && !(total==1 && max==1))))return -71;
    unsigned next[16];for(unsigned i=0;i<16;++i)next[i]=t->offset[i];
    for(unsigned symbol=0;symbol<n;++symbol)if(lengths[symbol])t->symbols[next[lengths[symbol]]++]=(uint16_t)symbol;
    return 0;
}
static unsigned symbol(struct decoder *d,const struct tree *t) {
    unsigned code=0;
    for(unsigned len=1;len<=15 && !d->error;++len) {
        code=code*2+take(d,1);
        if(code>=t->first[len] && code-t->first[len]<t->count[len])return t->symbols[t->offset[len]+code-t->first[len]];
    }
    if(!d->error)d->error=-71;return 0;
}
static int trees(struct decoder *d,struct tree *lit,struct tree *dist,unsigned kind) {
    unsigned lengths[320]={0};
    if(kind==1) {
        for(unsigned i=0;i<288;++i)lengths[i]=i<144?8:i<256?9:i<280?7:8;
        int r=tree_build(lit,lengths,288,0);if(r)return r;
        for(unsigned i=0;i<32;++i)lengths[i]=5;
        return tree_build(dist,lengths,32,0);
    }
    unsigned nl=take(d,5)+257,nd=take(d,5)+1,nc=take(d,4)+4;
    if(d->error)return d->error;if(nl>286)return -71;
    static const uint8_t canonical_order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    unsigned codes[19]={0};struct tree code_tree;
    for(unsigned i=0;i<nc;++i)codes[canonical_order[i]]=take(d,3);
    if(d->error)return d->error;
    int r=tree_build(&code_tree,codes,19,1);if(r)return r;
    for(unsigned at=0;at<nl+nd && !d->error;) {
        unsigned value=symbol(d,&code_tree),repeat=1;
        if(d->error)return d->error;
        if(value==16){if(!at)return -71;repeat=take(d,2)+3;value=lengths[at-1];}
        else if(value==17){repeat=take(d,3)+3;value=0;}
        else if(value==18){repeat=take(d,7)+11;value=0;}
        if(d->error)return d->error;
        if(repeat>nl+nd-at)return -71;
        while(repeat--)lengths[at++]=value;
    }
    if(d->error)return d->error;if(!lengths[256])return -71;
    r=tree_build(lit,lengths,nl,0);if(r)return r;
    return tree_build(dist,lengths+nl,nd,0);
}
static int blocks(struct decoder *d) {
    static const uint16_t length_base[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const uint8_t length_extra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const uint16_t distance_base[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const uint8_t distance_extra[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    unsigned final=0;
    while(!final && !d->error) {
        final=take(d,1);unsigned kind=take(d,2);
        if(d->error)return d->error;
        if(kind==3)return -71;
        if(!kind) {
            align_byte(d);unsigned n=take(d,16),inverse=take(d,16);
            if(d->error)return d->error;
            if((n^inverse)!=65535)return -71;
            while(n-- && !d->error)emit(d,take(d,8));
        } else {
            struct tree lit,dist;int r=trees(d,&lit,&dist,kind);if(r)return r;
            while(!d->error) {
                unsigned c=symbol(d,&lit);
                if(d->error)return d->error;
                if(c<256)emit(d,c);
                else if(c==256)break;
                else {
                    if(c>285)return -71;c-=257;
                    unsigned n=length_base[c]+take(d,length_extra[c]);
                    c=symbol(d,&dist);if(d->error)return d->error;if(c>29)return -71;
                    unsigned distance=distance_base[c]+take(d,distance_extra[c]);
                    if(d->error)return d->error;
                    if(distance>d->used-d->base || distance>d->window)return -71;
                    /* Byte copies deliberately permit overlapping matches. */
                    while(n-- && !d->error)emit(d,d->output[d->used-distance]);
                }
            }
        }
    }
    align_byte(d);return d->error;
}
static uint32_t little32(struct decoder *d) {
    uint32_t n=0;for(unsigned i=0;i<4;++i)n|=(uint32_t)take(d,8)<<(8*i);return n;
}
static int gzip_header(struct decoder *d) {
    unsigned start=d->at;
    unsigned magic0=take(d,8),magic1=take(d,8),method=take(d,8);
    if(d->error)return d->error;
    if(magic0!=31 || magic1!=139 || method!=8)return -71;
    unsigned flags=take(d,8);if(flags&224)return -71;
    for(unsigned i=0;i<6;++i)(void)take(d,8);
    if(flags&4) {
        unsigned n=take(d,16);if(n>8192)return -90;
        while(n-- && !d->error)(void)take(d,8);
    }
    for(unsigned flag=8;flag<=16;flag*=2)if(flags&flag) {
        unsigned c;
        do {if(d->at-start>=8192)return -90;c=take(d,8);}while(c && !d->error);
    }
    if(d->error)return d->error;if(d->at-start>8192)return -90;
    if(flags&2) {
        uint32_t crc=~0u;for(unsigned i=start;i<d->at;++i)crc=crc_byte(d,crc,d->input[i]);
        unsigned expected=take(d,16);if(d->error)return d->error;
        if(expected!=((~crc)&65535))return -71;
    }
    return d->error;
}
int g_inflate(const void *input,unsigned size,void *output,unsigned capacity,
              unsigned *written,unsigned encoding,int (*poll)(void *),void *context) {
    *written=0;
    if((!input && size) || !output || size>4259840 || capacity>4194304)return -22;
    if(encoding!=G_CONTENT_GZIP && encoding!=G_CONTENT_DEFLATE)return -95;
    struct decoder d={.input=input,.output=output,.size=size,.capacity=capacity,.window=32768,
                      .poll=poll,.context=context,.gzip=encoding==G_CONTENT_GZIP,.work=16383};
    progress(&d);if(d.error)return d.error;
    if(d.gzip) {
        for(unsigned i=0;i<256;++i){uint32_t c=i;for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0);d.table[i]=c;}
        unsigned members=0;
        do {
            if(++members>32)return -90;
            int r=gzip_header(&d);if(r)return r;
            d.base=d.used;d.crc=~0u;r=blocks(&d);if(r)return r;
            uint32_t crc=little32(&d),count=little32(&d);
            if(d.error)return d.error;if(crc!=~d.crc || count!=d.used-d.base)return -71;
        }while(d.at<d.size);
    } else {
        unsigned cmf=take(&d,8),flags=take(&d,8);
        if(d.error)return d.error;
        if((cmf&15)!=8 || (cmf>>4)>7 || ((cmf<<8)|flags)%31)return -71;
        if(flags&32)return -95; /* Preset dictionaries have no HTTP negotiation. */
        d.window=1u<<((cmf>>4)+8);d.adler_a=1;
        int r=blocks(&d);if(r)return r;
        uint32_t checksum=0;for(unsigned i=0;i<4;++i)checksum=(checksum<<8)|take(&d,8);
        if(d.error)return d.error;
        if(checksum!=((d.adler_b<<16)|d.adler_a) || d.at!=d.size)return -71;
    }
    *written=d.used;return 0;
}
