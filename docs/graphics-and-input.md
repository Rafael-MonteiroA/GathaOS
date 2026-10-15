# Graphics and input contract — Stage 3

## Framebuffer and drawing

The BIOS selects 1024x768x32 (800x600 fallback). Paging maps the physical VBE
front buffer at 0xe8000000. After memory exhaustion tests, the driver allocates
one tightly packed RGB back buffer from the heap: 3,145,728 bytes at 1024x768,
or 1,920,000 bytes at 800x600. These front/back buffers provide software double
buffering. Presentation copies completed regions using firmware pitch and channel
positions. It does not page-flip or synchronize to vertical retrace; tearing is
possible.

`gui/gfx/` supplies clipped pixels, Bresenham lines, filled rectangles and rounded
rectangles, vertical gradients, alpha-filled rectangles and bitmap text. Colors
are 0xRRGGBB; alpha is 0..255 with rounded integer channel interpolation. Signed
coordinates/dimensions are limited to +/-32767 by the internal API contract.
Text supports 95 printable ASCII characters, newline and scales 1..4. See
`graphics-font-license.txt` for the original font/cursor permission grant.

The demo presents changed text/status areas and old/new cursor rectangles at
at most 50 Hz. The 16x24 cursor saves/restores pixels in the back buffer, including
when overlapping text or screen edges. IRQs never render or allocate. This is a
kernel driver demo without a window manager, widgets or application processes.

## PS/2 and events

Initialization uses bounded polling, port tests and ACK checking, retrying a
command up to three times on RESEND. Controller translation is disabled. The
keyboard is explicitly configured and queried for native scancode set 2, or
set 1 with `PS2_SET=1`. The mouse must identify as a standard three-byte device
and runs in 1:1 stream mode at 100 Hz. Missing/unsupported required devices panic.

IRQ1 and IRQ12, plus cascade IRQ2, are enabled after setup. An ISR drains at most
32 bytes into a 512-entry single-CPU ring, including source and PIT timestamp.
Main-loop reads briefly disable IRQs. Queue overflow and parity/timeout errors
are counted; on loss the queue and decoder state reset, the cursor recenters,
and serial reports recovery. This prevents logically stuck modifiers.

Keyboard decoding handles make/break, E0 and consumes Pause/Print Screen
sequences without retaining fake modifiers. Left/right Shift, Control and Alt
are independent; Caps Lock is software state. Printable keys use US ASCII.
Shift XOR Caps changes letter case; Shift changes punctuation. Repeated hardware
make bytes are suppressed. The latest held printable/editing key repeats after
500 ms, every 40 ms, using PIT ticks and current modifiers. Release stops repeat;
an older held key does not resume automatically.

Mouse decoding checks the first-byte sync bit, times out incomplete packets
after 100 ms, handles signed nine-bit deltas and clamps screen coordinates.
Overflow packets update buttons but discard motion. Masks: left=1, right=2,
middle=4. Arbitrary byte loss cannot be perfectly resynchronized by this protocol;
packet timeout and queue-loss reset provide bounded recovery.

The demo offers append-only typing, Enter, Backspace, four-column Tab stops,
wrapping, scrolling and Ctrl+L. Control/Alt chords suppress text. Caps Lock is
shown on screen; physical LEDs are not updated. No arrow-key editing, ABNT2,
Unicode, persistent text, USB HID, mouse wheel or extra buttons are implemented.

Command references were checked against official QEMU device implementations
([PS/2](https://github.com/qemu/qemu/blob/master/hw/input/ps2.c),
[i8042](https://github.com/qemu/qemu/blob/master/hw/input/pckbd.c)). No QEMU or
other OS source code/data was copied into the guest.

## Validation

Boot self-tests cover clipping, alpha endpoints/interpolation, a diagonal,
mouse sign extension (including -256), synchronization, overflow, timeout,
keyboard repeat delay, duplicate makes and release. The host injects events
through QMP into emulated PS/2 devices and compares actual display captures
against text pixels, primitive samples, button colors and a complete restored
screen baseline. See `stage-3-report.md` for release evidence and limits.
