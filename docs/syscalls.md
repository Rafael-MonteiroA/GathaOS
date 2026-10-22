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
| 7 | open | path, flags | descriptor 0..7 |
| 8 | read | fd, writable buffer, bytes | bytes read |
| 9 | write | fd, readable buffer, bytes | bytes written |
| 10 | close | fd | 0 |
| 11 | seek | fd, signed offset, whence 0=set/1=current/2=end | new position |
| 12 | readdir | path, index, writable g_dirent pointer | 1 entry, 0 end |
| 13 | mkdir | path | 0 |
| 14 | unlink | path | 0 |
| 15 | stat | path, writable g_stat pointer | 0 |
| 16 | rename | source path, destination path | 0 |
| 17 | sync | — | 0 after ATA cache flush |
| 29 | sbrk | signed byte increment | previous break, or -12 |
| 30 | save | path, readable buffer, bytes | 0 after staged replacement/flush |
| 31 | network | operation, user buffer, bytes, handle | nonblocking result; see network.md |

Open flags: READ=1, WRITE=2, CREATE=4, TRUNC=8 (TRUNC requires WRITE). Paths are
absolute, NUL-terminated and limited to 255 bytes. Seek cannot create holes.
`g_stat` is four uint32 words: type (file=1/directory=2), size, created, modified.
`g_dirent` is a 40-byte name followed by g_stat. Typical errors are -2 missing,
-5 I/O, -9 bad descriptor, -12 memory capacity, -13 access mode, -14 bad user pointer, -16 busy,
-17 exists, -20 not a directory, -21 is a directory, -22 invalid, -24 descriptor
limit, -27 file too large, -28 capacity, -30 read-only, -36 name/path too long, -39 non-empty and -74 bad volume.

Unknown calls return -38. Invalid lengths/delays return -22 where applicable.
GUI calls 18 through 24 and their shared structures are specified in `gui.md`.
Calls 25 through 28 (power, processes, argument and bitmap) are specified in
`applications.md` and `gui.md`.
Calls 29/30 and their failure guarantees are specified in foundation.md.
User ranges must lie wholly in 0x00400000..0x0bffffff, with user-present PDEs/PTEs
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
