#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../apps/browser/document.h"
static void url_tests(void) {
    const char *base="http://a/b/c/d;p?q";
    const char *cases[][2]={
        {"g","http://a/b/c/g"},{"./g","http://a/b/c/g"},{"g/","http://a/b/c/g/"},
        {"/g","http://a/g"},{"//g","http://g/"},{"?y","http://a/b/c/d;p?y"},
        {"g?y","http://a/b/c/g?y"},{"#s","http://a/b/c/d;p?q#s"},{"g#s","http://a/b/c/g#s"},
        {"g?y#s","http://a/b/c/g?y#s"},{";x","http://a/b/c/;x"},{"g;x","http://a/b/c/g;x"},
        {"","http://a/b/c/d;p?q"},{".","http://a/b/c/"},{"./","http://a/b/c/"},
        {"..","http://a/b/"},{"../","http://a/b/"},{"../g","http://a/b/g"},
        {"../..","http://a/"},{"../../","http://a/"},{"../../g","http://a/g"},
        {"../../../g","http://a/g"},{"/./g","http://a/g"},{"/../g","http://a/g"},
        {"g/./h","http://a/b/c/g/h"},{"g/../h","http://a/b/c/h"},
        {"g?y/./x","http://a/b/c/g?y/./x"},{"HTTPS://Example.COM?x","https://example.com/?x"},
        {"http://a:8080//x/%2e%2e/y","http://a:8080//x/%2e%2e/y"}};
    char out[G_URL_MAX];
    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);++i){
        assert(!g_url(base,cases[i][0],out));
        if(strcmp(out,cases[i][1])){fprintf(stderr,"URL %s -> %s != %s\n",cases[i][0],out,cases[i][1]);abort();}
        assert(!g_url(out,"#here",out));
    }
    const char *bad[]={"javascript:alert(1)","file:///x","ftp://a/","http://a:0/","http://a:65536/","http://a:/",
        "http://user@a/","http://[::1]/","http://a/\r\nx","http://a/ x","http://a/%","http://a/%zz","http://-a/","http://a..b/","http://a/\\b"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(g_url(base,bad[i],out)<0);
    assert(!g_url_request("https://a/?q#section",out));assert(!strcmp(out,"https://a/?q"));
    char long_url[2048];memset(long_url,'a',sizeof(long_url));long_url[sizeof(long_url)-1]=0;assert(g_url(base,long_url,out)<0);
}
static void invariants(struct page *p) {
    assert(p->size<PAGE_TEXT&&p->link_count<=PAGE_LINKS&&p->anchor_count<=128&&p->row_count<=PAGE_ROWS);
    assert(!p->text[p->size]);
    for(unsigned i=0;i<p->size;++i)assert(p->link[i]<=p->link_count);
    for(unsigned i=0;i<p->row_count;++i)assert(p->rows[i].start<=p->rows[i].end&&p->rows[i].end<=p->size&&p->rows[i].end-p->rows[i].start<=(unsigned)p->columns);
}
int main(void) {
    url_tests();struct page *p=malloc(sizeof(*p));assert(p);
    const char *html="<head><title>A &amp; B</title><style>hide</style></head><body><h1 id='top'>Heading</h1>"
        "<p>Hello   world &lt;ok&gt; &#x41; &#65; caf\xc3\xa9</p><a href='../next?q=1&amp;b=2'>Next</a>"
        "<script>if(a<b){bad()}</script><p id='end'>Visible</p><a href='javascript:bad()'>Unsafe</a><pre>a\n b</pre><img alt='Picture'>"
        "<a href='https://caf\xc3\xa9.test/'>Unicode host</a><a href='/caf&#233;'>Unicode path</a>"
        "<a href='https://safe.test/&#10;tail'>Control in link</a></body>";
    page_parse(p,html,(unsigned)strlen(html),1,"https://unit.test/dir/page");
    assert(!strcmp(p->title,"A & B"));assert(strstr(p->text,"Hello world <ok> A A cafe"));
    assert(!strstr(p->text,"hide")&&!strstr(p->text,"bad"));assert(strstr(p->text,"Visible")&&strstr(p->text,"[Picture]"));
    assert(p->link_count==1&&!strcmp(p->links[0],"https://unit.test/next?q=1&b=2"));
    assert(p->anchor_count==2);page_layout(p,12);invariants(p);assert(page_fragment(p,"https://unit.test/#end")>0);
    static const char nul_link[]="<a href='https://unit.test/a\0b'>NUL link</a>";
    page_parse(p,nul_link,sizeof(nul_link)-1,1,"https://unit.test/");assert(!p->link_count);
    static const char invalid_link[]="<a href='https://unit.test/\xff'>Invalid UTF-8 link</a>";
    page_parse(p,invalid_link,sizeof(invalid_link)-1,1,"https://unit.test/");assert(!p->link_count);
    page_parse(p,"<b>plain</b>\n  spaces",22,0,"http://a/");assert(strstr(p->text,"<b>plain</b>"));page_layout(p,3);invariants(p);
    assert(page_type("text/html; charset=UTF-8")==1&&page_type("Text/Plain; charset=\"us-ascii\"")==0);
    assert(page_type("")<0&&page_type("application/octet-stream")<0&&page_type("text/html; charset=utf-16")<0);
    char *huge=malloc(524288);assert(huge);memset(huge,'<',524288);
    page_parse(p,huge,524288,1,"http://a/");assert(p->truncated);page_layout(p,1);invariants(p);
    memset(huge,'\n',524288);page_parse(p,huge,524288,0,"http://a/");page_layout(p,80);assert(p->truncated);invariants(p);free(huge);
    unsigned rng=0x12345678;char fuzz[4096],out[G_URL_MAX];
    for(unsigned round=0;round<12000;++round) {
        rng=rng*1664525+1013904223;unsigned n=rng%sizeof(fuzz);
        for(unsigned i=0;i<n;++i){rng=rng*1664525+1013904223;fuzz[i]=(char)(rng>>24);}
        page_parse(p,fuzz,n,round&1,"https://unit.test/dir/");page_layout(p,(int)(round%140)+1);invariants(p);
        fuzz[n]=0;(void)g_url("https://unit.test/dir/",fuzz,out);
    }
    free(p);puts("BROWSER NATIVE PASS: RFC URI vectors, bounded HTML/text, MIME, UTF-8 fallback, 12000 ASan/UBSan mutations");return 0;
}
