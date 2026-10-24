#include "network.h"
#include "memory.h"
#include "tls/tls.h"
#include "inflate.h"
int g_ipv4(const char *text,uint32_t *address) {
    uint32_t result=0;
    for(unsigned part=0;part<4;++part) {
        unsigned value=0,digits=0;
        while(*text>='0' && *text<='9'){if(++digits>3)return -22;value=value*10+(unsigned)(*text++-'0');}
        if(!digits || value>255)return -22;
        result=(result<<8)|value;if(part<3){if(*text++!='.')return -22;}else if(*text)return -22;
    }
    *address=result;return 0;
}
void g_ip_text(uint32_t address,char text[16]) {
    char *p=text;for(unsigned i=0;i<4;++i){p=g_decimal(p,(int)((address>>(24-i*8))&255));if(i<3)*p++='.';}*p=0;
}
static int pause(g_net_wait wait) {if(wait && wait())return -125;g_sleep(1);return 0;}
int g_resolve(const char *name,uint32_t *address,g_net_wait wait) {
    if(!g_ipv4(name,address))return 0;
    int handle=g_network(G_DNS_START,0,(void *)name,(unsigned)g_strlen(name)+1);if(handle<0)return handle;
    int result;while((result=g_network(G_DNS_RESULT,handle,address,4))==-11)if((result=pause(wait)))break;
    g_network(G_DNS_CLOSE,handle,0,0);return result;
}
struct stream {int handle;unsigned at,size,deadline,limit;g_net_wait wait;struct tls *tls;uint8_t buffer[2048];};
static int check(struct stream *s) {
    if((int32_t)(g_ticks()-s->deadline)>=0)return -110;
    return pause(s->wait);
}
static int decode_poll(void *context){return check(context);}
static int receive(struct stream *s) {
    if(s->at<s->size)return 1;
    int r;while((r=s->tls?g_tls_receive(s->tls,s->buffer,sizeof(s->buffer)):g_network(G_TCP_RECEIVE,s->handle,s->buffer,sizeof(s->buffer)))==-11)if((r=check(s)))return r;
    if(r<=0)return r;
    s->at=0;s->size=(unsigned)r;return 1;
}
static int line(struct stream *s,char *out,unsigned capacity,unsigned *budget) {
    unsigned n=0;
    for(;;) {
        int r=receive(s);if(r<=0)return r?r:-71;
        if(!*budget)return -90;--*budget;
        unsigned ch=s->buffer[s->at++];
        if(ch=='\n') {if(!n || out[n-1]!='\r')return -71;out[n-1]=0;return (int)n-1;}
        if(n && out[n-1]=='\r')return -71;
        if(!ch || (ch<32 && ch!='\r' && ch!='\t') || n+1>=capacity)return -71;
        out[n++]=(char)ch;
    }
}
static char lower(char c){return c>='A' && c<='Z'?(char)(c+32):c;}
static int equal(const char *a,const char *b){while(*a && *b && lower(*a)==*b){++a;++b;}return !*a && !*b;}
static char *field(char *text) {
    char *p=text;while(*p && *p!=':'){if(*p<=32 || *p>=127)return 0;++p;}
    if(p==text || *p!=':')return 0;*p++=0;while(*p==' ' || *p=='\t')++p;
    unsigned n=(unsigned)g_strlen(p);while(n && (p[n-1]==' ' || p[n-1]=='\t'))p[--n]=0;return p;
}
static int append(void **data,unsigned *size,unsigned *capacity,const void *from,unsigned n,unsigned limit) {
    if(n>limit-*size)return -27;
    if(*size+n>*capacity) {
        unsigned grow=*capacity?*capacity:4096;while(grow<*size+n)grow*=2;
        void *p=g_realloc(*data,grow);if(!p)return -12;*data=p;*capacity=grow;
    }
    if(n)memcpy((uint8_t *)*data+*size,from,n);*size+=n;return 0;
}
static int body(struct stream *s,void **data,unsigned *size,unsigned *capacity,unsigned count,int closed) {
    while(count || closed) {
        int r=receive(s);if(r<=0)return r?r:closed?0:-71;
        unsigned n=s->size-s->at;if(!closed && n>count)n=count;
        r=append(data,size,capacity,s->buffer+s->at,n,s->limit);if(r)return r;
        s->at+=n;if(!closed)count-=n;
        if((r=check(s)))return r;
    }
    return 0;
}
int g_http_fetch(const char *url,struct g_http_response *response,unsigned limit,g_net_wait wait) {
    memset(response,0,sizeof(*response));
    if(!limit || limit>4194304)return -22;
    void **data=&response->data;unsigned *size=&response->size;
    *data=0;*size=0;
    unsigned un=(unsigned)g_strlen(url);int secure=un>=8 && !memcmp(url,"https://",8);
    if(!secure && (un<7 || memcmp(url,"http://",7)))return -95;
    char host[254],path[768];unsigned hn=0,port=secure?443:80;const char *p=url+(secure?8:7);
    while(*p && *p!='/' && *p!=':'){if(hn>=253 || *p<=32 || *p>=127 || *p=='@' || *p=='#' || *p=='?')return -22;host[hn++]=*p++;}
    host[hn]=0;if(!hn)return -22;
    if(*p==':'){++p;port=0;unsigned digits=0;while(*p>='0' && *p<='9'){if(++digits>5)return -22;port=port*10+(unsigned)(*p++-'0');}if(!port || port>65535)return -22;}
    if(*p && *p!='/')return -22;
    if(g_strlen(p)>=sizeof(path))return -36;
    g_copy(path,*p?p:"/",sizeof(path));for(unsigned i=0;path[i];++i)if(path[i]<=32 || path[i]>=127 || path[i]=='#')return -22;
    uint32_t address;int r=g_resolve(host,&address,wait);if(r)return r;
    struct g_endpoint endpoint={address,port};int handle=g_network(G_TCP_CONNECT,0,&endpoint,sizeof(endpoint));if(handle<0)return handle;
    struct stream s={.handle=handle,.deadline=g_ticks()+12000,.limit=limit,.wait=wait};unsigned capacity=0,encoding=0;
    while((r=g_network(G_TCP_STATUS,handle,0,0))==-11)if((r=check(&s)))break;
    if(r)goto done;
    if(secure){r=g_tls_open(&s.tls,handle,host,s.deadline,wait);if(r)goto done;g_puts("TLS: authenticated TLS 1.2 / X25519 / AES-128-GCM\n");}
    char request[1200];g_copy(request,"GET ",sizeof(request));g_copy(request+4,path,sizeof(request)-4);
    unsigned n=(unsigned)g_strlen(request);g_copy(request+n," HTTP/1.1\r\nHost: ",sizeof(request)-n);n=(unsigned)g_strlen(request);
    g_copy(request+n,host,sizeof(request)-n);n=(unsigned)g_strlen(request);
    if(port!=(secure?443u:80u)){request[n++]=':';g_decimal(request+n,(int)port);n=(unsigned)g_strlen(request);}
    g_copy(request+n,"\r\nUser-Agent: GathaOS/15\r\nAccept: */*\r\nAccept-Encoding: gzip, deflate\r\nConnection: close\r\n\r\n",sizeof(request)-n);
    n=(unsigned)g_strlen(request);
    for(unsigned sent=0;sent<n;) {
        r=s.tls?g_tls_send(s.tls,request+sent,n-sent):g_network(G_TCP_SEND,handle,request+sent,n-sent);
        if(r==-11){if((r=check(&s)))goto done;continue;}if(r<=0){if(!r)r=-71;goto done;}sent+=(unsigned)r;
    }
    char text[2048];unsigned budget=8192;
    r=line(&s,text,sizeof(text),&budget);if(r<0)goto done;
    if(r<12 || (memcmp(text,"HTTP/1.1 ",9) && memcmp(text,"HTTP/1.0 ",9)) ||
       text[9]<'2'||text[9]>'5'||text[10]<'0'||text[10]>'9'||text[11]<'0'||text[11]>'9'||
       (text[12] && text[12]!=' ')){r=-71;goto done;}
    response->status=(unsigned)(text[9]-'0')*100+(unsigned)(text[10]-'0')*10+(unsigned)(text[11]-'0');
    int length_seen=0,chunked=0,encoding_seen=0;unsigned length=0,wire_limit=limit+65536;
    while((r=line(&s,text,sizeof(text),&budget))>0) {
        char *value=field(text);if(!value){r=-71;goto done;}
        if(equal(text,"content-length")) {
            if(length_seen++ || !*value){r=-71;goto done;}
            for(;*value;++value){if(*value<'0' || *value>'9'){r=-71;goto done;}unsigned digit=(unsigned)(*value-'0');if(length>(wire_limit-digit)/10){r=-27;goto done;}length=length*10+digit;}
        } else if(equal(text,"transfer-encoding")) {if(chunked || !equal(value,"chunked")){r=-95;goto done;}chunked=1;}
        else if(equal(text,"content-encoding")) {
            if(encoding_seen++){r=-71;goto done;}
            if(equal(value,"gzip") || equal(value,"x-gzip"))encoding=G_CONTENT_GZIP;
            else if(equal(value,"deflate"))encoding=G_CONTENT_DEFLATE;
            else if(!equal(value,"identity")){r=-95;goto done;}
        }
        else if(equal(text,"location")) {
            if(response->location[0] || !*value || g_strlen(value)>=sizeof(response->location)){r=-71;goto done;}
            g_copy(response->location,value,sizeof(response->location));
        } else if(equal(text,"content-type")) {
            if(response->type[0] || !*value || g_strlen(value)>=sizeof(response->type)){r=-95;goto done;}
            g_copy(response->type,value,sizeof(response->type));
        }
    }
    if(r<0)goto done;
    if(chunked && length_seen){r=-71;goto done;}
    s.limit=encoding?wire_limit:limit;
    if(length>s.limit){r=-27;goto done;}
    if(response->status==301 || response->status==302 || response->status==303 ||
       response->status==307 || response->status==308 || response->status==204){encoding=0;r=0;goto done;}
    if(!chunked){r=body(&s,data,size,&capacity,length,!length_seen);goto done;}
    for(;;) {
        budget=2048;r=line(&s,text,sizeof(text),&budget);if(r<=0){if(!r)r=-71;goto done;}
        unsigned chunk=0,digits=0;
        for(p=text;*p && *p!=';';++p) {
            unsigned digit=*p>='0' && *p<='9'?(unsigned)(*p-'0'):lower(*p)>='a' && lower(*p)<='f'?(unsigned)(lower(*p)-'a'+10):16;
            if(digit>=16 || ++digits>8){r=-71;goto done;}
            if(digit>s.limit || chunk>(s.limit-digit)/16){r=-27;goto done;}chunk=chunk*16+digit;
        }
        if(!digits){r=-71;goto done;}
        if(!chunk)break;
        r=body(&s,data,size,&capacity,chunk,0);if(r)goto done;
        budget=2;r=line(&s,text,sizeof(text),&budget);if(r){if(r>0)r=-71;goto done;}
    }
    budget=8192;while((r=line(&s,text,sizeof(text),&budget))>0) {
        if(!field(text) || equal(text,"content-length") || equal(text,"transfer-encoding") ||
           equal(text,"content-encoding") || equal(text,"content-type") || equal(text,"location")){r=-71;break;}
    }
done:
    if(r>=0 && encoding) {
        unsigned decoded=0;void *plain=g_malloc(limit);
        g_puts(encoding==G_CONTENT_GZIP?"HTTP: decoding gzip\n":"HTTP: decoding deflate\n");
        r=plain?g_inflate(*data,*size,plain,limit,&decoded,encoding,decode_poll,&s):-12;
        if(!r) {
            char number[24];g_puts(encoding==G_CONTENT_GZIP?"HTTP: decoded gzip ":"HTTP: decoded deflate ");
            g_decimal(number,(int)*size);g_puts(number);g_puts(" -> ");g_decimal(number,(int)decoded);g_puts(number);g_puts(" bytes\n");
            g_free(*data);*data=plain;*size=decoded;
        } else g_free(plain);
    }
    g_tls_free(s.tls);
    g_network(G_TCP_CLOSE,handle,0,0);
    if(r<0){g_free(*data);*data=0;*size=0;return r;}
    return 0;
}
int g_http_get(const char *url,void **data,unsigned *size,g_net_wait wait) {
    struct g_http_response response;
    int r=g_http_fetch(url,&response,4194304,wait);
    if(!r && response.status!=200){g_free(response.data);response.data=0;response.size=0;r=-71;}
    *data=response.data;*size=response.size;return r;
}
