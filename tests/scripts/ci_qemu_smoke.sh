#!/bin/bash
# ci_qemu_smoke.sh — Boot QEMU headless and require the userspace shell in the serial log.
# QEMU is expected to still be running at timeout (-no-shutdown); that is success
# only if the serial log shows a real boot through the shell prompt.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

KERNEL="${KERNEL:-build/ai_os.bin}"
INITRD="${INITRD:-my_initrd.tar}"
LOG="${LOG:-test_logs/ci-qemu-serial.log}"
TIMEOUT_SEC="${TIMEOUT_SEC:-15}"

if [ ! -f "$KERNEL" ] || [ ! -f "$INITRD" ]; then
    echo "ERROR: missing $KERNEL or $INITRD — run 'make all' first"
    exit 1
fi

mkdir -p "$(dirname "$LOG")"
rm -f "$LOG"

echo "=== QEMU smoke test (${TIMEOUT_SEC}s, display=none) ==="
set +e
timeout --foreground --signal=KILL "${TIMEOUT_SEC}s" \
    qemu-system-i386 \
        -kernel "$KERNEL" \
        -initrd "$INITRD" \
        -m 128M \
        -display none \
        -serial "file:$LOG" \
        -machine type=pc,accel=tcg \
        -no-reboot \
        -no-shutdown
qemu_rc=$?
set -e

# 124 = timeout(1) sent SIGTERM/SIGKILL — expected because of -no-shutdown
# 137 = killed by SIGKILL
if [ "$qemu_rc" -ne 0 ] && [ "$qemu_rc" -ne 124 ] && [ "$qemu_rc" -ne 137 ]; then
    echo "ERROR: QEMU exited with code $qemu_rc (crash or missing binary)"
    if [ -f "$LOG" ]; then
        echo "=== serial log ==="
        cat "$LOG"
    fi
    exit 1
fi

if [ ! -s "$LOG" ]; then
    echo "ERROR: serial log empty or missing ($LOG) — boot did not start"
    exit 1
fi

echo "=== serial log (last 80 lines) ==="
tail -n 80 "$LOG" || true
echo ""

fail=0
# Markers that mean the kernel reached userspace, not just "QEMU still running".
for needle in "Lancement du Shell Utilisateur" "(-.-)" "IRQ1 (keyboard): ENABLED"; do
    if grep -q -F "$needle" "$LOG"; then
        echo "OK: found '$needle'"
    else
        echo "FAIL: missing '$needle' in $LOG"
        fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    echo "QEMU smoke test failed: the guest did not reach the interactive shell."
    exit 1
fi

echo "QEMU smoke test passed."
exit 0
