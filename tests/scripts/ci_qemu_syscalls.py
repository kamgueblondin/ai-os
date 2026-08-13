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
LOG_DIR = os.path.join(ROOT, "test_logs")
LOG = os.environ.get("LOG", os.path.join(LOG_DIR, "ci-qemu-serial.log"))
QEMU_ERR = os.environ.get("QEMU_ERR", os.path.join(LOG_DIR, "ci-qemu-stderr.log"))
MON_SOCK = os.environ.get("QEMU_MON_SOCK", os.path.join(LOG_DIR, "qemu-monitor.sock"))
BOOT_TIMEOUT = float(os.environ.get("BOOT_TIMEOUT", "18"))
CMD_TIMEOUT = float(os.environ.get("CMD_TIMEOUT", "8"))


def say(msg):
    sys.stdout.write(msg + "\n")
    sys.stdout.flush()


def log_text():
    if not os.path.isfile(LOG):
        return ""
    with open(LOG, "r", errors="replace") as f:
        return f.read()


def err_text():
    if not os.path.isfile(QEMU_ERR):
        return ""
    with open(QEMU_ERR, "r", errors="replace") as f:
        return f.read()


def wait_needle_from(needle, timeout, proc, start):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if proc.poll() is not None:
            raise RuntimeError(
                "QEMU exited early with code %s\nstderr: %s"
                % (proc.returncode, err_text()[-1500:])
            )
        text = log_text()
        if needle in text[start:]:
            return len(text)
        time.sleep(0.15)
    raise RuntimeError("timeout waiting for %r in serial log" % needle)


def wait_needle(needle, timeout, proc):
    wait_needle_from(needle, timeout, proc, 0)


def monitor_connect(retries=50):
    last = None
    for _ in range(retries):
        if not os.path.exists(MON_SOCK):
            time.sleep(0.1)
            continue
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(1)
            s.connect(MON_SOCK)
            try:
                s.recv(4096)
            except socket.timeout:
                pass
            return s
        except (socket.error, OSError) as e:
            last = e
            try:
                s.close()
            except Exception:
                pass
            time.sleep(0.1)
    raise RuntimeError("cannot connect to QEMU monitor: %s" % last)


def sendkeys(mon, keys):
    for k in keys:
        mon.sendall(("sendkey %s\n" % k).encode("ascii"))
        time.sleep(0.12)


def dump_logs():
    text = log_text()
    if text:
        say("=== serial log (tail) ===")
        say("\n".join(text.splitlines()[-80:]))
    err = err_text()
    if err:
        say("=== qemu stderr (tail) ===")
        say(err[-2000:])


def kill_qemu(proc):
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except Exception:
        proc.kill()
        try:
            proc.wait(timeout=2)
        except Exception:
            pass


