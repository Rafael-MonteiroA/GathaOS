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
    assert(p->form_count<=PAGE_FORMS&&p->control_count<=PAGE_CONTROLS);
    for(unsigned i=0;i<p->control_count;++i){struct page_control *c=&p->controls[i];assert(c->form<p->form_count&&c->maximum<=255);assert(memchr(c->name,0,sizeof(c->name))&&memchr(c->value,0,sizeof(c->value)));}
}
static void form_html(struct page *p,const char *html) {
    page_parse(p,html,(unsigned)strlen(html),1,"https://unit.test/dir/form?old=1");page_layout(p,80);invariants(p);
}
static void form_tests(struct page *p) {
    char out[G_URL_MAX];int invalid;
    const char *good="<form action='../search?discard=1#result' method=GET><input type=search name=q value='a +&amp;=/%#~' required>"
        "<input type=hidden name=tag value=one><input type=hidden name=tag value=two>"
        "<input name=skip value=no disabled><input name=read value=yes readonly><input value=unnamed>"
        "<button name=go value=first>Find <b>now</b></button><input type=submit name=go value=second></form>";
    form_html(p,good);assert(p->form_count==1&&p->control_count==8&&!strcmp(p->controls[6].caption,"Find now"));
    assert(!page_submit(p,0,"https://unit.test/",out,&invalid));
    assert(!strcmp(out,"https://unit.test/search?q=a+%2B%26%3D%2F%25%23%7E&tag=one&tag=two&read=yes&go=first#result"));
    assert(!page_submit(p,7,"https://unit.test/",out,&invalid)&&strstr(out,"go=second"));
    p->controls[0].value[0]=0;assert(page_submit(p,6,"https://unit.test/",out,&invalid)==-22&&invalid==0);
    p->controls[0].readonly=1;assert(!page_submit(p,6,"https://unit.test/",out,&invalid));
    form_html(p,"<form><input name=q value=one></form><form action='/other'><input name=x value=two><button>Go</button></form>");
    assert(!page_submit(p,0,"https://unit.test/",out,&invalid)&&!strcmp(out,"https://unit.test/dir/form?q=one"));
    assert(!page_submit(p,2,"https://unit.test/",out,&invalid)&&!strcmp(out,"https://unit.test/other?x=two"));
    form_html(p,"<form><input name=a><input name=b></form>");assert(page_submit(p,0,"https://unit.test/",out,&invalid)==-11);
    form_html(p,"<form><input name=a><button disabled>Stop</button><button>Go</button></form>");
    assert(page_submit(p,0,"https://unit.test/",out,&invalid)==-11);assert(!page_submit(p,2,"https://unit.test/",out,&invalid));
    const char *bad[]={
        "<form method=post><input name=q><button>Go</button></form>",
        "<form action='javascript:bad()'><input name=q></form>",
        "<form><input type=password name=p><button>Go</button></form>",
        "<form><input name=q><select name=x><option>A</option></select></form>",
        "<form><input name=q><textarea name=x>text</textarea></form>",
        "<form><fieldset disabled><input name=q></fieldset></form>",
        "<form><input name=q form=other></form>",
        "<form><input name=q value='caf&#233;'></form>",
        "<form><input name=q value='caf\xc3\xa9'></form>",
        "<form><input name=q value='x&#0;y'></form>",
        "<form><input name=q value='x&#10;y'></form>",
        "<form><input name=q><form><input name=b></form></form>",
        "<form target=_blank><input name=q></form>",
        "<form><input name=q pattern='[0-9]'></form>",
        "<form><input name=q maxlength=2 value=long></form>",
        "<form><input name=q><button formaction='/other'>Go</button></form>",
        "<form onsubmit='change()'><input name=q></form>",
        "<form><input name=q minlength=3></form>",
        "<head><base href='https://other.test/'></head><form><input name=q></form>",
        "<form><input name=q><input name='unterminated"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i){form_html(p,bad[i]);assert(page_submit(p,p->control_count-1,"https://unit.test/",out,&invalid)<0);}
    form_html(p,"<form action='http://unit.test/search'><input name=q value=yes></form>");
    assert(page_submit(p,0,"https://unit.test/",out,&invalid)==-13);
    assert(!page_submit(p,0,"http://unit.test/",out,&invalid));
    form_html(p,"<form><input name=q maxlength=4><input type=submit value=Go></form>");assert(p->controls[0].maximum==4);
    memset(p->controls[0].value,'%',255);p->controls[0].value[255]=0;assert(page_submit(p,1,"https://unit.test/",out,&invalid)==-36);
    char many[8192]="<form>";for(unsigned i=0;i<PAGE_CONTROLS+1;++i)strcat(many,"<input name=q>");strcat(many,"</form>");
    form_html(p,many);assert(p->control_count==PAGE_CONTROLS&&page_submit(p,0,"https://unit.test/",out,&invalid)==-95);
    many[0]=0;for(unsigned i=0;i<PAGE_FORMS+1;++i)strcat(many,"<form><input name=q></form>");
    form_html(p,many);assert(p->form_count==PAGE_FORMS&&page_submit(p,0,"https://unit.test/",out,&invalid)==-95);
    form_html(p,"<template><form><input name=hidden></form></template><form><input name=q></form>");
    assert(p->form_count==1&&page_submit(p,0,"https://unit.test/",out,&invalid)==-95);
    form_html(p,"<form><input name=q><input type=hidden name=_charset_ value=wrong></form>");
    assert(!page_submit(p,0,"https://unit.test/",out,&invalid)&&strstr(out,"_charset_=UTF-8"));
    unsigned rng=0x13579bdf;char mutated[2048];
    for(unsigned round=0;round<6000;++round) {
        strcpy(mutated,good);unsigned n=(unsigned)strlen(mutated);
        for(unsigned j=0;j<1+round%12;++j){rng=rng*1664525+1013904223;unsigned at=rng%n;rng=rng*1664525+1013904223;mutated[at]=(char)(rng>>24);}
        page_parse(p,mutated,n,1,"https://unit.test/dir/form");page_layout(p,1+(int)(round%100));invariants(p);
        for(unsigned i=0;i<p->control_count;++i)if(!page_submit(p,i,"https://unit.test/",out,&invalid))assert(!g_url("",out,out));
    }
    puts("FORMS NATIVE PASS: encoding, ownership, validation, unsupported cases, bounds, 6000 ASan/UBSan mutations");
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
    form_tests(p);free(p);puts("BROWSER NATIVE PASS: RFC URI vectors, bounded HTML/text, MIME, UTF-8 fallback, 12000 ASan/UBSan mutations");return 0;
}
