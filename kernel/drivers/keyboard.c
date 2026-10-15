#include "input.h"
static uint8_t held[256], extended, releasing, pause_left, caps;
static unsigned repeat_code;
static uint32_t repeat_at;
static const char plain[128] = {
    [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
    [12]='-',[13]='=',[14]='\b',[15]='\t',[16]='q',[17]='w',[18]='e',[19]='r',[20]='t',
    [21]='y',[22]='u',[23]='i',[24]='o',[25]='p',[26]='[',[27]=']',[28]='\n',
    [30]='a',[31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',
    [39]=';',[40]='\'',[41]='`',[43]='\\',[44]='z',[45]='x',[46]='c',[47]='v',[48]='b',
    [49]='n',[50]='m',[51]=',',[52]='.',[53]='/',[55]='*',[57]=' ',[74]='-',[78]='+'
};
static const char shifted[128] = {
    [2]='!',[3]='@',[4]='#',[5]='$',[6]='%',[7]='^',[8]='&',[9]='*',[10]='(',[11]=')',
    [12]='_',[13]='+',[26]='{',[27]='}',[39]=':',[40]='"',[41]='~',[43]='|',
    [51]='<',[52]='>',[53]='?'
};
#if PS2_SET == 2
/* Hardware set-2 make codes mapped to our set-1-based key identifiers. */
static const uint8_t set2[128] = {
    [0x76]=1,[0x16]=2,[0x1e]=3,[0x26]=4,[0x25]=5,[0x2e]=6,[0x36]=7,[0x3d]=8,
    [0x3e]=9,[0x46]=10,[0x45]=11,[0x4e]=12,[0x55]=13,[0x66]=14,[0x0d]=15,
    [0x15]=16,[0x1d]=17,[0x24]=18,[0x2d]=19,[0x2c]=20,[0x35]=21,[0x3c]=22,
    [0x43]=23,[0x44]=24,[0x4d]=25,[0x54]=26,[0x5b]=27,[0x5a]=28,[0x14]=29,
    [0x1c]=30,[0x1b]=31,[0x23]=32,[0x2b]=33,[0x34]=34,[0x33]=35,[0x3b]=36,
    [0x42]=37,[0x4b]=38,[0x4c]=39,[0x52]=40,[0x0e]=41,[0x12]=42,[0x5d]=43,
    [0x1a]=44,[0x22]=45,[0x21]=46,[0x2a]=47,[0x32]=48,[0x31]=49,[0x3a]=50,
    [0x41]=51,[0x49]=52,[0x4a]=53,[0x59]=54,[0x7c]=55,[0x11]=56,[0x29]=57,
    [0x58]=58,[0x77]=69,[0x7e]=70,[0x6c]=71,[0x75]=72,[0x7d]=73,[0x7b]=74,
    [0x6b]=75,[0x73]=76,[0x74]=77,[0x79]=78,[0x69]=79,[0x72]=80,[0x7a]=81,
    [0x70]=82,[0x71]=83
};
#endif
void keyboard_reset(void) {
    for (unsigned i = 0; i < 256; ++i) held[i] = 0;
    extended = releasing = pause_left = caps = 0;
    repeat_code = 0;
}
static unsigned modifiers(void) {
    return ((held[42] || held[54]) ? MOD_SHIFT : 0) |
        ((held[29] || held[157]) ? MOD_CTRL : 0) |
        ((held[56] || held[184]) ? MOD_ALT : 0) | (caps ? MOD_CAPS : 0);
}
static void key_event(unsigned code, int pressed, int repeat, struct input_event *e) {
    *e = (struct input_event){.type=INPUT_KEY, .code=code, .pressed=(unsigned)pressed,
                             .repeat=(unsigned)repeat, .modifiers=modifiers()};
    if (!pressed) return;
    unsigned char c = code < 128 ? (unsigned char)plain[code] : (code == 156 ? '\n' : 0);
    if (c >= 'a' && c <= 'z') {
        if (!!(e->modifiers & MOD_SHIFT) != !!caps) c -= 'a' - 'A';
    } else if ((e->modifiers & MOD_SHIFT) && code < 128 && shifted[code]) {
        c = (unsigned char)shifted[code];
    }
    e->ascii = c;
}
int keyboard_byte(uint8_t byte, uint32_t now, struct input_event *event) {
    if (pause_left) { --pause_left; return 0; }
    if (byte == 0xe1) { pause_left = PS2_SET == 2 ? 7 : 5; extended = releasing = 0; return 0; }
    if (byte == 0xe0) { extended = 1; return 0; }
#if PS2_SET == 2
    if (byte == 0xf0) { releasing = 1; return 0; }
    unsigned code = byte < 128 ? set2[byte] : 0;
    int pressed = !releasing;
#else
    unsigned code = byte & 127;
    int pressed = !(byte & 128);
#endif
    if (extended && code) code += 128;
    extended = releasing = 0;
    /* Extended fake Shift bytes (Print Screen) must never change real Shift. */
    if (!code || code == 170 || code == 182 || code == 183) return 0;
    if (held[code] == pressed) return 0;
    held[code] = (uint8_t)pressed;
    if (code == 58 && pressed) caps ^= 1;
    key_event(code, pressed, 0, event);
    if (pressed && event->ascii) { repeat_code = code; repeat_at = now + 50; }
    if (!pressed && repeat_code == code) repeat_code = 0;
    return 1;
}
int keyboard_repeat(uint32_t now, struct input_event *event) {
    if (!repeat_code || now - repeat_at >= UINT32_C(0x80000000)) return 0;
    repeat_at = now + 4;
    key_event(repeat_code, 1, 1, event);
    return 1;
}
