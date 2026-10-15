#!/usr/bin/env bash
set -euo pipefail
exec "${QEMU:-qemu-system-i386}" -cdrom "${1:-dist/gathaos-stage3-drivers.iso}" \
    -boot d -m 256M -vga std -serial stdio -no-reboot
