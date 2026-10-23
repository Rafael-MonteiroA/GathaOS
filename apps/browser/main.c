#include "../common.h"
#include "../../libgatha/network.h"
#include "document.h"
#define HISTORY 16
static struct ui u;
static struct page *page;
static char address[G_URL_MAX]="about:home",current[G_URL_MAX]="about:home",status[80]="Ready";
static struct ui_text location={.data=address,.capacity=sizeof(address)};
static struct {char url[G_URL_MAX];int scroll;} history[HISTORY];
static int history_count=1,history_at,scroll,busy,cancelled;
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
    " JavaScript, CSS, images, forms, cookies and downloads are not supported."
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
/* Returns a toolbar action or a one-based link index plus 10. */
static int draw(void) {
    int action=0;
    if(page->columns!=(u.width-64)/8)page_layout(page,(u.width-64)/8);
    clamp();ui_begin(&u);
    if(ui_button(&u,12,10,64,"Back"))action=1;
    if(ui_button(&u,84,10,80,"Forward"))action=2;
    if(ui_button(&u,172,10,72,"Reload"))action=3;
    if(ui_button(&u,252,10,64,"Stop"))action=4;
    if(ui_button(&u,324,10,64,"Home"))action=5;
    label(408,16,busy?"Connecting...":"Gatha Browser",UI_ACCENT,u.width-420);
    ui_textbox(&u,12,48,u.width-100,34,&location);
    if(ui_button(&u,u.width-78,50,66,"Go"))action=6;
    ui_line(&u,12,88,u.width-12,88,0xc7d0db);
    label(16,96,page->title[0]?page->title:"Untitled page",UI_INK,u.width-32);
    ui_rect(&u,12,118,u.width-46,visible()*20+4,0xffffff);
    int hover=-1;
    for(int row=0;row<visible()&&scroll+row<(int)page->row_count;++row) {
        struct page_row *line=&page->rows[scroll+row];unsigned at=line->start;
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
    int is_home=!g_strcmp(input,"about:home"),r=0;unsigned code=200;
    if(is_home)g_copy(url,input,sizeof(url));
    else {
        /* Address-bar shorthand defaults to HTTPS; links arrive resolved. */
        unsigned i=0;while(input[i]&&input[i]!=':'&&input[i]!='/')++i;
        if(input[i]!=':') {
            if(g_strlen(input)+8>=sizeof(request))r=-36;
            else {g_copy(request,"https://",sizeof(request));g_copy(request+8,input,sizeof(request)-8);r=g_url("",request,url);}
        } else r=g_url("",input,url);
    }
    if(!r&&!reload&&!is_home&&!g_url_request(current,old_request)&&!g_url_request(url,request)&&!g_strcmp(old_request,request)) {
        remember(url,target);g_copy(current,url,sizeof(current));
        scroll=target>=0?history[history_at].scroll:page_fragment(page,url);set_address();repaint();log_url("anchor",url);return;
    }
    struct page *next=0;
    if(!r){next=g_malloc(sizeof(*next));if(!next)r=-12;}
    if(!r) {
        busy=1;cancelled=0;g_copy(status,"Loading page... Escape or Stop cancels.",sizeof(status));repaint();
        if(is_home)page_parse(next,home,sizeof(home)-1,1,"https://example.com/");
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
    g_free(page);page=next;page_layout(page,(u.width-64)/8);
    g_copy(current,url,sizeof(current));set_address();
    if(reload)g_copy(history[history_at].url,url,G_URL_MAX);
    scroll=reload?saved:target>=0?history[history_at].scroll:page_fragment(page,url);
    if(code>=400){g_copy(status,"HTTP error ",sizeof(status));g_decimal(status+11,(int)code);}
    else g_copy(status,is_home?"Ready - basic HTML and text":!memcmp(url,"https://",8)?"HTTPS - certificate verified (limited TLS profile)":"HTTP - unencrypted connection",sizeof(status));
    if(page->truncated)g_copy(status,"Page display limit reached; remaining text omitted.",sizeof(status));
    repaint();log_url("loaded",current);log_url("title",page->title);
}
int app_main(unsigned argument) {
    (void)argument;
    page=g_malloc(sizeof(*page));if(!page)return 1;
    page_parse(page,home,sizeof(home)-1,1,"https://example.com/");g_copy(history[0].url,current,G_URL_MAX);
    if(ui_open(&u,"Gatha Browser",780,550)<0){g_free(page);return 1;}
    repaint();g_puts("BROWSER: ready\n");
    for(;;) {
        if(!ui_poll(&u)){g_sleep(2);continue;}
        int key=u.e.type==G_KEY&&u.e.pressed,action=0;
        if(key&&(u.e.mods&2)&&(u.e.ascii=='l'||u.e.ascii=='L')) {
            location.focused=1;location.anchor=0;location.cursor=(unsigned)g_strlen(address);u.e.type=0;
        } else if(key&&(u.e.mods&4)&&u.e.code==203)action=1;
        else if(key&&(u.e.mods&4)&&u.e.code==205)action=2;
        else if(key&&u.e.code==63)action=3;
        else if(key&&location.focused&&u.e.ascii==10)action=6;
        else if(key&&!location.focused) {
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
        if(action>10&&(unsigned)(action-11)<page->link_count)navigate(page->links[action-11],-1,0);
    }
}
