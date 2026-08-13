#!/usr/bin/env python3
"""Boot QEMU, type ls/cat/ps/uptime via HMP sendkey, assert serial output.

Used by ci_qemu_smoke.sh so GitHub Actions actually exercises the initrd and
kernel syscalls, not only the boot prompt.
"""
from __future__ import print_function

import os
import socket
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
KERNEL = os.environ.get("KERNEL", os.path.join(ROOT, "build", "ai_os.bin"))
INITRD = os.environ.get("INITRD", os.path.join(ROOT, "my_initrd.tar"))
LOG = os.environ.get("LOG", os.path.join(ROOT, "test_logs", "ci-qemu-serial.log"))
MON_PORT = int(os.environ.get("QEMU_MON_PORT", "45454"))
BOOT_TIMEOUT = float(os.environ.get("BOOT_TIMEOUT", "18"))
CMD_TIMEOUT = float(os.environ.get("CMD_TIMEOUT", "8"))


def log_text():
    if not os.path.isfile(LOG):
        return ""
    with open(LOG, "r", errors="replace") as f:
        return f.read()


def wait_needle(needle, timeout, proc):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if proc.poll() is not None:
            raise RuntimeError("QEMU exited early with code %s" % proc.returncode)
        if needle in log_text():
            return
        time.sleep(0.15)
    raise RuntimeError("timeout waiting for %r in serial log" % needle)


def monitor_connect(retries=40):
    last = None
    for _ in range(retries):
        try:
            s = socket.create_connection(("127.0.0.1", MON_PORT), timeout=1)
            s.settimeout(0.4)
            try:
                s.recv(4096)
            except socket.timeout:
                pass
            return s
        except (socket.error, OSError) as e:
            last = e
            time.sleep(0.15)
    raise RuntimeError("cannot connect to QEMU monitor: %s" % last)


def sendkeys(mon, keys):
    for k in keys:
        mon.sendall(("sendkey %s\n" % k).encode("ascii"))
        time.sleep(0.12)


def main():
    if not os.path.isfile(KERNEL) or not os.path.isfile(INITRD):
        print("ERROR: missing %s or %s — run 'make all' first" % (KERNEL, INITRD))
        return 1

    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    if os.path.isfile(LOG):
        os.remove(LOG)

    cmd = [
        "qemu-system-i386",
        "-kernel", KERNEL,
        "-initrd", INITRD,
        "-m", "128M",
        "-display", "none",
        "-serial", "file:" + LOG,
        "-monitor", "tcp:127.0.0.1:%d,server,nowait" % MON_PORT,
        "-machine", "type=pc,accel=tcg",
        "-no-reboot",
        "-no-shutdown",
    ]
    print("=== QEMU syscall smoke (sendkey ls/cat/ps/uptime) ===")
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    rc = 1
    mon = None
    try:
        wait_needle("(-.-)", BOOT_TIMEOUT, proc)
        wait_needle("SYS_GETS: Debut", BOOT_TIMEOUT, proc)
        time.sleep(0.4)
        mon = monitor_connect()

        commands = [
            ("ls", ["l", "s", "ret"], "hello.txt"),
            ("cat hello.txt",
             ["c", "a", "t", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret"],
             "demonstration"),
            ("ls bin", ["l", "s", "spc", "b", "i", "n", "ret"], "fake_ai"),
            ("ps", ["p", "s", "ret"], "Processus (noyau)"),
            ("uptime", ["u", "p", "t", "i", "m", "e", "ret"], "PIT ticks"),
        ]
        for name, keys, needle in commands:
            print("typing %s ..." % name)
            sendkeys(mon, keys)
            wait_needle(needle, CMD_TIMEOUT, proc)

        text = log_text()
        checks = [
            ("initrd ls", "hello.txt"),
            ("initrd ls", "startup.sh"),
            ("cat hello.txt", "Un autre fichier de demonstration"),
            ("ls bin", "fake_ai"),
            ("ps kernel", "kern"),
            ("ps shell", "shell"),
            ("uptime", "PIT ticks"),
        ]
        fail = 0
        for label, needle in checks:
            if needle in text:
                print("OK: %s (%r)" % (label, needle))
            else:
                print("FAIL: %s missing %r" % (label, needle))
                fail = 1
        if fail:
            print("=== serial log (tail) ===")
            print("\n".join(text.splitlines()[-80:]))
            return 1
        print("QEMU syscall smoke passed.")
        rc = 0
        return 0
    except Exception as e:
        print("FAIL: %s" % e)
        text = log_text()
        if text:
            print("=== serial log (tail) ===")
            print("\n".join(text.splitlines()[-80:]))
        err = proc.stderr.read().decode("utf-8", "replace") if proc.stderr else ""
        if err:
            print("=== qemu stderr ===")
            print(err[-2000:])
        return 1
    finally:
        if mon is not None:
            try:
                mon.close()
            except Exception:
                pass
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except Exception:
                proc.kill()
        # preserve rc from return — finally cannot change returned value in a
        # useful way here; callers use sys.exit(main()).
        _ = rc


if __name__ == "__main__":
    sys.exit(main())
