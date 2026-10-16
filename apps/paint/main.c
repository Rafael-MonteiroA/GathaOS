#include "../common.h"
static struct ui u;
static struct {uint32_t magic,width,height,colors,palette[8];uint8_t pixels[256*160];} picture, pending;
static char path[256]="/Documents/Drawing.gbm",status[80]="Pencil / drag to draw";
static struct ui_text filename={.data=path,.capacity=256};
static int color=1,tool,anchor_x,anchor_y,previous_x=-1,previous_y=-1;
static void dot(int x,int y,int c){if(x>=0&&y>=0&&x<256&&y<160)picture.pixels[y*256+x]=(uint8_t)c;}
static void line(int x,int y,int x2,int y2,int c){int dx=x2>x?x2-x:x-x2,dy=y2>y?y-y2:y2-y,sx=x<x2?1:-1,sy=y<y2?1:-1,err=dx+dy;for(;;){dot(x,y,c);if(x==x2&&y==y2)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}}
static void load(void){
    int n=app_load(path,&pending,sizeof(pending));
    if(n!=(int)sizeof(pending)||pending.magic!=0x324d4247||pending.width!=256||pending.height!=160||pending.colors!=8){g_copy(status,"Invalid GBM2 image",80);return;}
    for(unsigned i=0;i<sizeof(pending.pixels);++i)if(pending.pixels[i]>7){g_copy(status,"Invalid GBM2 pixels",80);return;}
    memcpy(&picture,&pending,sizeof(picture));g_copy(status,"Opened drawing",80);g_puts("PAINT: opened\n");
}
int app_main(unsigned argument){
    (void)argument;picture.magic=0x324d4247;picture.width=256;picture.height=160;picture.colors=8;
    const unsigned colors[]={0xffffff,0x20344d,0xe35b67,0xf4b84b,0x45a984,0x449bd6,0x926cc7,0x187e83};memcpy(picture.palette,colors,sizeof(colors));
    char initial[256];app_argument(initial);if(*initial){g_copy(path,initial,256);load();}
    if(ui_open(&u,"Paint",560,500)<0)return 1;int first=1;
    for(;;){if(!ui_poll(&u)&&!first){g_sleep(2);continue;}first=0;
        if(u.e.type==G_MOUSE){int x=(u.e.x-12)/2,y=(u.e.y-84)/2;int inside=u.e.x>=12 && u.e.y>=84 && x<256&&y<160;
            if(inside && u.e.pressed==1){anchor_x=x;anchor_y=y;previous_x=x;previous_y=y;}
            if(inside && (u.e.buttons&1) && (tool==0||tool==1)){if(previous_x>=0)line(previous_x,previous_y,x,y,tool==1?0:color);previous_x=x;previous_y=y;g_puts("PAINT: stroke\n");}
            if(u.e.pressed==256){if(inside&&previous_x>=0){if(tool==2)line(anchor_x,anchor_y,x,y,color);if(tool==3){int x0=x<anchor_x?x:anchor_x,x1=x>anchor_x?x:anchor_x,y0=y<anchor_y?y:anchor_y,y1=y>anchor_y?y:anchor_y;for(int py=y0;py<=y1;++py)for(int px=x0;px<=x1;++px)dot(px,py,color);}}previous_x=-1;}
        }
        ui_begin(&u);const char *tools[]={"Pencil","Eraser","Line","Rect"};for(int i=0;i<4;++i)if(ui_button(&u,12+i*100,10,92,tools[i])){tool=i;previous_x=-1;}
        if(ui_button(&u,420,10,120,"Clear"))memset(picture.pixels,0,sizeof(picture.pixels));
        for(int i=0;i<8;++i){int x=12+i*42;ui_rect(&u,x,48,34,26,i==color?UI_INK:0xc8d6e3);ui_rect(&u,x+3,51,28,20,picture.palette[i]);if(ui_hit(&u,x,48,34,26))color=i;}
        struct g_bitmap bitmap={.x=12,.y=84,.width=256,.height=160,.scale=2,.pixels=picture.pixels};memcpy(bitmap.palette,picture.palette,32);ui_bitmap(&u,&bitmap);
        ui_textbox(&u,12,416,360,34,&filename);if(ui_button(&u,386,420,72,"Open"))load();
        if(ui_button(&u,468,420,72,"Save")){picture.magic=0x324d4247;picture.width=256;picture.height=160;picture.colors=8;int r=app_save(path,&picture,sizeof(picture));app_status(status,"Save",r);if(!r)g_puts("PAINT: saved\n");}
        ui_label(&u,12,472,status,UI_INK);ui_end(&u);
    }
}
