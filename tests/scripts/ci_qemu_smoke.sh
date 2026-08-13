#!/bin/bash
# ci_qemu_smoke.sh — Boot QEMU headless, type ls/cat/ps/uptime, require serial markers.
# Keyboard is PS/2: HMP sendkey injects scancodes (host TTY would not reach SYS_GETS).
# Hard timeout: QEMU stderr must not be a PIPE (deadlock on GitHub Actions).
# Overlay smoke and shell extras are separate boots so extra sendkeys do not ghost overlay.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

KERNEL="${KERNEL:-build/ai_os.bin}"
INITRD="${INITRD:-my_initrd.tar}"
OVERLAY_TIMEOUT="${OVERLAY_TIMEOUT:-180}"
EXTRAS_TIMEOUT="${EXTRAS_TIMEOUT:-90}"

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
if ! grep -a -qF "env ok" userspace/shell; then
    echo "ERROR: userspace/shell is stale (no env ok needle)."
    exit 1
fi

run_python() {
    local name="$1"
    local timeout_s="$2"
    local script="$3"
    echo "=== QEMU ${name} (timeout ${timeout_s}s) ==="
    set +e
    timeout --foreground --signal=KILL "${timeout_s}s" python3 "$script"
    local rc=$?
    set -e
    if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
        echo "FAIL: qemu-smoke ${name} killed after ${timeout_s}s"
        return 1
    fi
    return "$rc"
}

run_python overlay "$OVERLAY_TIMEOUT" "$ROOT/tests/scripts/ci_qemu_syscalls.py"
run_python extras "$EXTRAS_TIMEOUT" "$ROOT/tests/scripts/ci_qemu_shell_extras.py"
