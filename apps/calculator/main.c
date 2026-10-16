#include "../common.h"
static struct ui u;
static char display[64]="0";static int64_t stored;static char operation;static int fresh=1;
static int64_t number(void) {
    const char *p=display;int negative=*p=='-';if(negative)++p;int64_t n=0;unsigned decimals=0;int dot=0;
    while(*p){if(*p=='.'){dot=1;++p;continue;}if(*p<'0'||*p>'9')return 0;if(!dot||decimals<4){n=n*10+*p-'0';if(dot)++decimals;}++p;}
    while(decimals++<4)n*=10;return negative?-n:n;
}
static void show(int64_t n) {
    if(n>999999999 || n< -999999999){g_copy(display,"Overflow",64);return;}
    char *p=display;if(n<0){*p++='-';n=-n;}
    p=g_decimal(p,(int)(n/10000));unsigned fraction=(unsigned)n%10000;
    if(fraction){*p++='.';for(unsigned divisor=1000;divisor;divisor/=10)*p++=(char)('0'+fraction/divisor%10);while(p[-1]=='0')--p;*p=0;}
}
static void press(char key) {
    if(key=='C'){g_copy(display,"0",64);stored=0;operation=0;fresh=1;return;}
    if(key=='~'){show(-number());return;}
    if((key>='0'&&key<='9')||key=='.'){
        if(fresh){display[0]=0;fresh=0;}unsigned n=(unsigned)g_strlen(display);
        unsigned integers=0,decimals=0;int dot=0;
        for(unsigned i=0;i<n;++i){if(display[i]=='.')dot=1;else if(display[i]!='-'){if(dot)++decimals;else ++integers;}}
        if(key=='.'&&dot)return;
        if(key!='.'&&((dot&&decimals>=4)||(!dot&&integers>=5)))return;
        if(n<12){display[n]=key;display[n+1]=0;}return;
    }
    int64_t value=number();
    if(operation){
        if(operation=='+')value=stored+value;else if(operation=='-')value=stored-value;
        else if(operation=='*')value=stored*value/10000;else if(operation=='/'){if(!value){g_copy(display,"Divide by zero",64);operation=0;fresh=1;return;}value=stored*10000/value;}
        show(value);g_puts("CALC: result ");g_puts(display);g_puts("\n");
        if(value>999999999 || value< -999999999){stored=0;operation=0;fresh=1;return;}
    }
    stored=value;operation=key=='='?0:key;fresh=1;
}
int app_main(unsigned argument) {
    (void)argument;if(ui_open(&u,"Calculator",320,370)<0)return 1;int first=1;
    const char *keys[]={"7","8","9","/","4","5","6","*","1","2","3","-","0",".","=","+"};
    for(;;){if(!ui_poll(&u)&&!first){g_sleep(2);continue;}first=0;
        if(u.e.type==G_KEY && u.e.pressed){unsigned c=u.e.ascii;if((c>='0'&&c<='9')||c=='.'||c=='+'||c=='-'||c=='*'||c=='/'||c=='=')press((char)c);if(c==10)press('=');if(c=='c'||c=='C')press('C');}
        ui_begin(&u);ui_rect(&u,16,18,u.width-32,60,0xffffff);ui_label(&u,28,40,display,UI_INK);
        if(ui_button(&u,16,94,130,"Clear"))press('C');if(ui_button(&u,158,94,144,"+ / -"))press('~');
        for(int i=0;i<16;++i)if(ui_button(&u,16+(i%4)*74,138+(i/4)*46,66,keys[i]))press(keys[i][0]);
        ui_label(&u,16,334,"4 decimals / max 99999.9999",0x61768a);ui_end(&u);
    }
}
