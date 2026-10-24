#include "../common.h"
#include "../../libgatha/network.h"
#include "document.h"
#define HISTORY 16
static struct ui u;
static struct page *page;
static char address[G_URL_MAX]="about:home",current[G_URL_MAX]="about:home",status[80]="Ready";
static struct ui_text location={.data=address,.capacity=sizeof(address)};
static struct ui_text fields[PAGE_CONTROLS];
static int focused=-1;
static struct {char url[G_URL_MAX];int scroll;} history[HISTORY];
static int history_count=1,history_at,scroll,busy,cancelled,form_navigation;
static const char demo[]=
    "<title>Try a form</title><p>Offline form demo - no data is sent to the internet.</p>"
    "<form action='/search'><p>Type a search below, then press Enter or Preview:</p>"
    "<input type='search' name='q' required maxlength='120'>"
    "<input type='hidden' name='source' value='gatha'>"
    "<button type='submit' name='action' value='preview'>Preview</button></form>"
    "<p>This demonstrates editing and form encoding, not Google search.</p>"
    "<p>Tab moves between fields and buttons. Ctrl+L selects the address.</p>";
static const char home[]=
    "<title>Gatha Browser</title><h1>Welcome to the web</h1>"
    "<p>A little window to the internet, made for GathaOS.</p>"
    "<h2>Start exploring</h2><p><a href='http://example.com/'>Example Domain - a simple web page</a></p>"
    "<p><a href='https://www.google.com/robots.txt'>Google - public text over HTTPS</a></p>"
    "<h2>Your way around</h2><p>Type an http:// or https:// address and press Enter."
    " Click blue links to open them. Use Back, Forward and Reload to navigate.</p>"
    "<p>Scroll with Up, Down, Page Up, Page Down or the bar on the right."
    " Ctrl+L selects the address. Escape or Stop cancels a request.</p>"
    "<h2>A simple browser</h2><p>This version reads text and basic HTML."
    " GET forms support text fields and submit buttons."
    " Compressed web pages open automatically."
    " JavaScript, CSS, images, cookies and downloads are not supported."
    " Google search and other modern web apps are not supported yet.</p>"
    "<p>HTTPS checks certificates using the limited GathaOS TLS profile."
    " Some secure sites are not compatible. HTTP is unencrypted.</p>";
