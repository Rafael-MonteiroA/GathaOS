#include "tls.h"
#include "../security.h"
#include "../memory.h"
#include "trust-roots.h"
#define RECORD 16384u
#define HANDSHAKE 65536u
struct tls {
    int handle,read_secure,write_secure,ready,closed,failed;
    unsigned deadline,rx_at,rx_size,type;
    g_net_wait wait;
    uint64_t read_sequence,write_sequence;
    struct aes128 read_key,write_key;
    uint8_t read_iv[4],write_iv[4],random[64],secret[32],master[48];
    struct sha256 transcript;
    struct rsa_key leaf;
    uint8_t rx[RECORD+24],tx[RECORD+29],message[HANDSHAKE+4];
};
static unsigned be16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static unsigned be24(const uint8_t *p){return (unsigned)p[0]<<16|(unsigned)p[1]<<8|p[2];}
static void put16(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void put24(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>16);p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)n;}
static void put64(uint8_t *p,uint64_t n){for(unsigned i=0;i<8;++i)p[7-i]=(uint8_t)(n>>(i*8));}
static int check(struct tls *t) {if((int32_t)(g_ticks()-t->deadline)>=0)return -110;if(t->wait && t->wait())return -125;return 0;}
static int raw(struct tls *t,unsigned op,void *buffer,unsigned n) {
    uint8_t *p=buffer;
    while(n){int r=check(t);if(r)return r;r=g_network(op,t->handle,p,n>16384?16384:n);if(r==-11){g_sleep(1);continue;}if(r<=0)return r?r:-74;p+=r;n-=(unsigned)r;}
    return 0;
}
static void aad(uint8_t out[13],uint64_t sequence,unsigned type,unsigned n){put64(out,sequence);out[8]=(uint8_t)type;out[9]=3;out[10]=3;put16(out+11,n);}
static int send_record(struct tls *t,unsigned type,const void *data,unsigned n) {
    if(n>RECORD)return -90;uint8_t *p=t->tx;unsigned wire=n;p[0]=(uint8_t)type;p[1]=3;p[2]=3;
    if(t->write_secure){if(t->write_sequence==UINT64_MAX)return -74;uint8_t nonce[12],additional[13];memcpy(nonce,t->write_iv,4);put64(nonce+4,t->write_sequence);memcpy(p+5,nonce+4,8);memcpy(p+13,data,n);aad(additional,t->write_sequence++,type,n);gcm_encrypt(&t->write_key,nonce,additional,13,p+13,n,p+13+n);wire+=24;}
    else memcpy(p+5,data,n);
    put16(p+3,wire);return raw(t,G_TCP_SEND,p,wire+5);
}
static int receive_record(struct tls *t) {
    uint8_t header[5];int r=raw(t,G_TCP_RECEIVE,header,5);if(r)return r;
    if(header[1]!=3 || header[2]!=3)return -74;unsigned n=be16(header+3);t->type=header[0];t->rx_at=0;t->rx_size=0;
    if(n>RECORD+(t->read_secure?24:0) || (t->read_secure && n<24))return -74;
    if((r=raw(t,G_TCP_RECEIVE,t->rx,n)))return r;
    if(t->read_secure){if(t->read_sequence==UINT64_MAX)return -74;uint8_t nonce[12],additional[13];memcpy(nonce,t->read_iv,4);memcpy(nonce+4,t->rx,8);n-=24;aad(additional,t->read_sequence++,t->type,n);if(gcm_decrypt(&t->read_key,nonce,additional,13,t->rx+8,n,t->rx+8+n))return -74;memmove(t->rx,t->rx+8,n);}
    t->rx_size=n;
    if(t->type==21){if(!t->ready || n!=2 || t->rx[0]!=1 || t->rx[1]!=0)return -74;t->closed=1;t->rx_size=0;return 0;}
    return 0;
}
static int handshake_bytes(struct tls *t,void *data,unsigned n) {
    uint8_t *p=data;
    while(n){if(t->rx_at==t->rx_size){int r=receive_record(t);if(r)return r;if(t->type!=22 || !t->rx_size)return -74;}if(t->type!=22)return -74;
        unsigned take=t->rx_size-t->rx_at;if(take>n)take=n;memcpy(p,t->rx+t->rx_at,take);p+=take;n-=take;t->rx_at+=take;
    }return 0;
}
static int read_handshake(struct tls *t,unsigned type,int transcript) {
    int r=handshake_bytes(t,t->message,4);if(r)return r;unsigned n=be24(t->message+1);if(t->message[0]!=type || n>HANDSHAKE)return -74;
    if((r=handshake_bytes(t,t->message+4,n)))return r;if(transcript)sha256_update(&t->transcript,t->message,n+4);return (int)n;
}
static int send_handshake(struct tls *t,unsigned type,unsigned n) {
    t->message[0]=(uint8_t)type;put24(t->message+1,n);sha256_update(&t->transcript,t->message,n+4);return send_record(t,22,t->message,n+4);
}
static int hostname(const char *host,unsigned *length,int *numeric) {
    unsigned n=0,label=0;*numeric=1;
    for(;host[n];++n){unsigned ch=(uint8_t)host[n];if(n>=253)return -22;
        if(ch=='.'){if(!label || host[n-1]=='-')return -22;label=0;continue;}
        if(ch<'0' || ch>'9')*numeric=0;
        if(!((ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || (ch>='0' && ch<='9') || ch=='-') || (!label && ch=='-') || ++label>63)return -22;
    }
    if(!label || host[n-1]=='-')return -22;*length=n;return 0;
}
static int client_hello(struct tls *t,const char *host) {
    unsigned hn;int numeric,r=hostname(host,&hn,&numeric);if(r)return r;uint8_t entropy[64];r=g_random(entropy,64);if(r){crypto_wipe(entropy,64);return r;}
    memcpy(t->random,entropy,32);memcpy(t->secret,entropy+32,32);crypto_wipe(entropy,64);
    uint8_t *p=t->message+4;p[0]=3;p[1]=3;memcpy(p+2,t->random,32);p+=34;*p++=0;put16(p,2);p+=2;*p++=0xc0;*p++=0x2f;*p++=1;*p++=0;
    uint8_t *extensions=p;p+=2;
    if(!numeric){put16(p,0);put16(p+2,hn+5);put16(p+4,hn+3);p[6]=0;put16(p+7,hn);memcpy(p+9,host,hn);p+=9+hn;}
    static const uint8_t options[]={
        0,10,0,4,0,2,0,29, /* X25519 */
        0,11,0,2,1,0, /* uncompressed EC points */
        0,13,0,8,0,6,4,1,5,1,6,1, /* RSA PKCS1 SHA256/384/512 */
        0,23,0,0, /* mandatory extended master secret */
        255,1,0,1,0, /* initial secure renegotiation binding */
        0,16,0,11,0,9,8,'h','t','t','p','/','1','.','1'};
    memcpy(p,options,sizeof(options));p+=sizeof(options);put16(extensions,(unsigned)(p-extensions-2));return send_handshake(t,1,(unsigned)(p-t->message-4));
}
static int server_hello(struct tls *t) {
    int r=read_handshake(t,2,1);if(r<0)return r;unsigned n=(unsigned)r;uint8_t *p=t->message+4;
    if(n<38 || p[0]!=3 || p[1]!=3)return -74;memcpy(t->random+32,p+2,32);unsigned session=p[34];if(session>32 || n<38+session)return -74;
    unsigned at=35+session;if(be16(p+at)!=0xc02f || p[at+2])return -74;at+=3;if(n-at<2 || be16(p+at)!=n-at-2)return -74;at+=2;
    unsigned seen=0;
    while(at<n){if(n-at<4)return -74;unsigned type=be16(p+at),size=be16(p+at+2);at+=4;if(size>n-at)return -74;unsigned bit;
        if(type==23){bit=1;if(size)return -74;}
        else if(type==0xff01){bit=2;if(size!=1 || p[at])return -74;}
        else if(type==0){bit=4;if(size)return -74;}
        else if(type==16){bit=8;static const uint8_t alpn[]={0,9,8,'h','t','t','p','/','1','.','1'};if(size!=sizeof(alpn) || memcmp(p+at,alpn,size))return -74;}
        else if(type==11){bit=16;if(size<2 || p[at]!=size-1)return -74;int zero=0;for(unsigned i=1;i<size;++i)zero|=!p[at+i];if(!zero)return -74;}
        else return -74;
        if(seen&bit)return -74;seen|=bit;at+=size;
    }
    return (seen&3)==3?0:-74;
}
static int certificates(struct tls *t,const char *host,const struct der *roots,unsigned rn,uint32_t now) {
    int r=read_handshake(t,11,1);if(r<0)return r;uint8_t *p=t->message+4;unsigned n=(unsigned)r;if(n<3 || be24(p)!=n-3)return -129;
    struct der chain[6];unsigned count=0,at=3;
    while(at<n){if(n-at<3 || count==6)return -129;unsigned size=be24(p+at);at+=3;if(!size || size>n-at)return -129;chain[count++]=(struct der){p+at,size};at+=size;}
    return x509_chain(chain,count,roots,rn,host,now,&t->leaf);
}
static int exchange(struct tls *t) {
    int r=read_handshake(t,12,1);if(r<0)return r;uint8_t *p=t->message+4;unsigned n=(unsigned)r;
    if(n<40 || p[0]!=3 || be16(p+1)!=29 || p[3]!=32 || p[37]!=1 || p[36]<4 || p[36]>6 || be16(p+38)!=n-40)return -74;
    uint8_t signed_data[100],digest[64],peer[32],public_key[32],shared[32],base[32]={9};
    memcpy(signed_data,t->random,64);memcpy(signed_data+64,p,36);crypto_hash(p[36],signed_data,100,digest);
    if(rsa_verify(&t->leaf,p[36],digest,p+40,n-40))return -129;memcpy(peer,p+4,32);
    r=read_handshake(t,14,1);if(r<0)return r;if(r || t->rx_at!=t->rx_size)return -74;
    if(x25519(public_key,t->secret,base) || x25519(shared,t->secret,peer)){crypto_wipe(shared,32);return -74;}
    crypto_wipe(t->secret,32);t->message[4]=32;memcpy(t->message+5,public_key,32);r=send_handshake(t,16,33);
    if(!r){struct sha256 transcript=t->transcript;sha256_final(&transcript,digest);tls_prf(shared,32,"extended master secret",digest,32,t->master,48);}
    crypto_wipe(shared,32);if(r)return r;
    uint8_t seed[64],keys[40];memcpy(seed,t->random+32,32);memcpy(seed+32,t->random,32);tls_prf(t->master,48,"key expansion",seed,64,keys,40);
    aes128_init(&t->write_key,keys);aes128_init(&t->read_key,keys+16);memcpy(t->write_iv,keys+32,4);memcpy(t->read_iv,keys+36,4);crypto_wipe(keys,40);
    uint8_t ccs=1;r=send_record(t,20,&ccs,1);if(r)return r;t->write_secure=1;
    struct sha256 transcript=t->transcript;sha256_final(&transcript,digest);tls_prf(t->master,48,"client finished",digest,32,t->message+4,12);
    r=send_handshake(t,20,12);if(r)return r;
    r=receive_record(t);if(r)return r;if(t->type!=20 || t->rx_size!=1 || t->rx[0]!=1)return -74;t->rx_at=t->rx_size;t->read_secure=1;
    r=read_handshake(t,20,0);if(r<0)return r;if(r!=12 || t->rx_at!=t->rx_size)return -74;
    transcript=t->transcript;sha256_final(&transcript,digest);uint8_t expected[12];tls_prf(t->master,48,"server finished",digest,32,expected,12);
    if(!crypto_equal(expected,t->message+4,12))return -74;
    crypto_wipe(t->master,48);crypto_wipe(t->message,sizeof(t->message));crypto_wipe(&t->transcript,sizeof(t->transcript));t->ready=1;return 0;
}
int g_tls_open_roots(struct tls **out,int handle,const char *host,unsigned deadline,g_net_wait wait,const struct der *roots,unsigned count) {
    *out=0;uint32_t now;int r=g_utc(&now);if(r)return r;
    struct tls *t=g_calloc(1,sizeof(*t));if(!t)return -12;t->handle=handle;t->deadline=deadline;t->wait=wait;sha256_init(&t->transcript);
    if(!(r=client_hello(t,host)) && !(r=server_hello(t)) && !(r=certificates(t,host,roots,count,now)))r=exchange(t);
    if(r){g_tls_free(t);return r;}*out=t;return 0;
}
int g_tls_open(struct tls **out,int handle,const char *host,unsigned deadline,g_net_wait wait){return g_tls_open_roots(out,handle,host,deadline,wait,trust_roots,sizeof(trust_roots)/sizeof(trust_roots[0]));}
int g_tls_send(struct tls *t,const void *data,unsigned n) {
    if(!t->ready || t->closed || t->failed)return -74;const uint8_t *p=data;unsigned sent=0;
    while(sent<n){int r=check(t);unsigned take=n-sent;if(take>4096)take=4096;if(!r)r=send_record(t,23,p+sent,take);if(r){t->failed=1;return r;}sent+=take;}
    return (int)sent;
}
int g_tls_receive(struct tls *t,void *data,unsigned n) {
    if(!t->ready || t->failed || !n)return -74;if(t->closed)return 0;
    while(t->rx_at==t->rx_size){int r=check(t);if(!r)r=receive_record(t);if(r){t->failed=1;return r;}if(t->closed)return 0;if(t->type!=23){t->failed=1;return -74;}}
    unsigned take=t->rx_size-t->rx_at;if(take>n)take=n;memcpy(data,t->rx+t->rx_at,take);t->rx_at+=take;return (int)take;
}
void g_tls_free(struct tls *t){if(t){crypto_wipe(t,sizeof(*t));g_free(t);}}
