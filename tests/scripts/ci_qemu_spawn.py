#!/usr/bin/env python3
"""QEMU smoke: spawn idle actually runs (idle ok), yield switches, kill removes it."""
from __future__ import print_function

import os
import re
import socket
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
KERNEL = os.environ.get("KERNEL", os.path.join(ROOT, "build", "ai_os.bin"))
INITRD = os.environ.get("INITRD", os.path.join(ROOT, "my_initrd.tar"))
LOG_DIR = os.path.join(ROOT, "test_logs")
LOG = os.environ.get("SPAWN_LOG", os.path.join(LOG_DIR, "ci-qemu-spawn-serial.log"))
QEMU_ERR = os.environ.get("SPAWN_ERR", os.path.join(LOG_DIR, "ci-qemu-spawn-stderr.log"))
MON_SOCK = os.environ.get("SPAWN_MON_SOCK", os.path.join(LOG_DIR, "qemu-spawn-monitor.sock"))
BOOT_TIMEOUT = float(os.environ.get("BOOT_TIMEOUT", "40"))
CMD_TIMEOUT = float(os.environ.get("CMD_TIMEOUT", "20"))


def say(message):
    sys.stdout.write(message + "\n")
    sys.stdout.flush()


def log_text():
    try:
        with open(LOG, "r", errors="replace") as handle:
            return handle.read()
    except OSError:
        return ""


def wait_for(proc, needle, timeout, start=0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise RuntimeError("QEMU stopped unexpectedly; log tail:\n%s" % log_text()[-2000:])
        if needle in log_text()[start:]:
            time.sleep(0.35)
            return
        time.sleep(0.15)
    raise RuntimeError("timeout waiting for %r; log tail:\n%s" % (needle, log_text()[-2000:]))


def parse_spawn_pid(text, program):
    key = "spawn ok pid "
    idx = text.find(key)
    if idx < 0:
        raise RuntimeError("spawn ok pid not in log")
    tail = text[idx + len(key):]
    # Le timer peut entrelacer un changement de contexte dans l’écriture
    # série de la commande. La terminaison stable reste « <pid> programme ».
    match = re.search(r"(?:^|\n)([0-9]+) " + re.escape(program), tail)
    if match:
        return match.group(1)
    direct = re.match(r"([0-9]+)", tail)
    if direct:
        return direct.group(1)
    raise RuntimeError("no pid after spawn ok pid")


def qemu_disk_args():
    disk = os.environ.get("OVERLAY_DISK", os.path.join(ROOT, "build", "overlay.img"))
    if os.path.isfile(disk):
        return ["-drive", "file=%s,format=raw,if=ide,cache=writethrough" % disk]
    return []


def monitor_connect():
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if os.path.exists(MON_SOCK):
            client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                client.connect(MON_SOCK)
                client.settimeout(0.1)
                try:
                    client.recv(4096)
                except socket.timeout:
                    pass
                return client
            except OSError:
                client.close()
        time.sleep(0.1)
    raise RuntimeError("QEMU monitor unavailable")


def drain_monitor(client):
    client.settimeout(0.05)
    while True:
        try:
            data = client.recv(8192)
            if not data:
                break
        except socket.timeout:
            break


def send_command(client, command):
    aliases = {" ": "spc", "-": "minus", ".": "dot"}
    for char in command:
        client.sendall(("sendkey %s\n" % aliases.get(char, char.lower())).encode("ascii"))
        drain_monitor(client)
        time.sleep(0.20)
    client.sendall(b"sendkey ret\n")
    drain_monitor(client)


def send_command_until(client, command, marker, proc, attempts=3):
    failure = None
    for _ in range(attempts):
        start = len(log_text())
        send_command(client, command)
        try:
            wait_for(proc, marker, CMD_TIMEOUT, start)
            return start
        except RuntimeError as error:
            failure = error
            time.sleep(0.4)
    raise failure


def terminate(proc):
    if proc is None or proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=4)
    except subprocess.TimeoutExpired:
        proc.kill()


