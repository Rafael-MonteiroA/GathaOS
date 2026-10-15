#!/usr/bin/env bash
set -euo pipefail
exec python3 tools/smoke_test.py "${1:-dist/gathaos-stage0-boot.iso}"
