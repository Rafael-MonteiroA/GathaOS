#!/usr/bin/env bash
set -euo pipefail
exec "${QEMU:-qemu-system-i386}" -cpu max -rtc base=utc -cdrom "${1:-dist/gathaos-stage15-large-boot.iso}" \
    -drive "file=${2:-dist/gathaos-large-boot-disk.img},format=raw,if=ide,index=0" \
    -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
