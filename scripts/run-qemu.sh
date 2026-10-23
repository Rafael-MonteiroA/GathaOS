#!/usr/bin/env bash
set -euo pipefail
exec "${QEMU:-qemu-system-i386}" -cpu max -rtc base=utc -cdrom "${1:-dist/gathaos-stage12-browser.iso}" \
    -drive "file=${2:-dist/gathaos-browser-disk.img},format=raw,if=ide,index=0" \
    -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
