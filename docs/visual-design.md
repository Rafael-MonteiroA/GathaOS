# Visual design — clean early-2000s desktop

Original geometric artwork, rendered by Gatha's own primitives. Blue-gray window
chrome, light neutral client surfaces, thin borders, beveled silver buttons and
warm folder icons evoke early-2000s desktop software. Three restrained gradient
wallpapers have broad ribbons. Start rows highlight under the pointer; title
controls use drawn symbols instead of text approximations. Focused and inactive
windows have distinct title colors. Text remains high contrast.

The original font keeps the 8x16 cell ABI, with a six-pixel body and nine-pixel
cap height centered in the cell. This replaces the old vertically doubled 5x7
appearance. tools/make_font.py reproducibly regenerates gui/gfx/font.h from
original project artwork; no external font or image assets are imported.

Window/client coordinates and event hit regions are unchanged. The taskbar is
42 pixels high, captions 32 pixels, toolkit buttons 28 pixels and lists 24 pixels
per row. The wallpaper renders ribbon scanlines only within the damaged region.
Screenshot artifacts come directly from a running QEMU framebuffer, not mockups.

Color changes in Settings apply to window chrome and wallpaper. Application
controls share a fixed neutral/blue toolkit palette. Theme selection is not saved
across reboot. There is no antialiasing or display scaling in this release.
