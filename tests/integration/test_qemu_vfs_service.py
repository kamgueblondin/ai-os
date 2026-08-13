#!/usr/bin/env python3
"""Contrat MOHHOS Foundation : lecture VFS par médiateur Ring 3."""
import os
import re
import socket
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LOG_DIR = os.path.join(ROOT, "test_logs")
LOG = os.path.join(LOG_DIR, "vfs-service.log")
ERR = os.path.join(LOG_DIR, "vfs-service.err")
MON = os.path.join(LOG_DIR, "vfs-service-monitor.sock")
KERNEL = os.path.join(ROOT, "build", "ai_os.bin")
INITRD = os.path.join(ROOT, "my_initrd.tar")


def log_text():
    try:
        with open(LOG, "r", errors="replace") as handle:
            return handle.read()
    except OSError:
        return ""


def wait_for(needle, proc, offset=0, timeout=15):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError("QEMU s'est arrêté prématurément")
        if needle in log_text()[offset:]:
            return
        time.sleep(0.1)
    raise RuntimeError("sortie manquante : %s" % needle)


def connect_monitor():
    deadline = time.time() + 5
    while time.time() < deadline:
        if os.path.exists(MON):
            client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                client.connect(MON)
                client.settimeout(0.1)
                try:
                    client.recv(4096)
                except socket.timeout:
                    pass
                return client
            except OSError:
                client.close()
        time.sleep(0.1)
    raise RuntimeError("moniteur QEMU indisponible")


def send_command(client, command):
    special = {" ": "spc", "-": "minus", ".": "dot"}
    for char in command:
        client.sendall(("sendkey %s\n" % special.get(char, char.lower())).encode("ascii"))
        time.sleep(0.06)
    client.sendall(b"sendkey ret\n")


def main():
    os.makedirs(LOG_DIR, exist_ok=True)
    for path in (LOG, ERR, MON):
        try:
            os.remove(path)
        except OSError:
            pass
    command = [
        "qemu-system-i386", "-kernel", KERNEL, "-initrd", INITRD,
        "-m", "1024M", "-display", "none", "-vga", "none",
        "-serial", "file:" + LOG,
        "-monitor", "unix:%s,server,nowait" % MON,
        "-machine", "type=pc,accel=tcg", "-no-reboot", "-no-shutdown",
    ]
    with open(ERR, "wb") as err_handle:
        proc = subprocess.Popen(command, stdout=err_handle, stderr=err_handle)
        monitor = None
        try:
            wait_for("(-.-)", proc)
            monitor = connect_monitor()
            time.sleep(0.5)
            before_spawn = len(log_text())
            send_command(monitor, "spawn vfsserver")
            wait_for("spawn ok pid", proc, before_spawn)
            spawned = re.search(r"spawn ok pid (\d+) vfsserver", log_text()[before_spawn:])
            if not spawned:
                raise RuntimeError("serveur VFS non lance")
            server_pid = spawned.group(1)
            wait_for("vfsserver ready vfs", proc, before_spawn)
            before_pending = len(log_text())
            send_command(monitor, "ipc-send 1 deferred")
            wait_for("ipc-send ok 1 8", proc, before_pending)
            before_read = len(log_text())
            send_command(monitor, "vfs-read hello.txt")
            wait_for("vfs-read ok", proc, before_read)
            wait_for("request 1 data", proc, before_read)
            wait_for("Un autre fichier de demonstration.", proc, before_read)
            before_receive = len(log_text())
            send_command(monitor, "ipc-recv")
            wait_for("ipc-recv from 1", proc, before_receive)
            wait_for("type 0 data deferred", proc, before_receive)
            before_kill = len(log_text())
            send_command(monitor, "kill %s" % server_pid)
            wait_for("Processus %s termine" % server_pid, proc, before_kill)
            before_missing = len(log_text())
            send_command(monitor, "vfs-read hello.txt")
            wait_for("vfs-read: service vfs indisponible", proc, before_missing)
            before_return = len(log_text())
            send_command(monitor, "rc")
            wait_for("rc ok 0", proc, before_return)
            print("MOHHOS Foundation VFS service contract passed")
            return 0
        finally:
            if monitor is not None:
                monitor.close()
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
            try:
                os.remove(MON)
            except OSError:
                pass


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print("MOHHOS Foundation VFS service contract failed: %s" % error, file=sys.stderr)
        raise SystemExit(1)
