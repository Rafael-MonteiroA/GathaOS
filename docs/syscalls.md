# Gatha syscall ABI v1

32-bit x86 `int 0x80`, DPL 3 interrupt gate. EAX selects the call; arguments are
EBX, ECX, EDX, ESI, EDI. Signed EAX is the result (negative values are errors).
Other GPRs and user flags are restored. Calls execute with interrupts disabled;
the kernel is preempted between syscalls, not in their critical sections.

| Number | Call | Arguments | Result |
|---:|---|---|---|
| 0 | ticks | — | PIT count at 100 Hz |
| 1 | yield | — | 0 after rescheduling |
| 2 | sleep | ticks, at most 0x7fffffff | 0 after wakeup |
| 3 | exit | signed exit code | never returns |
| 4 | log | user pointer, length <=1024 | bytes, or -14 |
| 5 | pid | — | current process identifier |
| 6 | demo | animation frame | 0, or -1 for a task without a demo lane |

Unknown calls return -38. Invalid lengths/delays return -22 where applicable.
User ranges must lie wholly in 0x00400000..0x007fffff, with user-present PTEs
and writable PTEs for output. Zero-length transfers do not dereference pointers.
High kernel mappings are supervisor-only in every address space.

The saved interrupt prefix is 68 bytes. Ring-3 transitions append ESP and SS
(8 bytes). CS=0x1b, data/stack=0x23, IOPL=0. A TSS supplies each task's kernel
stack; its absent I/O bitmap denies user port operations. User exceptions end
the offending task. Ring-0 exceptions still panic with diagnostics.

Scheduling is single-CPU round-robin with a two-tick quantum. A runnable idle
task always exists. Each task has a separate 16 KiB kernel stack and stack
sentinel. User tasks have separate page directories and private low pages;
supervisor kernel mappings are shared. Dead-task resources are reclaimed from
the idle task, after another stack and page directory are active. There is no
SMP, priorities, FPU context or signals. User programs must not use FPU/SIMD.

Reference: Intel IA-32 SDM, system programming volumes, protection/interrupts/TSS
([official manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)).
