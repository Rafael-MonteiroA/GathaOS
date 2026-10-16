#!/usr/bin/env bash
set -euo pipefail
exec "${QEMU:-qemu-system-i386}" -cdrom "${1:-dist/gathaos-stage7-apps.iso}" \
    -drive "file=${2:-dist/gathaos-disk.img},format=raw,if=ide,index=0" \
    -boot d -m 256M -vga std -serial stdio -no-reboot
