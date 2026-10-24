#include "document.h"
#include "../../libgatha/api.h"
#include "../../libgatha/memory.h"
static int same(const char *p,unsigned n,const char *s) {
    unsigned i=0;while(i<n&&s[i]){char c=p[i];if(c>='A'&&c<='Z')c+=32;if(c!=s[i])return 0;++i;}return i==n&&!s[i];
}
static int word(const char *p,const char *s) {return same(p,(unsigned)g_strlen(p),s);}
static int attr(const char *s,unsigned n,const char *key,char *out,unsigned cap) {
    return page_attribute(s,n,key,out,cap,1);
}
static int has(const char *s,unsigned n,const char *key) {char value[256];return attr(s,n,key,value,sizeof(value))!=0;}
static int option(const char *s,unsigned n,const char *key,const char *allowed) {
    char value[128];int r=attr(s,n,key,value,sizeof(value));return r<0||(r&&value[0]&&!word(value,allowed));
}
int page_form_tag(struct page *p,struct form_parser *state,const char *name,unsigned n,
                  const char *attrs,unsigned size,int closing,const char *base) {
    if(same(name,n,"form")) {
        state->button=-1;
        if(closing){state->form=-1;return 1;}
        if(state->form>=0||p->form_count==PAGE_FORMS){p->forms_blocked=1;return 1;}
        state->form=(int)p->form_count++;
        struct page_form *f=&p->forms[state->form];char action[G_URL_MAX];
        int r=attr(attrs,size,"action",action,sizeof(action));
        if(r<0||g_url(base,action,f->action))f->unsupported=1;
        if(option(attrs,size,"method","get")||option(attrs,size,"enctype","application/x-www-form-urlencoded")||
           option(attrs,size,"target","_self")||option(attrs,size,"accept-charset","utf-8"))f->unsupported=1;
        if(has(attrs,size,"onsubmit")||has(attrs,size,"novalidate"))f->unsupported=1;
        return 1;
    }
    int input=same(name,n,"input"),button=same(name,n,"button");
    if(button&&closing){state->button=-1;return 1;}
    if(closing)return 0;
    if(has(attrs,size,"form")&&(input||button||same(name,n,"select")||same(name,n,"textarea")))p->forms_blocked=1;
    if(state->form<0)return 0;
    struct page_form *f=&p->forms[state->form];
    if(same(name,n,"select")||same(name,n,"textarea")||same(name,n,"fieldset")||same(name,n,"object"))f->unsupported=1;
    if(!input&&!button)return 0;
    if(p->control_count==PAGE_CONTROLS){f->unsupported=1;return 1;}
    struct page_control *c=&p->controls[p->control_count++];
    c->form=(unsigned)state->form;c->maximum=255;c->offset=~0u;
    c->disabled=has(attrs,size,"disabled");c->readonly=has(attrs,size,"readonly");c->required=has(attrs,size,"required");
    char type[64];int r=attr(attrs,size,"type",type,sizeof(type));
    if(r<0)f->unsupported=1;
    if(!type[0])g_copy(type,button?"submit":"text",sizeof(type));
    if(word(type,"hidden")&&!button)c->type=FIELD_HIDDEN;
    else if(word(type,"submit"))c->type=FIELD_SUBMIT;
    else if(!button&&(word(type,"text")||word(type,"search")))c->type=FIELD_TEXT;
    else {c->type=FIELD_HIDDEN;if(!c->disabled)f->unsupported=1;}
    if(attr(attrs,size,"name",c->name,sizeof(c->name))<0||attr(attrs,size,"value",c->value,sizeof(c->value))<0)f->unsupported=1;
    if(has(attrs,size,"formaction")||has(attrs,size,"formmethod")||has(attrs,size,"formenctype")||
       has(attrs,size,"formtarget")||has(attrs,size,"formnovalidate")||has(attrs,size,"pattern")||has(attrs,size,"dirname")||
       has(attrs,size,"minlength")||has(attrs,size,"onclick")||has(attrs,size,"onchange")||has(attrs,size,"oninput"))f->unsupported=1;
    if(c->type==FIELD_HIDDEN&&!g_strcmp(c->name,"_charset_"))g_copy(c->value,"UTF-8",sizeof(c->value));
    char maximum[16];r=attr(attrs,size,"maxlength",maximum,sizeof(maximum));
    if(r<0)f->unsupported=1;
    if(r>0&&maximum[0]) {
        unsigned value=0;
        for(unsigned i=0;maximum[i];++i){if(maximum[i]<'0'||maximum[i]>'9'){f->unsupported=1;break;}if(value<256)value=value*10+(unsigned)(maximum[i]-'0');}
        c->maximum=value<255?value:255;
    }
    if(c->type==FIELD_TEXT&&g_strlen(c->value)>c->maximum)f->unsupported=1;
    if(c->type==FIELD_SUBMIT) {
        g_copy(c->caption,button?"":c->value,sizeof(c->caption));
        if(!button&&!c->caption[0])g_copy(c->caption,"Submit",sizeof(c->caption));
    }
    if(button){state->button=(int)p->control_count-1;state->caption_size=0;}
    return 1;
}
static int add(char *out,unsigned *at,char c) {if(*at>=G_URL_MAX-1)return -36;out[(*at)++]=c;out[*at]=0;return 0;}
static int encode(char *out,unsigned *at,const char *s) {
    static const char hex[]="0123456789ABCDEF";
    for(unsigned i=0;s[i];++i) {
        unsigned c=(unsigned char)s[i];int r;
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='*'||c=='-'||c=='.'||c=='_')r=add(out,at,(char)c);
        else if(c==' ')r=add(out,at,'+');
        else {r=add(out,at,'%');if(!r)r=add(out,at,hex[c>>4]);if(!r)r=add(out,at,hex[c&15]);}
        if(r)return r;
    }
    return 0;
}
int page_submit(const struct page *p,unsigned control,const char *current,char out[G_URL_MAX],int *invalid) {
    *invalid=-1;out[0]=0;
    if(control>=p->control_count)return -22;
    const struct page_control *source=&p->controls[control];unsigned form=source->form;
    if(source->disabled||source->type==FIELD_HIDDEN)return -11;
    if(form>=p->form_count||p->forms_blocked||p->truncated||p->forms[form].unsupported)return -95;
    int submit=(int)control;
    if(source->type==FIELD_TEXT) {
        submit=-1;unsigned fields=0;
        for(unsigned i=0;i<p->control_count;++i)if(p->controls[i].form==form) {
            if(p->controls[i].type==FIELD_SUBMIT&&submit<0)submit=(int)i;
            if(p->controls[i].type==FIELD_TEXT)++fields;
        }
        if(submit>=0&&p->controls[submit].disabled)return -11;
        if(submit<0&&fields!=1)return -11;
    }
    for(unsigned i=0;i<p->control_count;++i) {
        const struct page_control *c=&p->controls[i];
        if(c->form==form&&!c->disabled&&!c->readonly&&c->type==FIELD_TEXT&&c->required&&!c->value[0]){*invalid=(int)i;return -22;}
    }
    const char *action=p->forms[form].action;
    if(g_strlen(current)>=8&&!memcmp(current,"https://",8)&&memcmp(action,"https://",8))return -13;
    unsigned at=0;while(action[at]&&action[at]!='?'&&action[at]!='#'){out[at]=action[at];++at;}out[at]=0;
    if(add(out,&at,'?'))return -36;
    unsigned entries=0;
    for(unsigned i=0;i<p->control_count;++i) {
        const struct page_control *c=&p->controls[i];
        if(c->form!=form||c->disabled||!c->name[0]||(c->type==FIELD_SUBMIT&&(int)i!=submit))continue;
        if(entries++&&add(out,&at,'&'))return -36;
        if(encode(out,&at,c->name)||add(out,&at,'=')||encode(out,&at,c->value))return -36;
    }
    const char *fragment=action;while(*fragment&&*fragment!='#')++fragment;
    while(*fragment)if(add(out,&at,*fragment++))return -36;
    char checked[G_URL_MAX];int r=g_url("",out,checked);if(r)return r;
    g_copy(out,checked,G_URL_MAX);return 0;
}
