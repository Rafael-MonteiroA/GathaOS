#ifndef GATHA_INTERRUPTS_H
#define GATHA_INTERRUPTS_H
#include <stddef.h>
#include <stdint.h>
/* Ring 3 appends user ESP and SS after this common prefix. */
struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, saved_esp, ebx, edx, ecx, eax;
    uint32_t vector, error, eip, cs, eflags;
};
_Static_assert(sizeof(struct interrupt_frame) == 68, "ISR frame size");
_Static_assert(offsetof(struct interrupt_frame, vector) == 48, "ISR vector offset");
_Static_assert(offsetof(struct interrupt_frame, eflags) == 64, "ISR flags offset");
void gdt_init(void);
void idt_init(void);
struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame);
void tss_stack(uint32_t top);
int interrupt_probe(void);
_Noreturn void debug_trigger_divide(void);
_Noreturn void debug_trigger_gp(void);
_Noreturn void debug_trigger_page(void);
_Noreturn void debug_trigger_readonly(void);
#endif
