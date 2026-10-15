#ifndef GATHA_INTERRUPTS_H
#define GATHA_INTERRUPTS_H
#include <stddef.h>
#include <stdint.h>
/* Ring-0 frame only: no privilege transition, user ESP or user SS in Stage 1. */
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
void interrupt_dispatch(struct interrupt_frame *frame);
int interrupt_probe(void);
_Noreturn void debug_trigger_divide(void);
_Noreturn void debug_trigger_gp(void);
_Noreturn void debug_trigger_page(void);
_Noreturn void debug_trigger_readonly(void);
#endif
