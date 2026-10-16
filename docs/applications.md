# Applications

Start lists Terminal, Files, Text Editor, Calculator, Settings, System Monitor,
Paint, About GathaOS and the earlier Widgets demo. Every launch creates a new
ring-3 process with its own page directory. Each application is separately
compiled and packed as GEX1. Built-in images are embedded in the kernel so an
older compatible data disk still works; a new Stage 7 seed also contains their
standalone executables under /System/Apps. Files can launch these disk GEXs.

## Terminal / GathaShell

The shell supports ls, cd, cat, mkdir, rm, cp, mv, echo, clear, ps, mem, uptime,
help, reboot and shutdown. Paths can be absolute or relative to the shell's
working directory. Commands accept up to three space-separated arguments;
quoting, pipes, redirection, background jobs and scripting are not implemented.
The terminal retains 80 lines and displays the latest fitting lines. Ctrl+L
clears output. cp handles files up to GathaFS's 192 KiB limit. rm refuses open
files and nonempty directories. reboot flushes the filesystem and requests an
8042 reset; shutdown flushes and halts with a safe-to-close screen. Stage 8 adds
automatic power-off where supported.

## Files

Enter an absolute folder in the location field and press Go. Select entries,
then Open; folders navigate, .gbm opens Paint, .gex launches the executable and
other files open Text Editor. Up returns to the parent. The name field is used
by New file, New folder and Rename. New file refuses an existing name. Delete
requires a dialog acknowledgement; Escape cancels. Directories must be empty.
The list scrolls using the scrollbar; there is no mouse wheel or double-click.

## Text Editor

The path field is the file to Open or Save. Text is limited to 16,383 bytes.
The document supports insertion, Backspace/Delete, arrows, Home/End, Shift
selection, Ctrl+A and vertical scrolling to the caret. Ctrl+S saves. New/Open
prompt before discarding modified content. Closing currently exits immediately;
save first. There is no undo, clipboard, search, Unicode or rich text.

## Calculator

Four-function sequential calculator with decimal input, sign toggle and clear.
Use buttons or digits/operators on the keyboard; Enter is equals. Arithmetic
uses signed fixed point with four fractional digits and truncates extra result
digits toward zero. The display range is +/-99,999.9999. Overflow and division
by zero show errors; chained operations execute left to right, without algebraic
precedence. No floating-point hardware or hosted math runtime is used.

## Settings and System Monitor

Settings changes the wallpaper and window accent for this session. It displays
the boot-selected resolution and RTC date/time in UTC. Runtime resolution and
RTC editing are not implemented. System Monitor refreshes task state, PID,
accounted CPU ticks, free physical memory, mapped kernel heap and frame count
once per second. CPU ticks are scheduler samples, not wall-clock CPU percentages.

## Paint / GBM2

The 256x160 indexed canvas is shown at 2x scale. Choose one of eight colors and
Pencil, Eraser, Line or filled Rect. Lines and rectangles finish on mouse release.
Clear erases the canvas. The path field supports Open/Save. Drawings use this
original, little-endian, uncompressed format (41,008 bytes):

| Offset | Value |
|---|---|
| 0 | u32 magic 0x324d4247 (GBM2) |
| 4 | u32 width, 256 |
| 8 | u32 height, 160 |
| 12 | u32 palette count, 8 |
| 16 | eight u32 RGB colors |
| 48 | 40,960 row-major bytes, each a palette index 0..7 |

About GathaOS is its own user process/window with project and runtime details.

## ABI additions

SYS_LAUNCH (22) accepts an optional path in ECX; index 99 loads a GEX from that
file. Registered app indices are in protocol.h. SYS_POWER (25) flushes and
requests shutdown (EBX=0) or reboot (1). SYS_PROCESSES (26) copies up to 16
g_process records to EBX, capacity ECX. SYS_ARGUMENT (27) copies the launch path
to EBX (ECX >=256). SYS_BITMAP (28) draws a validated indexed g_bitmap into an
owned window. All user ranges and dimension products are bounded.

Draw batches now use ESI flags: 1 begins a frame, 2 ends it. The compositor
waits for completed batches, up to 50 ticks before recovering from an abandoned
frame. The user toolkit flushes command chunks without presenting incomplete
frames; the bitmap operation participates in the same transaction.
