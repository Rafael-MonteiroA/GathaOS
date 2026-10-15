#!/usr/bin/env bash
set -euo pipefail
image=$1
output=$2
build=$3
mkdir -p "$build/iso/boot" "$(dirname "$output")"
cp "$image" "$build/iso/boot/gatha.img"
# Fixed metadata and volume dates make clean builds byte-for-byte reproducible.
touch -t 202601010000.00 "$build/iso/boot/gatha.img" "$build/iso/boot" "$build/iso"
xorriso -as mkisofs -quiet -V GATHAOS_STAGE4 \
    --modification-date=2026010100000000 \
    --set_all_file_dates 2026010100000000 \
    -b boot/gatha.img -c boot/boot.cat -o "$build/gathaos.iso" "$build/iso"
# Stage artifacts are immutable; clean does not remove dist/.
if [[ -e "$output" ]]; then
    if cmp -s "$build/gathaos.iso" "$output"; then
        echo "Verified identical existing ISO: $output"
    else
        echo "Refusing to overwrite $output; select ISO=dist/...-fix<K>.iso" >&2
        exit 1
    fi
else
    cp "$build/gathaos.iso" "$output"
fi
