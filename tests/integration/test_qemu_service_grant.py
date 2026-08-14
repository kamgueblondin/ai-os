#!/usr/bin/env python3
"""Contrat MOHHOS Foundation : transfert borné d'un nom de service."""
import os
import re
import socket
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LOG_DIR = os.path.join(ROOT, "test_logs")
LOG = os.path.join(LOG_DIR, "service-grant.log")
ERR = os.path.join(LOG_DIR, "service-grant.err")
MON = os.path.join(LOG_DIR, "service-grant-monitor.sock")
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
        time.sleep(0.20)
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
            before_publish = len(log_text())
            send_command(monitor, "service-publish demo")
            wait_for("service-publish ok demo", proc, before_publish)
            before_capacity_first = len(log_text())
            send_command(monitor, "ipc-send 1 one")
            wait_for("ipc-send ok 1 3", proc, before_capacity_first)
            before_capacity_second = len(log_text())
            send_command(monitor, "ipc-send 1 two")
            wait_for("ipc-send ok 1 3", proc, before_capacity_second)
            before_capacity_full = len(log_text())
            send_command(monitor, "ipc-send 1 three")
            wait_for("ipc-send: capacite du service atteinte", proc, before_capacity_full)
            before_capacity_drain_one = len(log_text())
            send_command(monitor, "ipc-recv")
            wait_for("ipc-recv from 1 type 0 data one", proc, before_capacity_drain_one)
            before_capacity_drain_two = len(log_text())
            send_command(monitor, "ipc-recv")
            wait_for("ipc-recv from 1 type 0 data two", proc, before_capacity_drain_two)
            before_watch = len(log_text())
            send_command(monitor, "service-watch demo")
            wait_for("service-watch ok demo", proc, before_watch)
            before_spawn = len(log_text())
            send_command(monitor, "spawn serviceclaim")
            wait_for("spawn ok pid", proc, before_spawn)
            spawned = re.search(r"spawn ok pid (\d+) serviceclaim", log_text()[before_spawn:])
            if not spawned:
                raise RuntimeError("client de revendication non lance")
            claimant_pid = spawned.group(1)
            wait_for("serviceclaim waiting demo", proc, before_spawn)
            before_grant = len(log_text())
            send_command(monitor, "service-grant demo %s" % claimant_pid)
            wait_for("service-grant ok demo %s" % claimant_pid, proc, before_grant)
            send_command(monitor, "yield")
            wait_for("serviceclaim notified demo", proc, before_grant)
            wait_for("serviceclaim claimed demo", proc, before_grant)
            before_grant_event = len(log_text())
            send_command(monitor, "ipc-recv")
            wait_for("service-event demo old 1 new %s reason 2" % claimant_pid,
                     proc, before_grant_event)
            before_lookup = len(log_text())
            send_command(monitor, "service-find demo")
            wait_for("service-find ok demo %s" % claimant_pid, proc, before_lookup)
            before_kill = len(log_text())
            send_command(monitor, "kill %s" % claimant_pid)
            wait_for("Processus %s termine" % claimant_pid, proc, before_kill)
            before_purge_event = len(log_text())
            send_command(monitor, "ipc-recv")
            wait_for("service-event demo old %s new 0 reason 4" % claimant_pid,
                     proc, before_purge_event)
            before_missing = len(log_text())
            send_command(monitor, "service-find demo")
            wait_for("service-find: service indisponible", proc, before_missing)
            print("MOHHOS Foundation service grant contract passed")
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
        print("MOHHOS Foundation service grant contract failed: %s" % error, file=sys.stderr)
        raise SystemExit(1)
