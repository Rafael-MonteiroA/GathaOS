#!/usr/bin/env bash
set -euo pipefail
exec python3 tools/smoke_test.py "${1:-dist/gathaos-stage1-core.iso}" "${2:-none}" "${3:-build/none/kernel.elf}"
