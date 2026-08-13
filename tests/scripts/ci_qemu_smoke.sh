#!/bin/bash
# ci_qemu_smoke.sh — Boot QEMU headless, type ls/cat/ps/uptime, require serial markers.
# Keyboard is PS/2: HMP sendkey injects scancodes (host TTY would not reach SYS_GETS).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

KERNEL="${KERNEL:-build/ai_os.bin}"
INITRD="${INITRD:-my_initrd.tar}"

if [ ! -f "$KERNEL" ] || [ ! -f "$INITRD" ]; then
    echo "ERROR: missing $KERNEL or $INITRD — run 'make all' first"
    exit 1
fi

export KERNEL INITRD
export LOG="${LOG:-test_logs/ci-qemu-serial.log}"

exec python3 "$ROOT/tests/scripts/ci_qemu_syscalls.py"
