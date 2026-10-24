#include "document.h"
#include "../../libgatha/api.h"
#include "../../libgatha/memory.h"
static char lower(char c) {return c>='A'&&c<='Z'?(char)(c+32):c;}
static int space(unsigned c) {return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f';}
static int same(const char *p,unsigned n,const char *s) {
    unsigned i=0;while(i<n&&s[i]&&lower(p[i])==s[i])++i;return i==n&&!s[i];
}
static int starts(const char *p,unsigned n,const char *s) {
    unsigned i=0;while(s[i]){if(i>=n||lower(p[i])!=s[i])return 0;++i;}return 1;
}
static char glyph(unsigned c) {
    if(c>=32&&c<127)return (char)c;
    if(c==160)return ' ';
    if(c>=192&&c<=197)return 'A';if(c>=224&&c<=229)return 'a';
    if(c==199)return 'C';if(c==231)return 'c';
    if(c>=200&&c<=203)return 'E';if(c>=232&&c<=235)return 'e';
    if(c>=204&&c<=207)return 'I';if(c>=236&&c<=239)return 'i';
    if(c==209)return 'N';if(c==241)return 'n';
    if((c>=210&&c<=214)||c==216)return 'O';if((c>=242&&c<=246)||c==248)return 'o';
    if(c>=217&&c<=220)return 'U';if(c>=249&&c<=252)return 'u';
    if(c==221)return 'Y';if(c==253||c==255)return 'y';
    if(c==8211||c==8212)return '-';if(c==8216||c==8217)return '\'';
    if(c==8220||c==8221)return '"';if(c==8226)return '*';
    return '?';
}
static unsigned utf8(const char *s,unsigned n,unsigned *used) {
    unsigned c=(unsigned char)*s;*used=1;if(c<128)return c;
    unsigned count=c>=194&&c<=223?2:c>=224&&c<=239?3:c>=240&&c<=244?4:0;
    if(!count||count>n)return '?';
    unsigned value=c&((1u<<(7-count))-1);
    for(unsigned i=1;i<count;++i){unsigned b=(unsigned char)s[i];if((b&192)!=128)return '?';value=(value<<6)|(b&63);}
    if(value<(count==2?128u:count==3?2048u:65536u)||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return '?';
    *used=count;return value;
}
static unsigned character(const char *s,unsigned n,unsigned *used,int entities) {
    if(entities&&*s=='&') {
        unsigned end=1;while(end<n&&end<16&&s[end]!=';'&&!space((unsigned char)s[end])&&s[end]!='<')++end;
        if(end<n&&s[end]==';') {
            unsigned c=0;
            if(s[1]=='#') {
                unsigned at=2,base=10;if(at<end&&(s[at]=='x'||s[at]=='X')){base=16;++at;}
                unsigned digits=0;
                for(;at<end;++at){unsigned d=s[at]>='0'&&s[at]<='9'?(unsigned)(s[at]-'0'):lower(s[at])>='a'&&lower(s[at])<='f'?(unsigned)(lower(s[at])-'a'+10):16;
                    if(d>=base||c>0x10ffff/base){c=0;break;}c=c*base+d;++digits;}
                if(!digits)c=0;
            } else {
                const char *names[]={"amp","lt","gt","quot","apos","nbsp","copy","ndash","mdash","bull"};
                const unsigned codes[]={38,60,62,34,39,160,169,8211,8212,8226};
                for(unsigned i=0;i<10;++i)if(same(s+1,end-1,names[i])){c=codes[i];break;}
            }
            if(c){*used=end+1;return c;}
        }
    }
    return utf8(s,n,used);
}
static void emit(struct page *p,char c,unsigned link,int heading,int collapse) {
    if(collapse&&space((unsigned char)c)){
        if(!p->size||p->text[p->size-1]==' '||p->text[p->size-1]=='\n')return;c=' ';
    }
    if(p->size==PAGE_TEXT-1){p->truncated=1;return;}
    p->text[p->size]=c;p->link[p->size]=(uint16_t)link;p->heading[p->size]=(uint8_t)heading;++p->size;
}
static void newline(struct page *p) {
    if(p->size&&p->text[p->size-1]==' ')--p->size;
    if(p->size&&p->text[p->size-1]!='\n')emit(p,'\n',0,0,0);
}
static int bad_attribute(char *out) {out[0]=0;return -1;}
int page_attribute(const char *s,unsigned n,const char *name,char *out,unsigned capacity,int exact) {
    unsigned i=0;out[0]=0;
    while(i<n) {
        while(i<n&&(space((unsigned char)s[i])||s[i]=='/'))++i;
        unsigned start=i;while(i<n&&!space((unsigned char)s[i])&&s[i]!='='&&s[i]!='/')++i;
        unsigned size=i-start;if(!size){++i;continue;}
        while(i<n&&space((unsigned char)s[i]))++i;
        if(i==n||s[i]!='='){if(same(s+start,size,name))return 1;continue;}
        ++i;while(i<n&&space((unsigned char)s[i]))++i;
        char quote=i<n&&(s[i]=='\''||s[i]=='"')?s[i++]:0;
        unsigned value=i;while(i<n&&(quote?s[i]!=quote:!space((unsigned char)s[i])))++i;
        unsigned end=i;if(quote&&i<n)++i;
        if(!same(s+start,size,name))continue;
        unsigned to=0;
        while(value<end){
            if((exact||!g_strcmp(name,"href"))&&(unsigned char)s[value]>=127)return bad_attribute(out);
            unsigned used,c=character(s+value,end-value,&used,1);
            if(exact&&s[value]=='&'&&value+1<end&&s[value+1]=='#'&&used==1)return bad_attribute(out);
            value+=used;
            if(to+1>=capacity)return bad_attribute(out);
            /* Text fallback must never rewrite a link's destination. */
            if((exact&&(c<32||c>=127))||(!g_strcmp(name,"href")&&(c<=32||c>=127)))return bad_attribute(out);
            out[to++]=c<128?(char)c:glyph(c);
        }
        out[to]=0;return 1;
    }
    out[0]=0;return 0;
}
static int attribute(const char *s,unsigned n,const char *name,char *out,unsigned capacity) {
    return page_attribute(s,n,name,out,capacity,0);
}
static int block(const char *s,unsigned n) {
    const char *names[]={"p","div","section","article","header","footer","main","nav","ul","ol","li","pre","blockquote","table","tr","dl","dt","dd","hr"};
    for(unsigned i=0;i<sizeof(names)/sizeof(*names);++i)if(same(s,n,names[i]))return 1;
    return n==2&&lower(s[0])=='h'&&s[1]>='1'&&s[1]<='6';
}
void page_parse(struct page *p,const void *data,unsigned size,int html,const char *base) {
    memset(p,0,sizeof(*p));const char *s=data;unsigned i=0,link=0,title_size=0;
    int heading=0,pre=0,head=0,title=0;char raw[16]="";
    struct form_parser form={.form=-1,.button=-1};
    while(i<size&&!p->truncated) {
        if(raw[0]) {
            unsigned n=(unsigned)g_strlen(raw);
            if(i+n+3>size||s[i]!='<'||s[i+1]!='/'||!same(s+i+2,n,raw)||
               !(space((unsigned char)s[i+n+2])||s[i+n+2]=='>')){++i;continue;}
        }
        if(html&&s[i]=='<') {
            if(i+4<=size&&!memcmp(s+i,"<!--",4)) {
                i+=4;while(i+3<=size&&memcmp(s+i,"-->",3))++i;i=i+3<=size?i+3:size;continue;
            }
            unsigned begin=i+1;int closing=begin<size&&s[begin]=='/';if(closing)++begin;
            unsigned end=begin;while(end<size&&((lower(s[end])>='a'&&lower(s[end])<='z')||(s[end]>='0'&&s[end]<='9')))++end;
            unsigned name_size=end-begin,finish=end;char quote=0;
            while(finish<size){char c=s[finish];if(quote){if(c==quote)quote=0;}else if(c=='\''||c=='"')quote=c;else if(c=='>')break;++finish;}
            if(finish==size){p->forms_blocked=1;if(raw[0]||head)break;emit(p,'<',link,heading,0);++i;html=0;continue;}
            if(raw[0]){if(closing&&same(s+begin,name_size,raw))raw[0]=0;i=finish+1;continue;}
            if(!name_size){i=finish+1;continue;}
            if(same(s+begin,name_size,"script")||same(s+begin,name_size,"style")||same(s+begin,name_size,"iframe")||same(s+begin,name_size,"template")) {
                if(same(s+begin,name_size,"template"))p->forms_blocked=1;
                if(!closing){unsigned n=name_size<15?name_size:15;for(unsigned j=0;j<n;++j)raw[j]=lower(s[begin+j]);raw[n]=0;}i=finish+1;continue;
            }
            if(same(s+begin,name_size,"head"))head=!closing;
            if(same(s+begin,name_size,"body"))head=0;
            if(same(s+begin,name_size,"title"))title=!closing;
            if(!closing&&same(s+begin,name_size,"base"))p->forms_blocked=1;
            if(!head&&!title) {
                unsigned controls=p->control_count;
                if(page_form_tag(p,&form,s+begin,name_size,s+end,finish-end,closing,base)) {
                    newline(p);
                    if(p->control_count>controls&&p->controls[controls].type!=FIELD_HIDDEN) {
                        p->controls[controls].offset=p->size;
                        emit(p,1,0,0,0);emit(p,'\n',0,0,0);emit(p,' ',0,0,0);emit(p,'\n',0,0,0);
                    }
                    i=finish+1;continue;
                }
                if(block(s+begin,name_size)||same(s+begin,name_size,"br"))newline(p);
                if(name_size==2&&lower(s[begin])=='h'&&s[begin+1]>='1'&&s[begin+1]<='6')heading=!closing;
                if(same(s+begin,name_size,"pre"))pre=!closing;
                if(!closing) {
                    char value[G_URL_MAX];int id=attribute(s+end,finish-end,"id",value,64);
                    if(id==1&&value[0]&&p->anchor_count<128){struct page_anchor *a=&p->anchors[p->anchor_count++];g_copy(a->name,value,64);a->offset=p->size;}
                    if(same(s+begin,name_size,"li")){emit(p,'*',0,0,0);emit(p,' ',0,0,0);}
                    if(same(s+begin,name_size,"td")||same(s+begin,name_size,"th"))emit(p,' ',0,0,1);
                    if(same(s+begin,name_size,"img")) {
                        if(attribute(s+end,finish-end,"alt",value,160)==1){emit(p,'[',link,0,0);for(unsigned j=0;value[j];++j)emit(p,glyph((unsigned char)value[j]),link,0,1);emit(p,']',link,0,0);}
                    }
                    if(same(s+begin,name_size,"a")) {
                        link=0;
                        if(attribute(s+end,finish-end,"href",value,sizeof(value))==1&&p->link_count<PAGE_LINKS &&
                           !g_url(base,value,p->links[p->link_count]))link=++p->link_count;
                    }
                } else if(same(s+begin,name_size,"a"))link=0;
            }
            i=finish+1;continue;
        }
        if(raw[0]){++i;continue;}
        unsigned used,c=character(s+i,size-i,&used,html);i+=used;
        if(form.button>=0) {
            struct page_control *control=&p->controls[form.button];char ch=space(c)?' ':glyph(c);
            if(form.caption_size<sizeof(control->caption)-1&&(ch!=' '||!form.caption_size||control->caption[form.caption_size-1]!=' '))
                control->caption[form.caption_size++]=ch;
        } else if(title) {
            if(title_size<sizeof(p->title)-1){char ch=space(c)?' ':glyph(c);if(ch!=' '||!title_size||p->title[title_size-1]!=' ')p->title[title_size++]=ch;}
        } else if(!head) {
            if(!html||pre) {if(c=='\r')continue;if(c=='\n')emit(p,'\n',link,heading,0);else if(c=='\t'){for(int j=0;j<4;++j)emit(p,' ',link,heading,0);}else emit(p,glyph(c),link,heading,0);}
            else emit(p,space(c)?' ':glyph(c),link,heading,1);
        }
    }
    p->text[p->size]=0;
}
void page_layout(struct page *p,int columns) {
    if(columns<1)columns=1;if(columns>256)columns=256;p->columns=columns;p->row_count=0;
    unsigned at=0;
    while(at<p->size&&p->row_count<PAGE_ROWS) {
        unsigned start=at,end=at,last_space=at;
        while(end<p->size&&p->text[end]!='\n'&&end-start<(unsigned)columns){if(p->text[end]==' ')last_space=end;++end;}
        unsigned next=end;
        if(end<p->size&&p->text[end]=='\n')++next;
        else if(end<p->size&&last_space>start){end=last_space;next=last_space+1;}
        p->rows[p->row_count++]=(struct page_row){start,end};at=next;
    }
    if(at<p->size)p->truncated=1;
}
int page_fragment(const struct page *p,const char *url) {
    const char *fragment=url;while(*fragment&&*fragment!='#')++fragment;
    if(!*fragment||!fragment[1])return 0;++fragment;
    for(unsigned i=0;i<p->anchor_count;++i)if(!g_strcmp(fragment,p->anchors[i].name)){
        for(unsigned row=0;row<p->row_count;++row)if(p->rows[row].end>=p->anchors[i].offset)return (int)row;
    }
    return 0;
}
int page_type(const char *type) {
    unsigned n=(unsigned)g_strlen(type),end=0;while(end<n&&type[end]!=';'&&type[end]!=' ')++end;
    int html=same(type,end,"text/html");if(!html&&!same(type,end,"text/plain"))return -95;
    unsigned at=end;while(at<n&&(type[at]==';'||space((unsigned char)type[at])))++at;
    if(at<n) {
        if(!starts(type+at,n-at,"charset="))return -95;at+=8;
        char quote=at<n&&(type[at]=='"'||type[at]=='\'')?type[at++]:0;
        unsigned begin=at;while(at<n&&(quote?type[at]!=quote:!space((unsigned char)type[at])&&type[at]!=';'))++at;
        if(!same(type+begin,at-begin,"utf-8")&&!same(type+begin,at-begin,"us-ascii"))return -95;
        if(quote){if(at==n)return -95;++at;}while(at<n&&space((unsigned char)type[at]))++at;if(at<n)return -95;
    }
    return html;
}
