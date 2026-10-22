#!/usr/bin/env bash
set -euo pipefail
exec "${QEMU:-qemu-system-i386}" -cdrom "${1:-dist/gathaos-stage10-network.iso}" \
    -drive "file=${2:-dist/gathaos-network-disk.img},format=raw,if=ide,index=0" \
    -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