static void log_url(const char *event,const char *url) {g_puts("BROWSER: ");g_puts(event);g_puts(" ");g_puts(url);g_puts("\n");}
static int visible(void) {int rows=(u.height-154)/20;return rows>0?rows:1;}
static void clamp(void) {int max=(int)page->row_count-visible();if(max<0)max=0;if(scroll>max)scroll=max;if(scroll<0)scroll=0;}
static void label(int x,int y,const char *text,unsigned color,int width) {
    char shown[128];unsigned n=width>0?(unsigned)width/8:0;if(n>127)n=127;
    g_copy(shown,text,n+1);ui_label(&u,x,y,shown,color);
}
static void clear_focus(void) {
    focused=-1;for(unsigned i=0;i<PAGE_CONTROLS;++i)fields[i].focused=0;
}
static void reset_fields(void) {
    clear_focus();memset(fields,0,sizeof(fields));
    for(unsigned i=0;i<page->control_count;++i) {
        fields[i].data=page->controls[i].value;fields[i].capacity=page->controls[i].maximum+1;
    }
}
static void focus_field(int index) {
    clear_focus();location.focused=0;focused=index;fields[index].focused=1;
    fields[index].cursor=fields[index].anchor=(unsigned)g_strlen(fields[index].data);
    for(unsigned row=0;row<page->row_count;++row)if(page->rows[row].start==page->controls[index].offset) {
        if((int)row<scroll)scroll=(int)row;
        if((int)row+2>scroll+visible())scroll=(int)row+2-visible();break;
    }
}
static void tab_field(int backwards) {
    int index=focused<0&&backwards?(int)page->control_count:focused;
    for(unsigned i=0;i<=page->control_count;++i) {
        index+=backwards?-1:1;
        if(index<0||index>=(int)page->control_count){clear_focus();location.focused=1;return;}
        struct page_control *c=&page->controls[index];
        if(c->type!=FIELD_HIDDEN&&!c->disabled){focus_field(index);return;}
    }
}
static int draw_field(unsigned index,int row) {
    struct page_control *c=&page->controls[index];int x=20,y=120+row*20,w=u.width-76;
    if(w>480)w=480;if(row+1>=visible())return 0;
    if(c->type==FIELD_TEXT) {
        struct g_event saved=u.e;
        if(c->disabled||(c->readonly&&u.e.type==G_KEY))u.e.type=0;
        ui_textbox(&u,x,y,w,34,&fields[index]);u.e=saved;
        if(fields[index].focused)focused=(int)index;
        if(c->disabled){ui_rect(&u,x+1,y+1,w-2,32,0xe5eaf0);label(x+8,y+8,c->value,0x788795,w-16);}
    } else {
        if(w>200)w=200;struct g_event saved=u.e;if(c->disabled)u.e.type=0;
        char caption[64];g_copy(caption,c->caption[0]?c->caption:"Submit",(unsigned)(w-16)/8+1);
        int hit=ui_button(&u,x,y,w,caption);u.e=saved;
        if(c->disabled){ui_rect(&u,x+1,y+1,w-2,26,0xe5eaf0);label(x+8,y+6,caption,0x788795,w-16);}
        if(focused==(int)index)ui_line(&u,x+4,y+26,x+w-5,y+26,UI_ACCENT);
        if(hit&&!c->disabled){focus_field((int)index);return 1000+(int)index;}
    }
    return 0;
}
/* Returns a toolbar action or a one-based link index plus 10. */
static int draw(void) {
    int action=0;
    if(page->columns!=(u.width-64)/8)page_layout(page,(u.width-64)/8);
    if(u.e.type==G_MOUSE&&u.e.pressed==1)clear_focus();
    clamp();ui_begin(&u);
    if(ui_button(&u,12,10,64,"Back"))action=1;
    if(ui_button(&u,84,10,80,"Forward"))action=2;
    if(ui_button(&u,172,10,72,"Reload"))action=3;
    if(ui_button(&u,252,10,64,"Stop"))action=4;
    if(ui_button(&u,324,10,64,"Home"))action=5;
    if(ui_button(&u,408,10,112,"Forms demo"))action=8;
    label(536,16,busy?"Connecting...":"Gatha Browser",UI_ACCENT,u.width-548);
    ui_textbox(&u,12,48,u.width-100,34,&location);
    if(ui_button(&u,u.width-78,50,66,"Go"))action=6;
    ui_line(&u,12,88,u.width-12,88,0xc7d0db);
    label(16,96,page->title[0]?page->title:"Untitled page",UI_INK,u.width-32);
    ui_rect(&u,12,118,u.width-46,visible()*20+4,0xffffff);
    int hover=-1;
    for(int row=0;row<visible()&&scroll+row<(int)page->row_count;++row) {
        struct page_row *line=&page->rows[scroll+row];unsigned at=line->start;
        if(page->text[at]==1) {
            for(unsigned i=0;i<page->control_count;++i)if(page->controls[i].offset==at){int hit=draw_field(i,row);if(hit)action=hit;break;}
            continue;
        }
        while(at<line->end) {
            unsigned start=at,link=page->link[at],heading=page->heading[at];char text[64];
            while(at<line->end&&at-start<63&&page->link[at]==link&&page->heading[at]==heading)++at;
            unsigned count=at-start;memcpy(text,page->text+start,count);text[count]=0;
            int x=20+(int)(start-line->start)*8,y=120+row*20;
            unsigned color=link?0x245d9c:heading?UI_ACCENT:UI_INK;ui_label(&u,x,y,text,color);
            if(link){ui_line(&u,x,y+16,x+(int)count*8-1,y+16,color);
                if(u.e.type==G_MOUSE&&u.e.x>=x&&u.e.x<x+(int)count*8&&u.e.y>=y&&u.e.y<y+20)hover=(int)link-1;
                if(ui_hit(&u,x,y,(int)count*8,20))action=(int)link+10;
            }
        }
    }
    int old=scroll;ui_scrollbar(&u,u.width-32,118,visible()*20+4,&scroll,(int)page->row_count,visible());
    if(old!=scroll)action=7;
    label(12,u.height-22,hover>=0&&!busy?page->links[hover]:status,UI_INK,u.width-24);
    ui_end(&u);return action;
}
static void repaint(void) {u.e.type=0;(void)draw();}
static int wait_network(void) {
    int redraw=0;
    for(int i=0;i<16&&ui_poll(&u)>0;++i) {
        if((u.e.type==G_KEY&&u.e.pressed&&u.e.code==1)||ui_hit(&u,252,10,64,28))cancelled=1;
        if(u.e.type==G_RESIZE)redraw=1;
    }
    if(redraw)repaint();return cancelled;
}
static int redirect(unsigned code) {return code==301||code==302||code==303||code==307||code==308;}
static int fetch(char *url,struct page *next,unsigned *code) {
    char request[G_URL_MAX],target[G_URL_MAX];struct g_http_response response;
    for(unsigned hops=0;hops<=5;++hops) {
        int r=g_url_request(url,request);if(r)return r;
        log_url("request",request);
        r=g_http_fetch(request,&response,524288,wait_network);if(r)return r;
        if(redirect(response.status)) {
            g_free(response.data);
            if(hops==5||!response.location[0])return -40;
            if((r=g_url(url,response.location,target)))return r;
            if(!memcmp(url,"https://",8)&&memcmp(target,"https://",8))return -13;
            g_copy(url,target,G_URL_MAX);continue;
        }
        int html=page_type(response.type);
        if(response.status==204)html=0;
        if(html<0){g_free(response.data);return html;}
        page_parse(next,response.data,response.size,html,url);g_free(response.data);
        *code=response.status;return 0;
    }
    return -40;
}
static void set_address(void) {
    g_copy(address,current,sizeof(address));location.cursor=location.anchor=(unsigned)g_strlen(address);location.scroll=0;location.focused=0;
}
static void remember(const char *url,int target) {
    history[history_at].scroll=scroll;
    if(target>=0)history_at=target;
    else {
        history_count=history_at+1;
        if(history_count==HISTORY){memmove(history,history+1,sizeof(history[0])*(HISTORY-1));--history_count;}
        history_at=history_count++;history[history_at].scroll=0;
    }
    g_copy(history[history_at].url,url,G_URL_MAX);
}
static void navigate(const char *input,int target,int reload) {
    char url[G_URL_MAX],request[G_URL_MAX],old_request[G_URL_MAX];
    int is_home=!g_strcmp(input,"about:home"),is_demo=!g_strcmp(input,"about:forms")||
        (g_strlen(input)>=12&&!memcmp(input,"about:forms?",12)),r=0;unsigned code=200;
    if(is_home||is_demo){if(g_strlen(input)>=sizeof(url))r=-36;else g_copy(url,input,sizeof(url));}
    else {
        /* Address-bar shorthand defaults to HTTPS; links arrive resolved. */
        unsigned i=0;while(input[i]&&input[i]!=':'&&input[i]!='/')++i;
        if(input[i]!=':') {
            if(g_strlen(input)+8>=sizeof(request))r=-36;
            else {g_copy(request,"https://",sizeof(request));g_copy(request+8,input,sizeof(request)-8);r=g_url("",request,url);}
        } else r=g_url("",input,url);
    }
    if(!r&&!reload&&!form_navigation&&!is_home&&!is_demo&&g_strcmp(url,current)&&!g_url_request(current,old_request)&&!g_url_request(url,request)&&!g_strcmp(old_request,request)) {
        remember(url,target);g_copy(current,url,sizeof(current));
        scroll=target>=0?history[history_at].scroll:page_fragment(page,url);set_address();repaint();log_url("anchor",url);return;
    }
    struct page *next=0;
    if(!r){next=g_malloc(sizeof(*next));if(!next)r=-12;}
    if(!r) {
        busy=1;cancelled=0;g_copy(status,"Loading page... Escape or Stop cancels.",sizeof(status));repaint();
        if(is_home)page_parse(next,home,sizeof(home)-1,1,"https://example.com/");
        else if(is_demo) {
            if(url[11]=='?') {
                char result[G_URL_MAX+128];g_copy(result,"Local form preview\nNo network request was made.\n\nEncoded GET query:\n",sizeof(result));
                g_copy(result+g_strlen(result),url+12,sizeof(result)-(unsigned)g_strlen(result));
                page_parse(next,result,(unsigned)g_strlen(result),0,"");g_copy(next->title,"Local form preview",sizeof(next->title));
            } else page_parse(next,demo,sizeof(demo)-1,1,"http://form.invalid/");
        }
        else r=fetch(url,next,&code);
        busy=0;
    }
    if(r) {
        g_free(next);
        if(r==-40)g_copy(status,"Redirect limit reached or missing destination.",sizeof(status));
        else if(r==-13)g_copy(status,"Blocked redirect from HTTPS to unencrypted HTTP.",sizeof(status));
        else app_status(status,"Open",r);
        set_address();log_url("error",status);repaint();return;
    }
    int saved=scroll;
    if(!reload)remember(url,target);
    g_free(page);page=next;page_layout(page,(u.width-64)/8);reset_fields();
    g_copy(current,url,sizeof(current));set_address();
    if(reload)g_copy(history[history_at].url,url,G_URL_MAX);
    scroll=reload?saved:target>=0?history[history_at].scroll:page_fragment(page,url);
    if(code>=400){g_copy(status,"HTTP error ",sizeof(status));g_decimal(status+11,(int)code);}
    else g_copy(status,is_demo?"Offline form demo - no network request":is_home?"Ready - basic HTML and text":!memcmp(url,"https://",8)?"HTTPS - certificate verified (limited TLS profile)":"HTTP - unencrypted connection",sizeof(status));
    if(page->truncated)g_copy(status,"Page display limit reached; remaining text omitted.",sizeof(status));
    repaint();log_url("loaded",current);log_url("title",page->title);
}
static void submit_form(unsigned index) {
    char url[G_URL_MAX];int invalid,r=page_submit(page,index,current,url,&invalid);
    if(r) {
        if(r==-95)g_copy(status,"This form needs unsupported features; nothing was sent.",sizeof(status));
        else if(r==-22&&invalid>=0){g_copy(status,"Fill in the required field before submitting.",sizeof(status));focus_field(invalid);}
        else if(r==-13)g_copy(status,"Blocked form from HTTPS to unencrypted HTTP.",sizeof(status));
        else if(r==-11)g_copy(status,"Use an enabled submit button for this form.",sizeof(status));
        else if(r==-36)g_copy(status,"Form is too large for this browser; nothing was sent.",sizeof(status));
        else app_status(status,"Form",r);
        repaint();log_url("form blocked",status);return;
    }
    /* A GET submission must fetch even when its URL matches the current page. */
    if(!g_strcmp(current,"about:forms")) {
        char local[G_URL_MAX];const char *query=url;while(*query&&*query!='?')++query;
        g_copy(local,"about:forms",sizeof(local));g_copy(local+11,query,sizeof(local)-11);
        log_url("form preview",local);navigate(local,-1,0);return;
    }
    log_url("form GET",url);form_navigation=1;navigate(url,-1,0);form_navigation=0;
}
int app_main(unsigned argument) {
    (void)argument;
    page=g_malloc(sizeof(*page));if(!page)return 1;
    page_parse(page,home,sizeof(home)-1,1,"https://example.com/");g_copy(history[0].url,current,G_URL_MAX);
    reset_fields();
    if(ui_open(&u,"Gatha Browser",780,550)<0){g_free(page);return 1;}
    repaint();g_puts("BROWSER: ready\n");
    for(;;) {
        if(!ui_poll(&u)){g_sleep(2);continue;}
        int key=u.e.type==G_KEY&&u.e.pressed,action=0;
        if(key&&(u.e.mods&2)&&(u.e.ascii=='l'||u.e.ascii=='L')) {
            clear_focus();location.focused=1;location.anchor=0;location.cursor=(unsigned)g_strlen(address);u.e.type=0;
        } else if(key&&(u.e.mods&4)&&u.e.code==203)action=1;
        else if(key&&(u.e.mods&4)&&u.e.code==205)action=2;
        else if(key&&u.e.code==63)action=3;
        else if(key&&u.e.code==15){tab_field((u.e.mods&1)!=0);u.e.type=0;}
        else if(key&&location.focused&&u.e.ascii==10)action=6;
        else if(key&&focused>=0&&(u.e.ascii==10||(u.e.ascii==' '&&page->controls[focused].type==FIELD_SUBMIT))){action=1000+focused;u.e.type=0;}
        else if(key&&!location.focused&&focused<0) {
            if(u.e.code==200)--scroll;if(u.e.code==208)++scroll;
            if(u.e.code==201)scroll-=visible();if(u.e.code==209)scroll+=visible();
            if(u.e.code==199)scroll=0;if(u.e.code==207)scroll=(int)page->row_count;
        }
        int clicked=draw();if(clicked)action=clicked;
        if(action==1&&history_at>0)navigate(history[history_at-1].url,history_at-1,0);
        if(action==2&&history_at+1<history_count)navigate(history[history_at+1].url,history_at+1,0);
        if(action==3)navigate(current,-1,1);
        if(action==5)navigate("about:home",-1,0);
        if(action==6)navigate(address,-1,0);
        if(action==7)repaint();
        if(action==8)navigate("about:forms",-1,0);
        if(action>=1000&&(unsigned)(action-1000)<page->control_count)submit_form((unsigned)(action-1000));
        else if(action>10&&(unsigned)(action-11)<page->link_count)navigate(page->links[action-11],-1,0);
    }
}
