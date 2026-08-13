#!/bin/bash
# ci_qemu_smoke.sh — Boot QEMU headless, type ls/cat/ps/uptime, require serial markers.
# Keyboard is PS/2: HMP sendkey injects scancodes (host TTY would not reach SYS_GETS).
# Hard timeout: QEMU stderr must not be a PIPE (deadlock on GitHub Actions).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

KERNEL="${KERNEL:-build/ai_os.bin}"
INITRD="${INITRD:-my_initrd.tar}"
SMOKE_TIMEOUT="${SMOKE_TIMEOUT:-100}"

if [ ! -f "$KERNEL" ] || [ ! -f "$INITRD" ]; then
    echo "ERROR: missing $KERNEL or $INITRD — run 'make all' first"
    exit 1
fi

export KERNEL INITRD
export LOG="${LOG:-test_logs/ci-qemu-serial.log}"
export PYTHONUNBUFFERED=1

if ! grep -a -qF "Initrd / VFS" userspace/shell; then
    echo "ERROR: userspace/shell is stale (no syscall ls). Expected 'make -C userspace all'."
    file userspace/shell || true
    exit 1
fi

echo "=== QEMU smoke (timeout ${SMOKE_TIMEOUT}s) ==="
set +e
timeout --foreground --signal=KILL "${SMOKE_TIMEOUT}s" \
    python3 "$ROOT/tests/scripts/ci_qemu_syscalls.py"
rc=$?
set -e

if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
    echo "FAIL: qemu-smoke killed after ${SMOKE_TIMEOUT}s"
    if [ -f "$LOG" ]; then
        echo "=== serial log (tail) ==="
        tail -n 80 "$LOG" || true
    fi
    if [ -f test_logs/ci-qemu-stderr.log ]; then
        echo "=== qemu stderr (tail) ==="
        tail -n 40 test_logs/ci-qemu-stderr.log || true
    fi
    exit 1
fi

exit "$rc"
