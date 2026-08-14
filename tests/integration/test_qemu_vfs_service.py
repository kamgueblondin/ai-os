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
DISK = os.path.join(LOG_DIR, "vfs-service-overlay.img")


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
    special = {" ": "spc", "-": "minus", ".": "dot", "/": "slash"}
    for char in command:
        client.sendall(("sendkey %s\n" % special.get(char, char.lower())).encode("ascii"))
        time.sleep(0.55)
    client.sendall(b"sendkey ret\n")


def main():
    os.makedirs(LOG_DIR, exist_ok=True)
    for path in (LOG, ERR, MON, DISK):
        try:
            os.remove(path)
        except OSError:
            pass
    with open(DISK, "wb") as disk_handle:
        disk_handle.truncate(64 * 512)
    command = [
        "qemu-system-i386", "-kernel", KERNEL, "-initrd", INITRD,
        "-m", "1024M", "-display", "none", "-vga", "none",
        "-serial", "file:" + LOG,
        "-monitor", "unix:%s,server,nowait" % MON,
        "-machine", "type=pc,accel=tcg", "-no-reboot", "-no-shutdown",
        "-drive", "file=%s,format=raw,if=ide,cache=writethrough" % DISK,
    ]
    with open(ERR, "wb") as err_handle:
        proc = subprocess.Popen(command, stdout=err_handle, stderr=err_handle)
        monitor = None
        try:
            wait_for("(-.-)", proc)
            monitor = connect_monitor()
            time.sleep(0.5)
            before_probe = len(log_text())
            send_command(monitor, "vfs-backend-probe hello.txt")
            wait_for("vfs-backend-probe denied", proc, before_probe)
            before_spawn = len(log_text())
            send_command(monitor, "spawn vfsserver")
            wait_for("spawn ok pid", proc, before_spawn)
            spawned = re.search(r"spawn ok pid (\d+) vfsserver", log_text()[before_spawn:])
            if not spawned:
                raise RuntimeError("serveur VFS non lance")
            server_pid = spawned.group(1)
            wait_for("vfsserver ready vfs", proc, before_spawn)
            wait_for("vfsserver mount initrd/", proc, before_spawn)
            wait_for("vfsserver mount overlay/ rw", proc, before_spawn)
            before_initial_stats = len(log_text())
            send_command(monitor, "vfs-stats")
            wait_for("vfsserver virtual vfs-stats", proc, before_initial_stats)
            wait_for("reads=1", proc, before_initial_stats)
            wait_for("writes=0", proc, before_initial_stats)
            wait_for("removes=0", proc, before_initial_stats)
            wait_for("renames=0", proc, before_initial_stats)
            before_direct_write = len(log_text())
            send_command(monitor, "vfs-backend-write-probe note.txt denied")
            wait_for("vfs-backend-write-probe denied", proc, before_direct_write)
            before_direct_remove = len(log_text())
            send_command(monitor, "vfs-backend-remove-probe note.txt")
            wait_for("vfs-backend-remove-probe denied", proc, before_direct_remove)
            before_direct_rename = len(log_text())
            send_command(monitor, "vfs-backend-rename-probe note.txt moved.txt")
            wait_for("vfs-backend-rename-probe denied", proc, before_direct_rename)
            before_mounts = len(log_text())
            send_command(monitor, "vfs-read vfs-mounts")
            wait_for("vfsserver virtual vfs-mounts", proc, before_mounts)
            wait_for("vfs-read ok", proc, before_mounts)
            wait_for("initrd/", proc, before_mounts)
            before_outside = len(log_text())
            send_command(monitor, "vfs-read hello.txt")
            wait_for("vfsserver path outside mounts", proc, before_outside)
            wait_for("vfs-read: chemin hors montage", proc, before_outside)
            before_pending = len(log_text())
            send_command(monitor, "ipc-send 1 deferred")
            wait_for("ipc-send ok", proc, before_pending)
            before_read = len(log_text())
            send_command(monitor, "vfs-read initrd/hello.txt")
            wait_for("vfs-read ok", proc, before_read)
            wait_for("request 4 data", proc, before_read)
            wait_for("Un autre fichier de demonstration.", proc, before_read)
            before_receive = len(log_text())
            send_command(monitor, "ipc-recv")
            wait_for("ipc-recv from 1", proc, before_receive)
            wait_for("type 0 data deferred", proc, before_receive)
            before_virtual = len(log_text())
            send_command(monitor, "vfs-read vfs-info")
            wait_for("vfsserver virtual vfs-info", proc, before_virtual)
            wait_for("vfsserver ring3 policy", proc, before_virtual)
            before_readonly_write = len(log_text())
            send_command(monitor, "vfs-write initrd/no.txt denied")
            wait_for("vfsserver write outside mounts", proc, before_readonly_write)
            wait_for("vfs-write: chemin hors montage ecriture", proc, before_readonly_write)
            before_readonly_remove = len(log_text())
            send_command(monitor, "vfs-remove initrd/no.txt")
            wait_for("vfsserver remove outside mounts", proc, before_readonly_remove)
            wait_for("vfs-remove: chemin hors montage ecriture", proc, before_readonly_remove)
            before_readonly_rename = len(log_text())
            send_command(monitor, "vfs-rename initrd/no.txt overlay/moved.txt")
            wait_for("vfsserver rename outside mounts", proc, before_readonly_rename)
            wait_for("vfs-rename: chemins hors montage ecriture", proc, before_readonly_rename)
            before_write = len(log_text())
            send_command(monitor, "vfs-write overlay/note.txt vfsok")
            wait_for("vfsserver write request", proc, before_write)
            wait_for("vfs-write ok request", proc, before_write)
            before_written_read = len(log_text())
            send_command(monitor, "vfs-read overlay/note.txt")
            wait_for("vfs-read ok", proc, before_written_read)
            wait_for("vfsok", proc, before_written_read)
            before_rename = len(log_text())
            send_command(monitor, "vfs-rename overlay/note.txt overlay/moved.txt")
            wait_for("vfsserver rename request", proc, before_rename)
            wait_for("vfs-rename ok request", proc, before_rename)
            before_old_read = len(log_text())
            send_command(monitor, "vfs-read overlay/note.txt")
            wait_for("vfs-read: lecture refusee ou fichier absent", proc, before_old_read)
            before_renamed_read = len(log_text())
            send_command(monitor, "vfs-read overlay/moved.txt")
            wait_for("vfs-read ok", proc, before_renamed_read)
            wait_for("vfsok", proc, before_renamed_read)
            before_remove = len(log_text())
            send_command(monitor, "vfs-remove overlay/moved.txt")
            wait_for("vfsserver remove request", proc, before_remove)
            wait_for("vfs-remove ok request", proc, before_remove)
            before_removed_read = len(log_text())
            send_command(monitor, "vfs-read overlay/moved.txt")
            wait_for("vfs-read: lecture refusee ou fichier absent", proc, before_removed_read)
            before_final_stats = len(log_text())
            send_command(monitor, "vfs-stats")
            wait_for("reads=10", proc, before_final_stats)
            wait_for("writes=2", proc, before_final_stats)
            wait_for("removes=2", proc, before_final_stats)
            wait_for("renames=2", proc, before_final_stats)
            before_claim = len(log_text())
            send_command(monitor, "spawn vfsclaim")
            wait_for("spawn ok pid", proc, before_claim)
            claimed = re.search(r"spawn ok pid (\d+) vfsclaim", log_text()[before_claim:])
            if not claimed:
                raise RuntimeError("beneficiaire VFS non lance")
            claim_pid = claimed.group(1)
            wait_for("vfsclaim waiting vfs", proc, before_claim)
            before_grant = len(log_text())
            send_command(monitor, "vfs-grant %s" % claim_pid)
            wait_for("vfs-grant sent %s" % claim_pid, proc, before_grant)
            before_handoff = len(log_text())
            send_command(monitor, "yield")
            wait_for("yield ok", proc, before_handoff)
            wait_for("vfsserver grant vfs %s" % claim_pid, proc, before_grant)
            wait_for("vfsclaim backend granted", proc, before_grant)
            before_old_kill = len(log_text())
            send_command(monitor, "kill %s" % server_pid)
            wait_for("Processus %s termine" % server_pid, proc, before_old_kill)
            before_lookup = len(log_text())
            send_command(monitor, "service-find vfs")
            wait_for("service-find ok vfs %s" % claim_pid, proc, before_lookup)
            before_revoked_read = len(log_text())
            send_command(monitor, "vfs-read initrd/hello.txt")
            wait_for("vfs-read: reponse VFS absente ou invalide", proc, before_revoked_read)
            before_new_kill = len(log_text())
            send_command(monitor, "kill %s" % claim_pid)
            wait_for("Processus %s termine" % claim_pid, proc, before_new_kill)
            before_missing = len(log_text())
            send_command(monitor, "vfs-read initrd/hello.txt")
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
            for path in (MON, DISK):
                try:
                    os.remove(path)
                except OSError:
                    pass


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print("MOHHOS Foundation VFS service contract failed: %s" % error, file=sys.stderr)
        raise SystemExit(1)
