#include "graphics_demo.h"
#include "drivers/framebuffer.h"
#include "drivers/input.h"
#include "drivers/serial.h"
#include "arch/x86/pit.h"
#include "arch/x86/io.h"
#include "../gui/gfx/gfx.h"
#include "../libgatha/memory.h"
extern int tasks_demo_dirty(void);
extern void tasks_demo_paint(void);
extern void tasks_demo_report(void);
static int width, height, columns, rows, column, row, mouse_x, mouse_y;
static unsigned mods, buttons;
static char cells[20][129];
static uint32_t cursor_under[16 * 24];
static int cursor_x, cursor_y;
static const char *const cursor[24] = {
    "X", "XX", "XOX", "XOOX", "XOOOX", "XOOOOX", "XOOOOOX", "XOOOOOOX",
    "XOOOOOOOX", "XOOOOOOOOX", "XOOOOOOOOOX", "XOOOOOOOOOOX",
    "XOOOOOOOOOOOX", "XOOOOOOOOXXXXX", "XOOOOXOOX", "XOOOX XOOX",
    "XOOX  XOOX", "XOX    XOOX", "XX     XOOX", "X       XX", "", "", "", ""
};
static void cursor_hide(void) {
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 16; ++x)
            gfx_pixel(cursor_x + x, cursor_y + y, cursor_under[y * 16 + x]);
}
static void cursor_show(void) {
    cursor_x = mouse_x; cursor_y = mouse_y;
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 16; ++x) {
            int px = cursor_x + x, py = cursor_y + y;
            cursor_under[y * 16 + x] = (px < width && py < height) ?
                framebuffer_buffer()[py * width + px] : 0;
        }
        for (int x = 0; cursor[y][x]; ++x)
            if (cursor[y][x] != ' ') gfx_pixel(cursor_x + x, cursor_y + y,
                                               cursor[y][x] == 'X' ? 0x07111e : 0xf8fafc);
    }
}
static void clear_text(void) {
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) cells[y][x] = ' ';
        cells[y][columns] = 0;
    }
    column = row = 0;
}
static void advance_row(void) {
    column = 0;
    if (++row < rows) return;
    for (int y = 1; y < rows; ++y) memcpy(cells[y - 1], cells[y], sizeof(cells[0]));
    --row;
    for (int x = 0; x < columns; ++x) cells[row][x] = ' ';
}
static void type_character(unsigned char c) {
    if (c == '\b') {
        if (column) --column;
        else if (row) { --row; column = columns - 1; }
        cells[row][column] = ' ';
    } else if (c == '\n') advance_row();
    else if (c == '\t') {
        unsigned spaces = 4 - (unsigned)column % 4;
        while (spaces--) type_character(' ');
    } else if (c >= 32 && c <= 126) {
        cells[row][column++] = (char)c;
        if (column == columns) advance_row();
    }
}
static void draw_text_area(void) {
    gfx_rect(48, 360, width - 96, height - 448, 0x101e30);
    for (int y = 0; y < rows; ++y) gfx_text(48, 360 + y * 20, cells[y], 0xe2e8f0, 1);
    gfx_rect(48 + column * 8, 360 + row * 20, 2, 16, 0x5eead4);
}
static void draw_status(void) {
    gfx_rect(32, height - 64, width - 64, 24, 0x0b1424);
    gfx_text(32, height - 60, "SHIFT   CTRL   ALT   CAPS", 0x60728e, 1);
    if (mods & MOD_SHIFT) gfx_text(32, height - 60, "SHIFT", 0x5eead4, 1);
    if (mods & MOD_CTRL) gfx_text(96, height - 60, "CTRL", 0x5eead4, 1);
    if (mods & MOD_ALT) gfx_text(152, height - 60, "ALT", 0x5eead4, 1);
    if (mods & MOD_CAPS) gfx_text(200, height - 60, "CAPS", 0x5eead4, 1);
    gfx_text(width - 280, height - 60, "MOUSE", 0x94a3b8, 1);
    const unsigned masks[3] = {1, 4, 2};
    const char *names[3] = {"L", "M", "R"};
    for (int i = 0; i < 3; ++i) {
        gfx_round_rect(width - 220 + i * 48, height - 64, 36, 24, 5,
                       buttons & masks[i] ? 0x168b80 : 0x213249);
        gfx_text(width - 206 + i * 48, height - 60, names[i], 0xf8fafc, 1);
    }
}
void graphics_demo_init(const struct boot_info *boot) {
    framebuffer_init(boot);
    gfx_self_test(); input_self_test();
    width = (int)boot->width; height = (int)boot->height;
    columns = (width - 96) / 8; rows = (height - 448) / 20;
    mouse_x = width / 2; mouse_y = height / 2;
    clear_text();
    gfx_rect(0, 0, width, height, 0x0b1424);
    gfx_gradient(0, 0, width, 136, 0x213c59, 0x102137);
    gfx_rect(32, 32, 4, 48, 0x5eead4);
    gfx_text(52, 32, "GathaOS", 0xf1f5f9, 3);
    gfx_text(34, 100, "An original operating system. Built from the first instruction.", 0xa8bed4, 1);
    gfx_round_rect(width - 230, 36, 198, 32, 8, 0x164a4b);
    gfx_text(width - 214, 44, "STAGE 04 / TASK LAB", 0x8ff5d5, 1);
    int card = (width - 96) / 3;
    for (int i = 0; i < 3; ++i) gfx_round_rect(32 + i * (card + 16), 160, card, 120, 12, 0x18283d);
    gfx_text(48, 176, "01  SHAPES + LINES", 0x94a3b8, 1);
    gfx_round_rect(52, 211, 64, 44, 10, 0x5eead4);
    gfx_rect(136, 211, 44, 44, 0x60a5fa);
    gfx_line(198, 250, 238, 211, 0xfbbf24);
    gfx_line(198, 211, 238, 250, 0xfbbf24);
    int second = 48 + card + 16;
    gfx_text(second, 176, "02  GRADIENT", 0x94a3b8, 1);
    gfx_gradient(second, 211, card - 32, 44, 0x38bdf8, 0xa78bfa);
    int third = second + card + 16;
    gfx_text(third, 176, "03  ALPHA BLEND", 0x94a3b8, 1);
    gfx_rect(third, 211, card - 32, 44, 0x3b82f6);
    gfx_alpha_rect(third + 40, 211, card - 72, 44, 0xf472b6, 128);
    gfx_text(32, 310, "Type something.", 0xf1f5f9, 1);
    gfx_text(width - 368, 310, "US keyboard / Enter / Backspace / Tab", 0x94a3b8, 1);
    gfx_round_rect(32, 336, width - 64, height - 416, 12, 0x304860);
    gfx_round_rect(33, 337, width - 66, height - 418, 11, 0x101e30);
    draw_text_area(); draw_status();
    gfx_text(32, height - 28, "Ctrl+L clears text. Hold a key to repeat. Move the mouse and try its buttons.", 0x94a3b8, 1);
    cursor_show();
    framebuffer_present(0, 0, width, height);
    klog("GFX: 8x16 ASCII font primitives cursor ready\r\n");
}
static int handle_event(const struct input_event *e) {
    if (e->type == INPUT_MOUSE) {
        mouse_x = e->x; mouse_y = e->y; buttons = e->buttons;
        klog("INPUT: mouse x="); klog_u32((unsigned)e->x);
        klog(" y="); klog_u32((unsigned)e->y);
        klog(" buttons="); klog_u32(buttons); klog("\r\n");
        return 1;
    }
    mods = e->modifiers;
    klog("INPUT: key="); klog_u32(e->code);
    klog(" down="); klog_u32(e->pressed);
    klog(" ascii="); klog_u32(e->ascii);
    klog(" mods="); klog_u32(mods);
    klog(" repeat="); klog_u32(e->repeat); klog("\r\n");
    if (!e->pressed) return 1;
    if ((mods & MOD_CTRL) && (e->ascii == 'l' || e->ascii == 'L')) clear_text();
    else if (!(mods & (MOD_CTRL | MOD_ALT))) type_character(e->ascii);
    return 3;
}
void graphics_demo_run(void) {
    klog("STAGE3: READY\r\n");
    uint32_t next_report = 100, next_frame = 0;
    unsigned dirty = 0;
    for (;;) {
        uint32_t now = pit_ticks();
        struct input_event e;
        while (input_poll(&e, now)) dirty |= (unsigned)handle_event(&e);
        if (tasks_demo_dirty()) dirty |= 4;
        if (dirty && now - next_frame < UINT32_C(0x80000000)) {
            int old_x = cursor_x, old_y = cursor_y;
            uint32_t flags = irq_save();
            cursor_hide();
            if (dirty & 4) tasks_demo_paint();
            if (dirty & 2) draw_text_area();
            draw_status(); cursor_show();
            if (dirty & 2) framebuffer_present(48, 360, width - 96, height - 448);
            framebuffer_present(32, height - 64, width - 64, 24);
            framebuffer_present(old_x, old_y, 16, 24);
            framebuffer_present(cursor_x, cursor_y, 16, 24);
            if (dirty & 4) framebuffer_present(32,160,width-64,120);
            irq_restore(flags);
            dirty = 0; next_frame = now + 2;
        }
        if (now - next_report < UINT32_C(0x80000000)) {
            klog("PIT: ticks="); klog_u32(now); klog("\r\n");
            if (next_report == 300) klog("STAGE3: PASS\r\n");
            next_report += 100;
            tasks_demo_report();
        }
        /* IRQs only enqueue bytes; the main loop owns the back buffer and decoders. */
        __asm__ volatile("sti; hlt" : : : "memory");
    }
}
