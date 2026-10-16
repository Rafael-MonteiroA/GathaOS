#include "../common.h"
static struct ui u;
static void date_text(char out[32],unsigned seconds){
    unsigned days=seconds/86400,year=1970,month=0;
    for(;;){unsigned length=(year%4==0 && (year%100 || year%400==0))?366:365;if(days<length)break;days-=length;++year;}
    const unsigned months[]={31,28,31,30,31,30,31,31,30,31,30,31};
    while(month<11){unsigned length=months[month]+(month==1 && year%4==0 && (year%100 || year%400==0));if(days<length)break;days-=length;++month;}
    g_copy(out,"0000-00-00 00:00:00 UTC",32);
    unsigned values[]={year,month+1,days+1,seconds/3600%24,seconds/60%60,seconds%60};
    const unsigned offsets[]={0,5,8,11,14,17},digits[]={4,2,2,2,2,2};
    for(unsigned i=0;i<6;++i)for(unsigned j=0;j<digits[i];++j){out[offsets[i]+digits[i]-j-1]=(char)('0'+values[i]%10);values[i]/=10;}
}
int app_main(unsigned argument) {
    (void)argument;if(ui_open(&u,"Settings",500,330)<0)return 1;int first=1,wall=0;unsigned color=UI_ACCENT,last=0;
    for(;;){int event=ui_poll(&u);if(!event&&!first && g_ticks()-last<100){g_sleep(5);continue;}first=0;last=g_ticks();
        struct g_info info;ui_info(&info);ui_begin(&u);
        ui_label(&u,20,20,"Make GathaOS yours",UI_INK);ui_label(&u,20,58,"Wallpaper",UI_INK);
        const char *names[]={"Ocean","Dusk","Meadow"};
        for(int i=0;i<3;++i)if(ui_button(&u,20+i*150,86,136,names[i])){wall=i;g_call(SYS_THEME,color,(unsigned)wall,0,0,0);g_puts("SETTINGS: wallpaper changed\n");}
        ui_label(&u,20,136,"Window accent",UI_INK);unsigned colors[]={0x426d9c,0x71699b,0x568377};
        for(int i=0;i<3;++i){ui_rect(&u,20+i*150,164,136,32,colors[i]);if(ui_hit(&u,20+i*150,164,136,32)){color=colors[i];g_call(SYS_THEME,color,(unsigned)wall,0,0,0);g_puts("SETTINGS: accent changed\n");}}
        app_number(&u,20,222,"Screen width: ",info.width);app_number(&u,260,222,"Height: ",info.height);
        char date[32];date_text(date,info.seconds);ui_label(&u,20,252,date,UI_INK);
        ui_label(&u,20,290,"Boot resolution / session colors",0x61768a);ui_end(&u);
    }
}
