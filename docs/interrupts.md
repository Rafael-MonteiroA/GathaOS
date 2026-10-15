# Stage 1 interrupt and panic contract

The kernel replaces the bootloader GDT with a kernel-owned null descriptor,
flat ring-0 code descriptor (0x08) and flat ring-0 data descriptor (0x10).
All segment registers are reloaded. There is no ring 3 or TSS yet.

The IDT has 256 present 32-bit interrupt gates, selector 0x08, DPL 0. The CPU
clears IF on entry. NASM stubs normalize the stack: exceptions 8, 10-14, 17,
21, 29 and 30 already have a CPU-pushed error code; all other vectors receive
a synthetic zero. The stubs then push the vector number, general registers
and data segment registers. Modern reserved exception numbers are accounted
for, but only divide error and general protection have been deliberately tested.

The C `interrupt_frame` layout, starting at offset zero:

| Offset | Saved value |
|---|---|
| 0, 4, 8, 12 | GS, FS, ES, DS |
| 16, 20, 24, 28 | EDI, ESI, EBP, PUSHAD's saved ESP |
| 32, 36, 40, 44 | EBX, EDX, ECX, EAX |
| 48, 52 | Vector and error code |
| 56, 60, 64 | EIP, CS and EFLAGS |

This is a ring-0-only frame. There is no privilege transition stack tail.
Interrupted ESP is PUSHAD's saved ESP plus 20 bytes (two normalized fields and
three CPU-pushed words). SS is the current kernel stack selector.

The common entry clears DF for C, switches data segments to 0x10 and aligns the
call stack to 16 bytes. On IRQ return it restores the original segments and
general registers, discards exactly vector/error, and executes IRETD to restore
EIP, CS and flags. Exceptions call `panic()` and never return.

## PIC and PIT

The master 8259 is remapped to 0x20, the slave to 0x28. Only IRQ0 is unmasked.
Real slave interrupts require slave EOI before master EOI. Spurious IRQ7 receives
no EOI; spurious IRQ15 receives a master-only EOI. The spurious cases are
implemented but have not been deliberately injected in tests.

PIT channel 0 uses binary rate-generator mode 2, low/high divisor writes, divisor
11932. With nominal input 1,193,182 Hz this is approximately 99.9985 interrupts
per second, the closest integer-divisor setting to 100 Hz. The handler increments
a volatile 32-bit counter and acknowledges the PIC. Logs are emitted from the
idle main loop, not from the timer handler. The tick count wraps after about
497 days; reporting uses unsigned modular comparison. Long-duration rollover
has not been boot-tested.

The idle loop tests the counter with IRQs disabled and uses `sti; hlt` together
to avoid a lost wakeup. At boot an assembly probe sets seven known GPR values,
CF and DF, then waits for an IRQ. It verifies that those values and IF survived
the interrupt. DF is cleared before returning to C. No scheduler is present.

## Logging and panic

COM1 runs at 115200 baud, 8 data bits, no parity, one stop bit. `klog` saves
and restores the caller's interrupt flags and uses bounded polling so that an
absent UART does not indefinitely prevent framebuffer diagnostics.

`panic()` disables interrupts, draws an original bitmap font into the existing
VBE framebuffer, mirrors diagnostic lines to COM1, and halts. A recursive panic
attempt emits a short serial message and halts. If boot information has not yet
been validated, only serial output is used.

The divide demo is assembly, avoiding undefined C division. It sets known
register values, CF and DF before `div ecx` with ECX zero. Tests compare the saved
EIP with `debug_divide_instruction` in the ELF and check all sentinel values.
The GP demo loads the out-of-GDT selector 0x18 into DS; the CPU must report vector
13 and error 0x18 with EIP at `debug_gp_instruction`.

All code assumes a usable kernel stack. There is no dedicated double-fault task
or emergency stack; stack corruption, faults during low-level entry, and an
unusable framebuffer can still prevent full diagnostics. Ring-3 frames and task
switching belong to Stage 4. Paging and allocator work belong to Stage 2.
