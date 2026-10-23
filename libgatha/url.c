#include "url.h"
#include "api.h"
#include "memory.h"
static int hex(char c) { return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'); }
static char lower(char c) { return c>='A'&&c<='Z'?(char)(c+32):c; }
static int prefix(const char *s,const char *p) { while(*p)if(lower(*s++)!=*p++)return 0;return 1; }
static int length(const char *s) {
    unsigned n=0;
    while(s[n]) { if(n>=G_URL_MAX-1)return -36;unsigned c=(unsigned char)s[n++];
        if(c<=32||c>=127||c=='\\'||c=='"'||c=='<'||c=='>')return -22;
        if(c=='%'&&(!s[n]||!s[n+1]||!hex(s[n])||!hex(s[n+1])))return -22;
    } return (int)n;
}
static int join(char *out,const char *a,unsigned n,const char *b) {
    unsigned m=(unsigned)g_strlen(b);if(n+m>=G_URL_MAX)return -36;
    memcpy(out,a,n);memcpy(out+n,b,m+1);return 0;
}
static int absolute(const char *input,char *out) {
    unsigned scheme=prefix(input,"https://")?8:prefix(input,"http://")?7:0;
    if(!scheme)return -95;
    unsigned end=scheme;while(input[end]&&input[end]!='/'&&input[end]!='?'&&input[end]!='#')++end;
    unsigned host=scheme;while(host<end&&input[host]!=':')++host;
    if(host==scheme||host-scheme>253)return -22;
    unsigned label=0;
    for(unsigned i=scheme;i<host;++i) {
        char c=lower(input[i]);
        if(c=='.'){if(!label||input[i-1]=='-')return -22;label=0;}
        else {if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-')||(!label&&c=='-')||++label>63)return -22;}
    }
    if(!label||input[host-1]=='-')return -22;
    if(host<end) {
        unsigned port=0,digits=0;
        for(unsigned i=host+1;i<end;++i){if(input[i]<'0'||input[i]>'9'||++digits>5)return -22;port=port*10+(unsigned)(input[i]-'0');}
        if(!port||port>65535)return -22;
    }
    for(unsigned i=0;i<end;++i)out[i]=lower(input[i]);
    unsigned n=end,p=end,stack[G_URL_MAX/2+1],depth=0;
    /* Remove literal dot segments only: escaped delimiters stay escaped. */
    if(input[p]!='/')out[n++]='/';
    while(input[p]=='/') {
        unsigned start=p++;while(input[p]&&input[p]!='/'&&input[p]!='?'&&input[p]!='#')++p;
        unsigned size=p-start-1;
        if(size==1&&input[start+1]=='.') { if(input[p]!='/')out[n++]='/'; }
        else if(size==2&&input[start+1]=='.'&&input[start+2]=='.') {
            if(depth)n=stack[--depth];
            if(input[p]!='/')out[n++]='/';
        } else {stack[depth++]=n;memcpy(out+n,input+start,size+1);n+=size+1;}
    }
    if(n==end)out[n++]='/';
    if(n-end>767)return -36;
    unsigned rest=(unsigned)g_strlen(input+p);if(n+rest>=G_URL_MAX)return -36;
    memcpy(out+n,input+p,rest+1);
    unsigned request=n;while(out[request]&&out[request]!='#')++request;
    if(request-end>767)return -36;
    return 0;
}
int g_url(const char *base,const char *ref,char out[G_URL_MAX]) {
    int nr=length(ref);if(nr<0)return nr;
    char merged[G_URL_MAX],normalized[G_URL_MAX];unsigned colon=0;
    while(ref[colon]&&ref[colon]!=':'&&ref[colon]!='/'&&ref[colon]!='?'&&ref[colon]!='#')++colon;
    int r;
    if(ref[colon]==':')g_copy(merged,ref,sizeof(merged));
    else {
        if((r=length(base))<0)return r;
        if((r=absolute(base,normalized)))return r;
        unsigned scheme=prefix(normalized,"https://")?8:7,authority=scheme;
        while(normalized[authority]!='/')++authority;
        unsigned path=authority;while(normalized[path]&&normalized[path]!='?'&&normalized[path]!='#')++path;
        unsigned query=path;while(normalized[query]&&normalized[query]!='#')++query;
        unsigned take;
        if(ref[0]=='/'&&ref[1]=='/')take=scheme-2;
        else if(ref[0]=='/')take=authority;
        else if(ref[0]=='?')take=path;
        else if(!ref[0]||ref[0]=='#')take=query;
        else {take=path;while(take>authority&&normalized[take-1]!='/')--take;}
        if((r=join(merged,normalized,take,ref)))return r;
    }
    if((r=absolute(merged,normalized)))return r;
    g_copy(out,normalized,G_URL_MAX);return 0;
}
int g_url_request(const char *url,char out[G_URL_MAX]) {
    int r=g_url("",url,out);if(r)return r;
    for(unsigned i=0;out[i];++i)if(out[i]=='#'){out[i]=0;break;}
    return 0;
}
