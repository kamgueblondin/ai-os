#!/usr/bin/env python3
"""Boot QEMU, type ls/cat/ps/uptime/mem/getpid via HMP sendkey, assert serial output.

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


def parse_spawn_pid(text):
    key = "spawn ok pid "
    i = text.find(key)
    if i < 0:
        raise RuntimeError("spawn ok pid not in log")
    digits = []
    for ch in text[i + len(key):]:
        if ch.isdigit():
            digits.append(ch)
        elif digits:
            break
    if not digits:
        raise RuntimeError("no pid after spawn ok pid")
    return "".join(digits)


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


def drain_monitor(mon):
    """Read HMP replies until the socket is quiet so sendkey commands do not pile up."""
    mon.settimeout(0.05)
    while True:
        try:
            data = mon.recv(8192)
            if not data:
                break
        except socket.timeout:
            break


def sendkeys(mon, keys):
    for k in keys:
        mon.sendall(("sendkey %s\n" % k).encode("ascii"))
        drain_monitor(mon)
        time.sleep(0.16)


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
    say("=== QEMU syscall smoke (sendkey ls/cat/stat/test/head/sort/ai/ps/spawn/kill/uptime/mem/getpid/whoami/which/mkdir/cd/cp/mv/write/touch/append/grep/wc) ===")
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
            ("stat hello.txt",
             ["s", "t", "a", "t", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret"],
             "stat file hello.txt"),
            ("head hello.txt",
             ["h", "e", "a", "d", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret"],
             "head ok 1 hello.txt"),
            ("sort hello.txt",
             ["s", "o", "r", "t", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret"],
             "sort ok 1 hello.txt"),
            ("ls bin", ["l", "s", "spc", "b", "i", "n", "ret"], "fake_ai"),
            ("whoami", ["w", "h", "o", "a", "m", "i", "ret"], "whoami ok root"),
            ("which ls",
             ["w", "h", "i", "c", "h", "spc", "l", "s", "ret"],
             "which ok builtin ls"),
            ("ai hello",
             ["a", "i", "spc", "h", "e", "l", "l", "o", "ret"],
             "ai ok"),
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
        idle_pid = parse_spawn_pid(log_text()[mark:])
        say("spawned idle pid %s" % idle_pid)

        say("typing ps (after spawn) ...")
        mark = len(log_text())
        sendkeys(mon, ["p", "s", "ret"])
        wait_needle_from("user  idle", CMD_TIMEOUT, proc, mark)

        say("typing kill %s ..." % idle_pid)
        mark = len(log_text())
        sendkeys(mon, ["k", "i", "l", "l", "spc"] + list(idle_pid) + ["ret"])
        kill_needle = "Processus %s termine" % idle_pid
        wait_needle_from(kill_needle, CMD_TIMEOUT, proc, mark)
        time.sleep(0.3)

        say("typing ps (after kill) ...")
        mark = len(log_text())
        sendkeys(mon, ["p", "s", "ret"])
        wait_needle_from("Processus (noyau)", CMD_TIMEOUT, proc, mark)
        after_kill_ps = log_text()[mark:]
        if "user  idle" in after_kill_ps:
            raise RuntimeError("idle still listed in ps after kill %s" % idle_pid)

        say("typing uptime ...")
        sendkeys(mon, ["u", "p", "t", "i", "m", "e", "ret"])
        wait_needle("PIT ticks", CMD_TIMEOUT, proc)

        say("typing mem ...")
        mark = len(log_text())
        sendkeys(mon, ["m", "e", "m", "ret"])
        wait_needle_from("mem ok", CMD_TIMEOUT, proc, mark)

        say("typing getpid ...")
        mark = len(log_text())
        sendkeys(mon, ["g", "e", "t", "p", "i", "d", "ret"])
        wait_needle_from("getpid ok", CMD_TIMEOUT, proc, mark)

        say("typing test f hello.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "t", "e", "s", "t", "spc", "f", "spc",
            "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("test ok file hello.txt", CMD_TIMEOUT, proc, mark)

        say("typing mkdir mydir ...")
        mark = len(log_text())
        sendkeys(mon, ["m", "k", "d", "i", "r", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("mkdir ok mydir", CMD_TIMEOUT, proc, mark)

        say("typing stat mydir ...")
        mark = len(log_text())
        sendkeys(mon, ["s", "t", "a", "t", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("stat dir mydir", CMD_TIMEOUT, proc, mark)

        say("typing test d mydir ...")
        mark = len(log_text())
        sendkeys(mon, [
            "t", "e", "s", "t", "spc", "d", "spc", "m", "y", "d", "i", "r", "ret",
        ])
        wait_needle_from("test ok dir mydir", CMD_TIMEOUT, proc, mark)

        say("typing cd mydir ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("cd ok mydir", CMD_TIMEOUT, proc, mark)

        say("typing pwd ...")
        mark = len(log_text())
        sendkeys(mon, ["p", "w", "d", "ret"])
        wait_needle_from("/mydir", CMD_TIMEOUT, proc, mark)

        say("typing mkdir sub ...")
        mark = len(log_text())
        sendkeys(mon, ["m", "k", "d", "i", "r", "spc", "s", "u", "b", "ret"])
        wait_needle_from("mkdir ok sub", CMD_TIMEOUT, proc, mark)

        say("typing ls (inside mydir) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("sub", CMD_TIMEOUT, proc, mark)

        say("typing cd .. (to root) ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "dot", "dot", "ret"])
        wait_needle_from("cd ok ..", CMD_TIMEOUT, proc, mark)

        say("typing rmdir mydir (not empty) ...")
        mark = len(log_text())
        sendkeys(mon, ["r", "m", "d", "i", "r", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("repertoire non vide", CMD_TIMEOUT, proc, mark)

        say("typing cd mydir (cleanup sub) ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("cd ok mydir", CMD_TIMEOUT, proc, mark)

        say("typing rmdir sub ...")
        mark = len(log_text())
        sendkeys(mon, ["r", "m", "d", "i", "r", "spc", "s", "u", "b", "ret"])
        wait_needle_from("rmdir ok sub", CMD_TIMEOUT, proc, mark)

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

        say("typing mv copy.txt notes.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "m", "v", "spc", "c", "o", "p", "y", "dot", "t", "x", "t",
            "spc", "n", "o", "t", "e", "s", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("mv ok notes.txt", CMD_TIMEOUT, proc, mark)

        say("typing ls (after mv) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("notes.txt", CMD_TIMEOUT, proc, mark)
        after_mv_ls = log_text()[mark:]
        if "copy.txt" in after_mv_ls:
            raise RuntimeError("copy.txt still listed after mv")

        say("typing cat notes.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "c", "a", "t", "spc", "n", "o", "t", "e", "s", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("demonstration", CMD_TIMEOUT, proc, mark)

        say("typing rm notes.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "r", "m", "spc", "n", "o", "t", "e", "s", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("rm ok notes.txt", CMD_TIMEOUT, proc, mark)

        say("typing ls (after rm notes) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("Initrd / VFS", CMD_TIMEOUT, proc, mark)
        wait_needle_from("Total:", CMD_TIMEOUT, proc, mark)
        after_rm_notes = log_text()[mark:]
        if "notes.txt" in after_rm_notes:
            raise RuntimeError("notes.txt still listed in ls after rm")
        if "mydir" not in after_rm_notes:
            raise RuntimeError("ls after nested/mv missing mydir")
        if "hello.txt" not in after_rm_notes:
            raise RuntimeError("ls after nested/mv missing hello.txt")

        say("typing cp hello.txt mydir ...")
        mark = len(log_text())
        sendkeys(mon, [
            "c", "p", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t",
            "spc", "m", "y", "d", "i", "r", "ret",
        ])
        wait_needle_from("cp ok hello.txt", CMD_TIMEOUT, proc, mark)

        say("typing ls mydir ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "spc", "m", "y", "d", "i", "r", "ret"])
        wait_needle_from("hello.txt", CMD_TIMEOUT, proc, mark)

        say("typing cp mydir cpd ...")
        mark = len(log_text())
        sendkeys(mon, [
            "c", "p", "spc", "m", "y", "d", "i", "r", "spc", "c", "p", "d", "ret",
        ])
        wait_needle_from("cp ok cpd", CMD_TIMEOUT, proc, mark)

        say("typing mv mydir newd ...")
        mark = len(log_text())
        sendkeys(mon, [
            "m", "v", "spc", "m", "y", "d", "i", "r", "spc", "n", "e", "w", "d", "ret",
        ])
        wait_needle_from("mv ok newd", CMD_TIMEOUT, proc, mark)

        say("typing ls (after mv dir) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("Initrd / VFS", CMD_TIMEOUT, proc, mark)
        wait_needle_from("Total:", CMD_TIMEOUT, proc, mark)
        after_mv_dir = log_text()[mark:]
        if "mydir" in after_mv_dir:
            raise RuntimeError("mydir still listed after mv mydir newd")
        if "newd" not in after_mv_dir:
            raise RuntimeError("ls after mv dir missing newd")
        if "cpd" not in after_mv_dir:
            raise RuntimeError("ls after mv dir missing cpd copy")

        say("typing ls newd ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "spc", "n", "e", "w", "d", "ret"])
        wait_needle_from("hello.txt", CMD_TIMEOUT, proc, mark)

        say("typing cd newd ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "n", "e", "w", "d", "ret"])
        wait_needle_from("cd ok newd", CMD_TIMEOUT, proc, mark)

        say("typing cat hello.txt (in newd) ...")
        mark = len(log_text())
        sendkeys(mon, [
            "c", "a", "t", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("demonstration", CMD_TIMEOUT, proc, mark)

        say("typing rm hello.txt (in newd) ...")
        mark = len(log_text())
        sendkeys(mon, [
            "r", "m", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("rm ok hello.txt", CMD_TIMEOUT, proc, mark)

        say("typing cd .. (after mv dir) ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "dot", "dot", "ret"])
        wait_needle_from("cd ok ..", CMD_TIMEOUT, proc, mark)

        say("typing rmdir newd ...")
        mark = len(log_text())
        sendkeys(mon, ["r", "m", "d", "i", "r", "spc", "n", "e", "w", "d", "ret"])
        wait_needle_from("rmdir ok newd", CMD_TIMEOUT, proc, mark)

        say("typing ls (after rmdir) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("Initrd / VFS", CMD_TIMEOUT, proc, mark)
        wait_needle_from("Total:", CMD_TIMEOUT, proc, mark)
        after_rmdir_ls = log_text()[mark:]
        if "mydir" in after_rmdir_ls:
            raise RuntimeError("mydir still listed in ls after rmdir")
        if "newd" in after_rmdir_ls:
            raise RuntimeError("newd still listed in ls after rmdir")
        if "cpd" not in after_rmdir_ls:
            raise RuntimeError("cpd missing after rmdir newd")

        say("typing cd cpd ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "c", "p", "d", "ret"])
        wait_needle_from("cd ok cpd", CMD_TIMEOUT, proc, mark)

        say("typing cat hello.txt (in cpd) ...")
        mark = len(log_text())
        sendkeys(mon, [
            "c", "a", "t", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("demonstration", CMD_TIMEOUT, proc, mark)

        say("typing rm hello.txt (in cpd) ...")
        mark = len(log_text())
        sendkeys(mon, [
            "r", "m", "spc", "h", "e", "l", "l", "o", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("rm ok hello.txt", CMD_TIMEOUT, proc, mark)

        say("typing cd .. (after cp dir) ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "d", "spc", "dot", "dot", "ret"])
        wait_needle_from("cd ok ..", CMD_TIMEOUT, proc, mark)

        say("typing rmdir cpd ...")
        mark = len(log_text())
        sendkeys(mon, ["r", "m", "d", "i", "r", "spc", "c", "p", "d", "ret"])
        wait_needle_from("rmdir ok cpd", CMD_TIMEOUT, proc, mark)

        say("typing ls (after rmdir cpd) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("Initrd / VFS", CMD_TIMEOUT, proc, mark)
        wait_needle_from("Total:", CMD_TIMEOUT, proc, mark)
        after_rmdir_cpd = log_text()[mark:]
        if "cpd" in after_rmdir_cpd:
            raise RuntimeError("cpd still listed in ls after rmdir")

        say("typing touch z.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "t", "o", "u", "c", "h", "spc", "z", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("touch ok z.txt", CMD_TIMEOUT, proc, mark)

        say("typing stat z.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "s", "t", "a", "t", "spc", "z", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("stat file z.txt 0", CMD_TIMEOUT, proc, mark)

        say("typing append z.txt zap ...")
        mark = len(log_text())
        sendkeys(mon, [
            "a", "p", "p", "e", "n", "d", "spc", "z", "dot", "t", "x", "t",
            "spc", "z", "a", "p", "ret",
        ])
        wait_needle_from("append ok z.txt", CMD_TIMEOUT, proc, mark)

        say("typing stat z.txt (after append) ...")
        mark = len(log_text())
        sendkeys(mon, [
            "s", "t", "a", "t", "spc", "z", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("stat file z.txt 4", CMD_TIMEOUT, proc, mark)

        say("typing rm z.txt ...")
        mark = len(log_text())
        sendkeys(mon, ["r", "m", "spc", "z", "dot", "t", "x", "t", "ret"])
        wait_needle_from("rm ok z.txt", CMD_TIMEOUT, proc, mark)

        say("typing write hi.txt ping ...")
        mark = len(log_text())
        sendkeys(mon, [
            "w", "r", "i", "t", "e", "spc", "h", "i", "dot", "t", "x", "t",
            "spc", "p", "i", "n", "g", "ret",
        ])
        wait_needle_from("write ok hi.txt", CMD_TIMEOUT, proc, mark)

        say("typing cat hi.txt ...")
        mark = len(log_text())
        sendkeys(mon, ["c", "a", "t", "spc", "h", "i", "dot", "t", "x", "t", "ret"])
        wait_needle_from("ping", CMD_TIMEOUT, proc, mark)

        say("typing grep ping hi.txt ...")
        mark = len(log_text())
        sendkeys(mon, [
            "g", "r", "e", "p", "spc", "p", "i", "n", "g", "spc",
            "h", "i", "dot", "t", "x", "t", "ret",
        ])
        wait_needle_from("grep hits 1", CMD_TIMEOUT, proc, mark)

        say("typing wc hi.txt ...")
        mark = len(log_text())
        sendkeys(mon, ["w", "c", "spc", "h", "i", "dot", "t", "x", "t", "ret"])
        wait_needle_from("wc ok 1 1 5 hi.txt", CMD_TIMEOUT, proc, mark)

        say("typing rm hi.txt ...")
        mark = len(log_text())
        sendkeys(mon, ["r", "m", "spc", "h", "i", "dot", "t", "x", "t", "ret"])
        wait_needle_from("rm ok hi.txt", CMD_TIMEOUT, proc, mark)

        say("typing ls (after rm hi) ...")
        mark = len(log_text())
        sendkeys(mon, ["l", "s", "ret"])
        wait_needle_from("Initrd / VFS", CMD_TIMEOUT, proc, mark)
        wait_needle_from("Total:", CMD_TIMEOUT, proc, mark)
        after_rm_hi = log_text()[mark:]
        if "hi.txt" in after_rm_hi:
            raise RuntimeError("hi.txt still listed in ls after rm")

        text = log_text()
        checks = [
            ("syscall ls", "Initrd / VFS"),
            ("initrd ls", "hello.txt"),
            ("initrd ls", "startup.sh"),
            ("cat hello.txt", "Un autre fichier de demonstration"),
            ("ls bin", "fake_ai"),
            ("ai exec", "AI: bonjour"),
            ("ai exec returned", "ai ok"),
            ("ps kernel table", "Processus (noyau)"),
            ("ps kernel", "kern"),
            ("ps shell", "shell"),
            ("spawn idle", "spawn ok pid"),
            ("ps shows idle", "user  idle"),
            ("kill idle", "Processus %s termine" % idle_pid),
            ("uptime", "PIT ticks"),
            ("mem pmm", "mem ok"),
            ("getpid", "getpid ok"),
            ("whoami", "whoami ok root"),
            ("which builtin", "which ok builtin ls"),
            ("mkdir overlay", "mkdir ok mydir"),
            ("cd overlay", "cd ok mydir"),
            ("pwd overlay", "/mydir"),
            ("mkdir nested", "mkdir ok sub"),
            ("rmdir notempty", "repertoire non vide"),
            ("rmdir nested", "rmdir ok sub"),
            ("cp overlay", "cp ok copy.txt"),
            ("mv overlay", "mv ok notes.txt"),
            ("rm overlay", "rm ok notes.txt"),
            ("cp into dir", "cp ok hello.txt"),
            ("cp overlay dir", "cp ok cpd"),
            ("mv overlay dir", "mv ok newd"),
            ("cd renamed dir", "cd ok newd"),
            ("rmdir overlay", "rmdir ok newd"),
            ("cd copied dir", "cd ok cpd"),
            ("rmdir copied dir", "rmdir ok cpd"),
            ("write overlay", "write ok hi.txt"),
            ("touch overlay", "touch ok z.txt"),
            ("stat empty", "stat file z.txt 0"),
            ("append overlay", "append ok z.txt"),
            ("stat appended", "stat file z.txt 4"),
            ("grep overlay", "grep hits 1"),
            ("wc overlay", "wc ok 1 1 5 hi.txt"),
            ("rm write", "rm ok hi.txt"),
            ("stat file", "stat file hello.txt"),
            ("test file", "test ok file hello.txt"),
            ("head initrd", "head ok 1 hello.txt"),
            ("sort initrd", "sort ok 1 hello.txt"),
            ("stat dir", "stat dir mydir"),
            ("test dir", "test ok dir mydir"),
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