def main():
    if not os.path.isfile(KERNEL) or not os.path.isfile(INITRD):
        say("ERROR: missing %s or %s — run 'make all' first" % (KERNEL, INITRD))
        return 1

    os.makedirs(os.path.dirname(os.path.abspath(LOG)) or ".", exist_ok=True)
    for path in (LOG, QEMU_ERR, MON_SOCK):
        try:
            os.remove(path)
        except OSError:
            pass

    cmd = [
        "qemu-system-i386",
        "-kernel", KERNEL,
        "-initrd", INITRD,
        "-m", "128M",
        "-display", "none",
        "-vga", "none",
        "-serial", "file:" + LOG,
        "-monitor", "unix:%s,server,nowait" % MON_SOCK,
        "-machine", "type=pc,accel=tcg",
        "-no-reboot",
        "-no-shutdown",
    ]
    say("=== QEMU syscall smoke (sendkey ls/cat/ps/spawn/kill/mkdir/cd/cp/uptime) ===")
    err_f = open(QEMU_ERR, "wb")
    proc = subprocess.Popen(
        cmd,
        stdout=err_f,
        stderr=err_f,
        start_new_session=True,
    )
    mon = None
    try:
        wait_needle("(-.-)", BOOT_TIMEOUT, proc)
        wait_needle("SYS_GETS: Debut", BOOT_TIMEOUT, proc)
        time.sleep(0.4)
        mon = monitor_connect()

        commands = [
            ("ls", ["l", "s", "ret"], "Initrd / VFS"),
            ("cat hello.txt",
             ["c", "a", "t", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret"],
             "demonstration"),
            ("ls bin", ["l", "s", "spc", "b", "i", "n", "ret"], "fake_ai"),
            ("ps", ["p", "s", "ret"], "Processus (noyau)"),
        ]
        for name, keys, needle in commands:
            say("typing %s ..." % name)
            sendkeys(mon, keys)
            wait_needle(needle, CMD_TIMEOUT, proc)

        say("typing spawn idle ...")
        mark = len(log_text())
        sendkeys(mon, ["s", "p", "a", "w", "n", "spc", "i", "d", "l", "e", "ret"])
        wait_needle_from("spawn ok pid", CMD_TIMEOUT, proc, mark)

        say("typing ps (after spawn) ...")
        mark = len(log_text())
        sendkeys(mon, ["p", "s", "ret"])
        wait_needle_from("user  idle", CMD_TIMEOUT, proc, mark)

        say("typing kill 2 ...")
        mark = len(log_text())
        sendkeys(mon, ["k", "i", "l", "l", "spc", "2", "ret"])
        wait_needle_from("Processus 2 termine", CMD_TIMEOUT, proc, mark)

        say("typing ps (after kill) ...")
        mark = len(log_text())
        sendkeys(mon, ["p", "s", "ret"])
        wait_needle_from("Processus (noyau)", CMD_TIMEOUT, proc, mark)
        after_kill_ps = log_text()[mark:]
        if "user  idle" in after_kill_ps:
            raise RuntimeError("idle still listed in ps after kill 2")

        say("typing uptime ...")
        sendkeys(mon, ["u", "p", "t", "i", "m", "e", "ret"])
        wait_needle("PIT ticks", CMD_TIMEOUT, proc)

        say("typing mkdir mydir ...")
        mark = len(log_text())
        sendkeys(mon, ["m", "k", "d", "i", "r", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("mkdir ok mydir", CMD_TIMEOUT, proc, mark)

        say("typing cd mydir ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("cd ok mydir", CMD_TIMEOUT, proc, mark)

        say("typing pwd ...")
        mark = len(log_text())
        sendkeys(mon, ["p", "w", "d", "ret"])
        wait_needle_from("/mydir", CMD_TIMEOUT, proc, mark)

        say("typing cd .. ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "dot", "dot", "ret"])
        wait_needle_from("cd ok ..", CMD_TIMEOUT, proc, mark)

        say("typing cp hello.txt copy.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "c", "p", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t",
            "spc", "c", "o", "p", "y", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("cp ok copy.txt", CMD_TIMEOUT, proc, mark)

        say("typing ls (after cp) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("copy.txt", CMD_TIMEOUT, proc, mark)

        say("typing cat copy.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "c", "a", "t", "spc", "c", "o", "p", "y", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("demonstration", CMD_TIMEOUT, proc, mark)

        say("typing rm copy.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "r", "m", "spc", "c", "o", "p", "y", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("rm ok copy.txt", CMD_TIMEOUT, proc, mark)

        say("typing ls (after rm copy) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("Initrd / VFS", CMD_TIMEOUT, proc, mark)
        wait_needle_from("Total:", CMD_TIMEOUT, proc, mark)
        after_rm_copy = log_text()[mark:]
        if "copy.txt" in after_rm_copy:
            raise RuntimeError("copy.txt still listed in ls after rm")
        if "mydir" not in after_rm_copy:
            raise RuntimeError("ls after cd/cp missing mydir")
        if "hello.txt" not in after_rm_copy:
            raise RuntimeError("ls after cd/cp missing hello.txt")

        say("typing rmdir mydir ...")
        mark = len(log_text())
        sendkeys(mon, ["r", "m", "d", "i", "r", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("rmdir ok mydir", CMD_TIMEOUT, proc, mark)

        say("typing ls (after rmdir) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("Initrd / VFS", CMD_TIMEOUT, proc, mark)
        wait_needle_from("Total:", CMD_TIMEOUT, proc, mark)
        after_rmdir_ls = log_text()[mark:]
        if "mydir" in after_rmdir_ls:
            raise RuntimeError("mydir still listed in ls after rmdir")

        text = log_text()
        checks = [
            ("syscall ls", "Initrd / VFS"),
            ("initrd ls", "hello.txt"),
            ("initrd ls", "startup.sh"),
            ("cat hello.txt", "Un autre fichier de demonstration"),
            ("ls bin", "fake_ai"),
            ("ps kernel table", "Processus (noyau)"),
            ("ps kernel", "kern"),
            ("ps shell", "shell"),
            ("spawn idle", "spawn ok pid"),
            ("ps shows idle", "user  idle"),
            ("kill 2", "Processus 2 termine"),
            ("uptime", "PIT ticks"),
            ("mkdir overlay", "mkdir ok mydir"),
            ("cd overlay", "cd ok mydir"),
            ("pwd overlay", "/mydir"),
            ("cd parent", "cd ok .."),
            ("cp overlay", "cp ok copy.txt"),
            ("rm overlay", "rm ok copy.txt"),
            ("rmdir overlay", "rmdir ok mydir"),
        ]
        fail = 0
        for label, needle in checks:
            if needle in text:
                say("OK: %s (%r)" % (label, needle))
            else:
                say("FAIL: %s missing %r" % (label, needle))
                fail = 1
        if fail:
            dump_logs()
            return 1
        say("QEMU syscall smoke passed.")
        return 0
    except Exception as e:
        say("FAIL: %s" % e)
        dump_logs()
        return 1
    finally:
        if mon is not None:
            try:
                mon.close()
            except Exception:
                pass
        kill_qemu(proc)
        err_f.close()
        try:
            os.remove(MON_SOCK)
        except OSError:
            pass


if __name__ == "__main__":
    sys.exit(main())
