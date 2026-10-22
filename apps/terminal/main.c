#include "../common.h"
#include "../../libgatha/network.h"
static struct ui u;
static char command[256],cwd[256]="/",lines[80][80];static unsigned line_count;
static struct ui_text input={.data=command,.capacity=256,.focused=1};
static char transfer[196608];
static void draw(int busy) {
    ui_begin(&u);ui_rect(&u,0,0,u.width,u.height-80,0x102234);
    unsigned visible=(unsigned)(u.height-100)/20,start=line_count>visible?line_count-visible:0;
    for(unsigned i=start;i<line_count;++i)ui_label(&u,12,12+(int)(i-start)*20,lines[i],0xd4e9ee);
    ui_label(&u,12,u.height-70,cwd,UI_ACCENT);
    if(busy)ui_label(&u,12,u.height-38,"Connecting / transferring... Esc cancels",UI_INK);
    else ui_textbox(&u,12,u.height-46,u.width-24,34,&input);
    ui_end(&u);
}
static int network_wait(void) {
    if(ui_poll(&u)>0){if(u.e.type==G_KEY && u.e.pressed && u.e.code==1)return 1;draw(1);}
    return 0;
}
static void print(const char *s) {
    do {
        if(line_count==80){memmove(lines,lines+1,79*80);line_count=79;}
        unsigned n=0;while(*s && *s!='\n' && n<79)lines[line_count][n++]=*s++;
        lines[line_count][n]=0;g_puts("SHELL OUT: ");g_puts(lines[line_count]);g_puts("\n");++line_count;if(*s=='\n')++s;
    }while(*s);
}
static void result(const char *verb,int r){char status[80];app_status(status,verb,r);print(status);}
static void print_address(const char *label,uint32_t address) {
    char row[80],ip[16];g_ip_text(address,ip);g_copy(row,label,sizeof(row));g_copy(row+g_strlen(row),ip,sizeof(row)-(unsigned)g_strlen(row));print(row);
}
static void network_info(void) {
    struct g_network info;int r=g_network(G_NET_INFO,0,&info,sizeof(info));if(r<0){result("net",r);return;}
    print(info.state==2?"PCnet: connected (DHCP)":info.state==1?"PCnet: waiting for DHCP":"Offline: no supported network adapter");
    if(info.state==2){print_address("IP: ",info.address);print_address("Mask: ",info.mask);print_address("Gateway: ",info.gateway);print_address("DNS: ",info.dns);}
    char row[80];g_copy(row,"Packets received: ",80);g_decimal(row+g_strlen(row),(int)info.received);print(row);
    g_copy(row,"Packets sent: ",80);g_decimal(row+g_strlen(row),(int)info.sent);print(row);
}
static void download(const char *url,const char *destination) {
    print("Downloading HTTP. Esc cancels.");draw(1);
    void *data=0;unsigned size=0;int r=g_http_get(url,&data,&size,network_wait);
    if(!r)r=g_save(destination,data,size);
    g_free(data);result("wget",r);
    if(r>=0){char row[80];g_decimal(row,(int)size);g_copy(row+g_strlen(row)," bytes saved",40);print(row);}
    u.e.type=0;
}
static int copy_file(const char *source,const char *destination) {
    struct g_stat info;int r=g_stat(source,&info);if(r<0)return r;
    if(info.type!=G_FILE)return -21;
    void *data=g_malloc(info.size?info.size:1);if(!data)return -12;
    r=app_load(source,data,info.size);
    if(r>=0)r=app_save(destination,data,(unsigned)r);
    g_free(data);return r;
}
static void execute(void) {
    print(command);char *args[4]={0};unsigned argc=0;char *p=command;
    while(*p && argc<4){while(*p==' ')++p;if(!*p)break;args[argc++]=p;while(*p && *p!=' ')++p;if(*p)*p++=0;}
    if(!argc)return;
    const char *cmd=args[0],*a=argc>1?args[1]:"",*b=argc>2?args[2]:"";
    char path[256],other[256];app_path(path,cwd,a);app_path(other,cwd,b);
    if(!g_strcmp(cmd,"help"))print("ls cd cat mkdir rm cp mv echo clear ps mem uptime\nnet | nslookup domain | wget http://host/path destination\nhelp reboot shutdown\nPaths are absolute or relative. No quoting/redirection.");
    else if(!g_strcmp(cmd,"net"))network_info();
    else if(!g_strcmp(cmd,"nslookup")){uint32_t ip;draw(1);int r=*a?g_resolve(a,&ip,network_wait):-22;if(r)result("nslookup",r);else print_address("Address: ",ip);u.e.type=0;}
    else if(!g_strcmp(cmd,"wget")){if(*a && *b)download(a,other);else print("Usage: wget http://host/path destination");}
    else if(!g_strcmp(cmd,"clear"))line_count=0;
    else if(!g_strcmp(cmd,"echo")){char text[256]="";for(unsigned i=1;i<argc;++i){unsigned n=(unsigned)g_strlen(text);g_copy(text+n,args[i],256-n);n=(unsigned)g_strlen(text);if(n<255){text[n]=' ';text[n+1]=0;}}print(text);}
    else if(!g_strcmp(cmd,"ls")){struct g_dirent e;int r;unsigned i=0;while((r=g_readdir(path,i++,&e))>0){char row[80];g_copy(row,e.stat.type==G_DIR?"[dir] ":"      ",80);g_copy(row+6,e.name,74);print(row);}if(r<0)result("ls",r);}
    else if(!g_strcmp(cmd,"cd")){struct g_stat s;int r=g_stat(path,&s);if(!r && s.type==G_DIR)g_copy(cwd,path,256);else result("cd",r<0?r:-20);}
    else if(!g_strcmp(cmd,"cat")){int n=app_load(path,transfer,sizeof(transfer)-1);if(n>=0){transfer[n]=0;print(transfer);}else result("cat",n);}
    else if(!g_strcmp(cmd,"mkdir"))result("mkdir",*a?g_mkdir(path):-22);
    else if(!g_strcmp(cmd,"rm"))result("rm",*a?g_unlink(path):-22);
    else if(!g_strcmp(cmd,"mv"))result("mv",*a && *b?g_rename(path,other):-22);
    else if(!g_strcmp(cmd,"cp"))result("cp",*a && *b?copy_file(path,other):-22);
    else if(!g_strcmp(cmd,"ps")){struct g_process ps[16];unsigned n=app_processes(ps,16);for(unsigned i=0;i<n;++i){char row[80];char *end=g_decimal(row,(int)ps[i].pid);*end++=' ';g_copy(end,ps[i].name,50);print(row);}}
    else if(!g_strcmp(cmd,"mem") || !g_strcmp(cmd,"uptime")){struct g_info info;ui_info(&info);char row[80];g_decimal(row,(int)(cmd[0]=='m'?info.free_frames*4:info.ticks/100));g_copy(row+g_strlen(row),cmd[0]=='m'?" KiB free":" seconds since boot",40);print(row);}
    else if(!g_strcmp(cmd,"shutdown") || !g_strcmp(cmd,"reboot")){g_puts("SHELL: power request\n");g_call(SYS_POWER,cmd[0]=='r',0,0,0,0);}
    else print("Unknown command. Type help.");
    g_puts("SHELL: command ");g_puts(cmd);g_puts(" completed\n");
}
int app_main(unsigned argument) {
    (void)argument;if(ui_open(&u,"Terminal",660,430)<0)return 1;
    print("GathaShell / original user-mode command interpreter\nType help to list commands.");int first=1;
    for(;;){
        if(!ui_poll(&u)&&!first){g_sleep(2);continue;}first=0;
        if(u.e.type==G_KEY && u.e.pressed && u.e.ascii==10){execute();command[0]=0;input.cursor=input.anchor=input.scroll=0;}
        if(u.e.type==G_KEY && u.e.pressed && (u.e.mods&2) && u.e.ascii=='l')line_count=0;
        draw(0);
    }
}
