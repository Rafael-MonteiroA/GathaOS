#!/usr/bin/env bash
set -euo pipefail
exec "${QEMU:-qemu-system-i386}" -cdrom "${1:-dist/gathaos-stage4-tasks.iso}" \
    -boot d -m 256M -vga std -serial stdio -no-reboot
