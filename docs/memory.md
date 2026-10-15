# Stage 2 memory management

## Bootstrap and paging

The bootloader still copies a raw image to physical 1 MiB and jumps there. Its
entry assembly builds a temporary directory and one 4 KiB page table. The first
4 MiB are mapped both identity and at 0xc0000000. CR0.PG and CR0.WP are enabled,
an absolute jump reaches the linked high text, and ESP moves to 0xc0090000.
The high BSS is cleared before C. EBX is converted to the high alias of boot info.

The linker gives the bootstrap a low VMA/LMA and the main kernel a VMA offset
of 0xc0000000 from its physical LMA. `llvm-objcopy` packs by load addresses;
there is no 3 GiB hole in the raw image. Link-time checks bound BSS below 4 MiB
and the packer still bounds the loaded payload at 64 KiB.

The kernel copies boot info into owned static storage and installs high GDT/IDT
addresses. It builds final supervisor-only 4 KiB page tables, then switches CR3.
No low PDE survives. Kernel text and rodata are read-only, writable RAM is mapped
through the direct window, and a distinct uncached window maps the framebuffer.
The copied framebuffer pointer becomes virtual; original E820 records remain
accessible via their reserved high alias. Panic graphics are enabled only after
the framebuffer mapping is ready; earlier errors are serial-only.

Heap map/unmap helpers validate aligned ranges, reject existing mappings and use
INVLPG on updates. A test creates two aliases of an allocated frame, checks writes
in both directions, unmaps one and verifies its PTE is absent. Actual debug faults
verify that a null write yields #PF error 2 and a rodata write yields #PF error 3.
Tests read QMP CPU state to confirm high EIP/ESP and hardware CR0.PG/WP, rather
than trusting only the kernel's log messages.

## Physical page allocator

`pmm_alloc()` returns a page-aligned physical address, or zero on exhaustion.
`pmm_free()` returns false for unaligned, out-of-range, reserved, or already-free
frames. Physical page zero is never allocatable. `physical_pointer()` converts a
valid physical address under 512 MiB to its high direct-map alias.

Two bitmaps track eligible frames and busy frames. Initialization begins with
everything reserved, then enables only full pages within enabled E820 type-1
regions. A second pass gives reserved or disabled entries priority on overlaps.
Usable ranges round inward; reservations round outward. Ranges are clipped to
512 MiB with saturating 64-bit end arithmetic. Kernel/boot/stack/static metadata
and framebuffer reservations are applied last. The serial report prints raw E820
entries plus the final usable/free frame count and reservation end.

A next-search hint scans for free frames. Allocation/free mask and restore IRQs;
this is a single-CPU kernel and does not claim SMP synchronization. Firmware
memory above 512 MiB is reported but not managed. E820 attribute/type handling
is conservative; ACPI reclaimable memory remains reserved.

Synthetic range tests exercise unaligned usable spans, overlapping reservations,
64-bit overflow, the upper limit and addresses above 4 GiB before resetting the
bitmaps and applying the real firmware map. No physical allocations occur while
the synthetic bitmap is installed.

## Kernel heap

`kmalloc(size)` returns a 16-byte-aligned virtual pointer, or NULL for zero size,
overflow, virtual-capacity exhaustion or physical OOM. `kfree(NULL)` does nothing.
Other frees require the exact address of a currently allocated block; invalid
or double frees panic. Contents of allocated bytes are unspecified until written.

The heap uses an address-ordered doubly linked block list, 32-byte headers,
first-fit selection, splitting and adjacent-free coalescing. Its fixed virtual
window is 16 MiB, with 64 KiB initially backed. Growth maps pages in batches of
at least 64 KiB, capped by the remaining window. On physical OOM, every newly
mapped page is unmapped and freed before NULL is returned; existing metadata
is unchanged. `kfree` trims wholly unused trailing pages while retaining 64 KiB.

`heap_validate()` checks coverage, size/alignment, header magic, neighbor links
and the absence of adjacent uncoalesced free blocks. Metadata is not isolated
from callers: buffer overruns can still corrupt a kernel heap. There are no
slabs, realloc, per-allocation guard pages, process heaps or SMP locks yet.

## Boot stress suite

The normal and every debug boot run these checks before enabling the timer:

1. Test page aliases, null/low mapping removal, text/rodata flags and heap guard.
2. Allocate every free physical frame, recording linked-list metadata and data
   sentinels in each frame. Check exhaustion, free all frames, and restore the
   exact initial free count. Reject reserved, misaligned and double frees.
3. While physical RAM is exhausted, request heap growth. Then release just one
   frame and repeat, forcing partial-map rollback. Counts and mappings must stay
   unchanged after each failed request.
4. Check zero/overflow size behavior and explicit split/coalesce reuse. Run 4,096
   deterministic mixed operations across 128 slots, with varied sizes, byte
   patterns, alignment checks and periodic metadata/content verification.
5. Fill the heap with 255 allocations of 64 KiB, check rejection at its virtual
   limit, then free in alternating order. Verify content, coalescing, trimming
   back to 64 KiB and restoration of the physical free count.

`make test` runs normal kernels with 64, 256 and 512 MiB, plus page, readonly,
divide and general-protection faults at 256 MiB. Each boot also checks the
retained IRQ/timer functionality and displayed graphics. Stage 3 also boots a
native set-1 keyboard variant at 256 MiB and tests actual input; panic comparisons
still check every displayed pixel. For the QEMU PC
configuration, the host independently checks the usable-frame count against RAM,
the kernel reservation end and the firmware's top 128 KiB reservation.

The tests establish these exercised cases; they do not prove arbitrary runtime
workloads leak-free. The all-RAM exhaustion test is intended for this stage's
boot environment, before any other subsystem owns dynamic allocations.
