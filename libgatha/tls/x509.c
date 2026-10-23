#include "x509.h"
#include "../memory.h"
/* Strict, bounded DER. Views always refer to the supplied handshake buffer;
 * every nested length is constrained before its fields are accessed. */
static int take(struct der *in,unsigned tag,struct der *out,struct der *whole) {
    if(in->n<2 || in->p[0]!=tag)return -1;
    unsigned header=2,n=in->p[1];
    if(n&128){unsigned bytes=n&127;if(!bytes || bytes>3 || in->n<2+bytes || !in->p[2])return -1;n=0;for(unsigned i=0;i<bytes;++i)n=(n<<8)|in->p[2+i];if(n<128)return -1;header+=bytes;}
    if(n>in->n-header)return -1;
    if(out)*out=(struct der){in->p+header,n};if(whole)*whole=(struct der){in->p,n+header};in->p+=n+header;in->n-=n+header;return 0;
}
static int same(struct der a,struct der b){return a.n==b.n && !memcmp(a.p,b.p,a.n);}
static int oid(struct der value,const uint8_t *p,unsigned n){return value.n==n && !memcmp(value.p,p,n);}
static int integer(struct der *in,struct der *out){if(take(in,2,out,0) || !out->n || (out->p[0]&128) || (out->n>1 && !out->p[0] && !(out->p[1]&128)))return -1;return 0;}
static int small(struct der *in,unsigned *out){struct der value;if(integer(in,&value) || value.n>4)return -1;*out=0;for(unsigned i=0;i<value.n;++i)*out=(*out<<8)|value.p[i];return 0;}
static int boolean(struct der *in,unsigned *out){struct der value;if(take(in,1,&value,0) || value.n!=1 || (value.p[0]!=0 && value.p[0]!=255))return -1;*out=value.p[0]!=0;return 0;}
static unsigned signature_algorithm(struct der in) {
    static const uint8_t rsa_oid[]={0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1};struct der id,empty;
    if(take(&in,6,&id,0) || id.n!=9 || memcmp(id.p,rsa_oid,8))return 0;
    if(in.n && (take(&in,5,&empty,0) || empty.n))return 0;if(in.n)return 0;
    return id.p[8]==11?4:id.p[8]==12?5:id.p[8]==13?6:0;
}
static int public_key(struct der in,struct rsa_key *key) {
    static const uint8_t rsa_oid[]={0x2a,0x86,0x48,0x86,0xf7,0x0d,1,1,1};struct der algorithm,id,empty,bits,sequence,mod;
    if(take(&in,0x30,&algorithm,0) || take(&algorithm,6,&id,0) || !oid(id,rsa_oid,sizeof(rsa_oid)))return -1;
    if(algorithm.n && (take(&algorithm,5,&empty,0) || empty.n))return -1;if(algorithm.n)return -1;
    if(take(&in,3,&bits,0) || in.n || !bits.n || bits.p[0])return -1;++bits.p;--bits.n;
    if(take(&bits,0x30,&sequence,0) || bits.n || integer(&sequence,&mod))return -1;
    if(mod.n && !mod.p[0]){++mod.p;--mod.n;}
    if((mod.n!=256 && mod.n!=384 && mod.n!=512) || !(mod.p[0]&128) || !(mod.p[mod.n-1]&1))return -1;
    unsigned exponent;if(small(&sequence,&exponent) || sequence.n || exponent!=65537)return -1;
    memcpy(key->modulus,mod.p,mod.n);key->size=mod.n;key->exponent=exponent;return 0;
}
static int decimal(const uint8_t *p,unsigned n,unsigned *out){*out=0;for(unsigned i=0;i<n;++i){if(p[i]<'0' || p[i]>'9')return -1;*out=*out*10+p[i]-'0';}return 0;}
static int date(struct der *in,uint32_t *result) {
    struct der value;if(!in->n)return -1;unsigned tag=in->p[0],year,month,day,hour,minute,second;
    if((tag!=23 && tag!=24) || take(in,tag,&value,0) || value.n!=(tag==23?13u:15u) || value.p[value.n-1]!='Z')return -1;
    unsigned digits=tag==23?2:4;if(decimal(value.p,digits,&year))return -1;if(digits==2)year+=year<50?2000:1900;
    const uint8_t *p=value.p+digits;if(decimal(p,2,&month) || decimal(p+2,2,&day) || decimal(p+4,2,&hour) || decimal(p+6,2,&minute) || decimal(p+8,2,&second))return -1;
    static const unsigned days[12]={31,28,31,30,31,30,31,31,30,31,30,31};unsigned leap=year%4==0 && (year%100!=0 || year%400==0);
    if(year<1970 || year>2105 || month<1 || month>12 || day<1 || day>days[month-1]+(month==2 && leap) || hour>23 || minute>59 || second>59)return -1;
    uint32_t total=0;for(unsigned y=1970;y<year;++y)total+=365+(y%4==0 && (y%100!=0 || y%400==0));for(unsigned m=1;m<month;++m)total+=days[m-1]+(m==2 && leap);
    *result=((total+day-1)*24+hour)*3600+minute*60+second;return 0;
}
static int extensions(struct der in,struct certificate *c) {
    struct der list,seen[32];unsigned count=0;if(take(&in,0x30,&list,0) || in.n)return -1;
    while(list.n){struct der ext,id,data,value;if(count==32 || take(&list,0x30,&ext,0) || take(&ext,6,&id,0))return -1;
        for(unsigned i=0;i<count;++i)if(same(id,seen[i]))return -1;seen[count++]=id;
        unsigned critical=0;if(ext.n && ext.p[0]==1 && (boolean(&ext,&critical) || !critical))return -1;
        if(take(&ext,4,&data,0) || ext.n)return -1;
        unsigned number=id.n==3 && id.p[0]==0x55 && id.p[1]==0x1d?id.p[2]:0;
        if(number==19){if(take(&data,0x30,&value,0) || data.n)return -1;if(value.n && value.p[0]==1 && (boolean(&value,&c->ca) || !c->ca))return -1;
            if(value.n && (small(&value,&c->path_limit) || !c->ca))return -1;if(value.n || (c->ca && !critical))return -1;
        }else if(number==15){if(take(&data,3,&value,0) || data.n || value.n<2 || value.n>3 || value.p[0]>7 || (value.p[value.n-1]&((1u<<value.p[0])-1)))return -1;c->usage=value.p[1];c->has_usage=1;
        }else if(number==17){if(take(&data,0x30,&c->san,0) || data.n || !c->san.n)return -1;
        }else if(number==37){if(take(&data,0x30,&value,0) || data.n || !value.n)return -1;c->has_eku=1;
            static const uint8_t server[]={0x2b,6,1,5,5,7,3,1};while(value.n){struct der purpose;if(take(&value,6,&purpose,0))return -1;if(oid(purpose,server,sizeof(server)))c->server=1;}
        }else if(number==30 || number==33 || number==36 || number==54)return -1;
        else {
            /* Must-staple cannot be honored without OCSP. Never ignore it. */
            static const uint8_t feature[]={0x2b,6,1,5,5,7,1,24};if(oid(id,feature,sizeof(feature)) || critical)return -1;
        }
    }
    return 0;
}
int x509_parse(struct der data,struct certificate *c) {
    memset(c,0,sizeof(*c));c->encoded=data;c->path_limit=UINT32_MAX;struct der outer,tbs,algorithm,inner,bits,version,serial,validity,key,ext;
    if(data.n>16384 || take(&data,0x30,&outer,0) || data.n || take(&outer,0x30,&tbs,&c->tbs) || take(&outer,0x30,&algorithm,0))return -1;
    c->hash=signature_algorithm(algorithm);if(!c->hash || take(&outer,3,&bits,0) || outer.n || bits.n<2 || bits.p[0])return -1;c->signature=(struct der){bits.p+1,bits.n-1};
    unsigned v;if(take(&tbs,0xa0,&version,0) || small(&version,&v) || version.n || v!=2 || integer(&tbs,&serial) || serial.n>20)return -1;
    if(take(&tbs,0x30,&inner,0) || !same(inner,algorithm) || take(&tbs,0x30,0,&c->issuer) || take(&tbs,0x30,&validity,0))return -1;
    if(date(&validity,&c->before) || date(&validity,&c->after) || validity.n || c->before>c->after)return -1;
    if(take(&tbs,0x30,0,&c->subject) || take(&tbs,0x30,&key,0) || public_key(key,&c->key))return -1;
    if(tbs.n && (take(&tbs,0xa3,&ext,0) || extensions(ext,c)))return -1;return tbs.n?-1:0;
}
static unsigned lower(unsigned c){return c>='A' && c<='Z'?c+32:c;}
static int dns_name(struct der pattern,const char *host,unsigned hn) {
    if(pattern.n>253 || !pattern.n)return 0;unsigned start=0,offset=0;
    if(pattern.n>2 && pattern.p[0]=='*' && pattern.p[1]=='.'){
        /* Only a whole leftmost label. Reject a single-label suffix. */
        unsigned dots=0;for(unsigned i=2;i<pattern.n;++i)dots+=pattern.p[i]=='.';if(!dots)return 0;
        while(offset<hn && host[offset]!='.')++offset;if(!offset || offset==hn)return 0;start=1;
    }
    if(pattern.n-start!=hn-offset)return 0;
    for(unsigned i=start;i<pattern.n;++i){unsigned ch=pattern.p[i];if(!ch || ch>=128 || (ch!='.' && ch!='-' && !(lower(ch)>='a' && lower(ch)<='z') && !(ch>='0' && ch<='9')) || lower(ch)!=lower((uint8_t)host[offset+i-start]))return 0;}
    return 1;
}
static int ipv4(const char *host,unsigned hn,uint8_t out[4]){unsigned at=0;for(unsigned part=0;part<4;++part){unsigned n=0,value=0;while(at<hn && host[at]>='0' && host[at]<='9'){if(++n>3)return 0;value=value*10+host[at++]-'0';}if(!n || value>255)return 0;out[part]=(uint8_t)value;if(part<3 && (at>=hn || host[at++]!='.'))return 0;}return at==hn;}
int x509_name(const struct certificate *c,const char *host) {
    unsigned hn=0;while(host[hn]){if(++hn>253)return -1;}if(!hn)return -1;uint8_t address[4];int numeric=ipv4(host,hn,address),match=0;struct der names=c->san;
    while(names.n){unsigned tag=names.p[0];struct der name;if((tag&0xc0)!=0x80 || (tag&31)==31 || take(&names,tag,&name,0))return -1;
        if(tag==0x82 && !numeric && dns_name(name,host,hn))match=1;if(tag==0x87 && numeric && name.n==4 && !memcmp(name.p,address,4))match=1;
    }
    return match?0:-1;
}
static int signed_by(const struct certificate *child,const struct certificate *parent) {
    if(!same(child->issuer,parent->subject))return -1;uint8_t digest[64];if(!crypto_hash(child->hash,child->tbs.p,child->tbs.n,digest))return -1;
    return rsa_verify(&parent->key,child->hash,digest,child->signature.p,child->signature.n);
}
static int valid(const struct certificate *c,uint32_t now,int ca,unsigned depth) {
    if(now<c->before || now>c->after || (c->has_eku && !c->server))return -1;
    if(ca)return !c->ca || !c->has_usage || !(c->usage&4) || c->path_limit<depth?-1:0;
    return c->ca || (c->has_usage && !(c->usage&128))?-1:0;
}
int x509_chain(const struct der *chain,unsigned count,const struct der *roots,unsigned rn,const char *host,uint32_t now,struct rsa_key *leaf) {
    if(!count || count>6 || !rn || rn>8)return -129;
    struct certificate current,parent;
    if(x509_parse(chain[0],&current) || valid(&current,now,0,0) || x509_name(&current,host))return -129;*leaf=current.key;
    for(unsigned depth=0;depth<count;++depth){
        for(unsigned i=0;i<rn;++i){if(x509_parse(roots[i],&parent) || valid(&parent,now,1,depth))continue;
            if(!signed_by(&current,&parent))return 0;
        }
        if(depth+1==count || x509_parse(chain[depth+1],&parent) || valid(&parent,now,1,depth) || signed_by(&current,&parent))return -129;
        current=parent;
    }
    return -129;
}
