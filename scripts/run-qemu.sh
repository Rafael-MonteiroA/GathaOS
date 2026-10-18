#!/usr/bin/env bash
set -euo pipefail
exec "${QEMU:-qemu-system-i386}" -cdrom "${1:-dist/gathaos-stage9-foundation.iso}" \
    -drive "file=${2:-dist/gathaos-foundation-disk.img},format=raw,if=ide,index=0" \
    -boot d -m 256M -vga std -serial stdio
