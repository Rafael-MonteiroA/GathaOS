# Desktop, compositor and toolkit

The kernel owns up to eight windows. A window handle belongs to its creating
process and is never a pointer. Each window has a heap-allocated RGB client
buffer, title, rectangle, stacking position and 64-event queue. Exit cleanup
reclaims windows, file descriptors, stacks and user address spaces. Queued
pointer motion is coalesced; overflow is counted and exposed in SYS_INFO.

The compositor tracks the union of damaged rectangles, redraws clipped desktop,
window surfaces, chrome, taskbar and cursor into the screen back buffer, then
copies only that rectangle to VBE memory. Moving, hiding or closing a window
invalidates its previous rectangle as well. User drawing never writes hardware
memory. Presentation is serialized with graphics syscalls on this single CPU.
Software buffering does not provide hardware VSync.

Title-bar controls minimize, maximize/restore and request close. Taskbar buttons
restore/focus windows. Title-bar dragging is constrained to keep controls on
screen. Maximize allocates the new surface before releasing the old one and
sends a resize event. Allocation failure leaves the existing window intact.
The desktop icon and Start menu launch fresh user processes. The clock reads
the RTC as UTC; there is no timezone database.

The freestanding user toolkit in gui/toolkit batches bounded rectangle, line
and text commands. It provides labels, buttons, checkboxes, single/multiline
text fields, selectable lists, scrollbars, menu bars, context menus and dialogs.
Text fields support insertion, Backspace/Delete, Home/End, arrows, Shift
selection and Ctrl+A; multiline fields track the visible row. Text is ASCII.
Programs own their buffers and event loops; the kernel checks every pointer,
handle, draw count and coordinate. The widget demo exercises all controls.

## GUI ABI

All structures are fixed-width 32-bit layouts in gui/wm/protocol.h. Arguments
are EBX, ECX, EDX in order; result is signed EAX. Calls use int 0x80.

| Number | Operation | Arguments |
|---|---|---|
| 18 | Create window | pointer to g_window (dimensions, bounded title) |
| 19 | Draw | owned handle, g_draw array, count <= 256 |
| 20 | Poll event | owned handle, writable g_event; returns 1 or 0 |
| 21 | Close window | owned handle |
| 22 | Launch registered application | application index |
| 23 | System/desktop information | writable g_info |
| 24 | Theme | RGB accent, wallpaper index |

Errors include -14 for invalid user memory, -9 for unowned/missing handles,
-22 for invalid dimensions/commands and -12 for allocation failure.
Mouse events use client coordinates, current button mask and transition bits:
pressed bits 0..2, released bits 8..10. Keyboard events include pressed state,
ASCII, the driver's key ID and modifier flags. Resize events carry new client
dimensions. Window close is a request; well-behaved toolkit apps exit after
releasing the handle. The system does not yet forcibly kill unresponsive apps.
