#!/usr/bin/env bash
set -euo pipefail
# Ubuntu / WSL2 host tooling only. No host OS code enters the guest image.
if [[ $EUID -eq 0 ]]; then
    SUDO=()
else
    SUDO=(sudo)
fi
"${SUDO[@]}" apt-get update
"${SUDO[@]}" apt-get install -y clang lld llvm nasm make xorriso qemu-system-x86 python3 python3-cryptography openssl
clang --target=i686-elf -ffreestanding -x c -c /dev/null -o /tmp/gatha-toolchain-$$.o
rm -f /tmp/gatha-toolchain-$$.o
echo 'GathaOS host tools installed (Clang i686-elf cross target).'