def main():
    if not os.path.isfile(KERNEL) or not os.path.isfile(INITRD):
        raise RuntimeError("missing build artefacts; run make all first")
    os.makedirs(LOG_DIR, exist_ok=True)
    for path in (LOG, QEMU_ERR, MON_SOCK):
        try:
            os.remove(path)
        except OSError:
            pass

    proc = None
    monitor = None
    try:
        with open(QEMU_ERR, "wb") as err:
            proc = subprocess.Popen([
                "qemu-system-i386", "-cpu", "pentium3", "-kernel", KERNEL,
                "-initrd", INITRD, "-m", "1024M", "-display", "none", "-vga", "none",
                "-serial", "file:" + LOG, "-monitor", "unix:%s,server,nowait" % MON_SOCK,
                "-machine", "type=pc,accel=tcg", "-no-reboot", "-no-shutdown",
            ] + qemu_disk_args(), cwd=ROOT, stdout=err, stderr=err)
            wait_for(proc, "(-.-)", BOOT_TIMEOUT)
            wait_for(proc, "SYS_GETS: Debut", BOOT_TIMEOUT)
            monitor = monitor_connect()

            say("typing spawn idle ...")
            spawn_start = send_command_until(monitor, "spawn idle", "spawn ok pid", proc)
            idle_pid = parse_spawn_pid(log_text()[spawn_start:], "idle")
            say("spawned idle pid %s" % idle_pid)

            say("typing task-name %s sleeper ..." % idle_pid)
            send_command_until(monitor, "task-name %s sleeper" % idle_pid,
                               "task-name ok %s sleeper" % idle_pid, proc)

            say("typing task-capacity (one child) ...")
            send_command_until(monitor, "task-capacity", "task-capacity ok 3 16 13", proc)

            say("typing task-metrics %s ..." % idle_pid)
            send_command_until(monitor, "task-metrics %s" % idle_pid, "Parent : 1", proc)

            # La priorité haute peut légitimement affamer le shell dans cette
            # politique bornée ; la valeur normale prouve l’autorité du parent
            # tout en conservant le round-robin nécessaire à ce smoke.
            say("typing task-priority %s 2 ..." % idle_pid)
            send_command_until(monitor, "task-priority %s 2" % idle_pid,
                               "task-priority ok %s 2" % idle_pid, proc)

            say("typing yield ...")
            start = send_command_until(monitor, "yield", "yield ok", proc)
            wait_for(proc, "idle ok", CMD_TIMEOUT, spawn_start)

            say("typing ps ...")
            start = send_command_until(monitor, "ps", "%s    1" % idle_pid, proc)
            wait_for(proc, "user  sleeper", CMD_TIMEOUT, start)

            say("typing kill %s ..." % idle_pid)
            start = send_command_until(monitor, "kill %s" % idle_pid,
                                       "Processus %s termine" % idle_pid, proc)

            say("typing ipc-recv (killed event) ...")
            send_command_until(monitor, "ipc-recv",
                               "task-event child %s reason killed" % idle_pid, proc)

            say("typing child-result %s (killed) ..." % idle_pid)
            send_command_until(monitor, "child-result %s" % idle_pid,
                               "child-result ok %s -128 2" % idle_pid, proc)

            say("typing task-capacity (after kill) ...")
            send_command_until(monitor, "task-capacity", "task-capacity ok 2 16 14", proc)

            say("typing ps (after kill) ...")
            start = send_command_until(monitor, "ps", "Total:", proc)
            if "user  sleeper" in log_text()[start:]:
                raise RuntimeError("renamed child still listed in ps after kill %s" % idle_pid)

            say("typing spawn waitchild ...")
            spawn_start = send_command_until(monitor, "spawn waitchild", "spawn ok pid", proc)
            wait_pid = parse_spawn_pid(log_text()[spawn_start:], "waitchild")
            say("spawned waitchild pid %s" % wait_pid)

            say("typing task-metrics 1 (one child) ...")
            send_command_until(monitor, "task-metrics 1", "Enfants directs : 1", proc)

            say("typing wait-result %s ..." % wait_pid)
            wait_start = send_command_until(monitor, "wait-result %s" % wait_pid,
                                            "wait-result ok %s 0 1" % wait_pid, proc)
            wait_for(proc, "wait-child done", CMD_TIMEOUT, wait_start)

            say("typing ipc-recv (exited event) ...")
            send_command_until(monitor, "ipc-recv",
                               "task-event child %s reason exited" % wait_pid, proc)

            say("typing child-result %s (exited) ..." % wait_pid)
            send_command_until(monitor, "child-result %s" % wait_pid,
                               "child-result ok %s 0 1" % wait_pid, proc)

            say("typing child-results (history) ...")
            history_start = send_command_until(monitor, "child-results", "child-results ok 2", proc)
            wait_for(proc, "child-result-entry %s -128 2" % idle_pid, CMD_TIMEOUT, history_start)
            wait_for(proc, "child-result-entry %s 0 1" % wait_pid, CMD_TIMEOUT, history_start)

            say("typing child-results-observe 2 (stale) ...")
            send_command_until(monitor, "child-results-observe 2", "child-results-observe stale 3", proc)

            say("typing child-results-observe 3 (fresh) ...")
            observe_start = send_command_until(monitor, "child-results-observe 3",
                                               "child-results-observe ok 3 2", proc)
            wait_for(proc, "child-result-entry %s -128 2" % idle_pid, CMD_TIMEOUT, observe_start)
            wait_for(proc, "child-result-entry %s 0 1" % wait_pid, CMD_TIMEOUT, observe_start)

            say("typing child-results-clear ...")
            send_command_until(monitor, "child-results-clear", "child-results-clear ok 4", proc)
            say("typing child-results (empty) ...")
            send_command_until(monitor, "child-results", "child-results ok 0", proc)

            say("typing task-metrics 1 (no child) ...")
            send_command_until(monitor, "task-metrics 1", "Enfants directs : 0", proc)

        say("QEMU spawn/yield/wait smoke passed.")
        return 0
    finally:
        if monitor is not None:
            monitor.close()
        terminate(proc)
        try:
            os.remove(MON_SOCK)
        except OSError:
            pass


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print("QEMU spawn smoke failed: %s" % error, file=sys.stderr)
        raise SystemExit(1)
