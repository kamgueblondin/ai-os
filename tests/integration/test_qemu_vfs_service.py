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
FAT32_DISK = os.path.join(LOG_DIR, "vfs-service-fat32.img")
KEY_DELAY = float(os.environ.get("KEY_DELAY", "0.24"))
KEY_RETRIES = int(os.environ.get("KEY_RETRIES", "3"))


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
        # Les diagnostics timer asynchrones peuvent couper une ligne applicative
        # sans modifier le protocole VFS ; ils ne font pas partie du contrat testé.
        output = re.sub(r"TIMER_ALIVE: tick=[^\\n]*\\n", "", log_text()[offset:])
        if needle in output:
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


def send_command_once(client, command):
    special = {" ": "spc", "-": "minus", ".": "dot", "/": "slash"}
    for char in command:
        client.sendall(("sendkey %s\n" % special.get(char, char.lower())).encode("ascii"))
        time.sleep(KEY_DELAY)
    client.sendall(b"sendkey ret\n")


def send_command(client, command, proc=None):
    """Send a complete command and, when possible, validate its guest echo.

    QEMU TCG can occasionally duplicate a PS/2 scan-code. The echo guard
    retries only the injection; business assertions remain handled by callers.
    """
    if proc is None:
        send_command_once(client, command)
        return
    echo = "SYS_GETS: ligne lue: " + command
    for attempt in range(1, KEY_RETRIES + 1):
        start = len(log_text())
        send_command_once(client, command)
        deadline = time.time() + 15
        while time.time() < deadline:
            if proc.poll() is not None:
                raise RuntimeError("QEMU s'est arrêté prématurément")
            output = log_text()[start:]
            if echo in output:
                return
            if "SYS_GETS: ligne lue: " in output:
                break
            time.sleep(0.1)
        if attempt < KEY_RETRIES:
            time.sleep(0.2)
    raise RuntimeError("echo commande instable : %s" % command)


def send_command_until(client, command, needle, proc, attempts=3):
    error = None
    for _ in range(attempts):
        start = len(log_text())
        try:
            send_command(client, command, proc)
            wait_for(needle, proc, start)
            return
        except RuntimeError as caught:
            error = caught
            time.sleep(0.2)
    raise error


def main():
    os.makedirs(LOG_DIR, exist_ok=True)
    for path in (LOG, ERR, MON, DISK, FAT32_DISK):
        try:
            os.remove(path)
        except OSError:
            pass
    subprocess.check_call([
        sys.executable,
        os.path.join(ROOT, "tests", "scripts", "make_fat16_image.py"),
        "--image", DISK,
    ])
    subprocess.check_call([
        sys.executable,
        os.path.join(ROOT, "scripts", "make_fat32_secondary_image.py"),
        "--image", FAT32_DISK,
    ])
    command = [
        "qemu-system-i386", "-cpu", "pentium3", "-kernel", KERNEL, "-initrd", INITRD,
        "-m", "1024M", "-display", "none", "-vga", "none",
        "-serial", "file:" + LOG,
        "-monitor", "unix:%s,server,nowait" % MON,
        "-machine", "type=pc,accel=tcg", "-no-reboot", "-no-shutdown",
        "-drive", "file=%s,format=raw,if=ide,index=0,cache=writethrough" % DISK,
        "-drive", "file=%s,format=raw,if=ide,index=1,cache=writethrough" % FAT32_DISK,
    ]
    with open(ERR, "wb") as err_handle:
        proc = subprocess.Popen(command, stdout=err_handle, stderr=err_handle)
        monitor = None
        try:
            wait_for("(-.-)", proc)
            monitor = connect_monitor()
            time.sleep(0.5)
            before_probe = len(log_text())
            send_command_until(monitor, "vfs-backend-probe hello.txt",
                               "vfs-backend-probe denied", proc)
            before_worker_spawn = len(log_text())
            send_command_until(monitor, "spawn vfsvirtual", "spawn ok pid", proc)
            worker_spawned = re.search(r"spawn ok pid[\s\S]*?(\d+) vfsvirtual", log_text()[before_worker_spawn:])
            if not worker_spawned:
                raise RuntimeError("worker virtuel VFS non lance")
            worker_pid = worker_spawned.group(1)
            send_command(monitor, "yield", proc)
            wait_for("vfsvirtual ready", proc, before_worker_spawn)
            send_command_until(monitor, "service-find vfs-virtual",
                               "service-find ok vfs-virtual %s" % worker_pid, proc)
            before_spawn = len(log_text())
            send_command_until(monitor, "spawn vfsserver", "spawn ok pid", proc)
            spawned = re.search(r"spawn ok pid[\s\S]*?(\d+) vfsserver", log_text()[before_spawn:])
            if not spawned:
                raise RuntimeError("serveur VFS non lance")
            server_pid = spawned.group(1)
            send_command(monitor, "yield", proc)
            wait_for("vfsserver ready vfs", proc, before_spawn)
            wait_for("vfsserver mount initrd/", proc, before_spawn)
            wait_for("vfsserver mount overlay/ rw", proc, before_spawn)
            before_cap_claim = len(log_text())
            send_command_until(monitor, "spawn vfscapclaim", "spawn ok pid", proc)
            cap_claimed = re.search(r"spawn ok pid[\s\S]*?(\d+) vfscapclaim", log_text()[before_cap_claim:])
            if not cap_claimed:
                raise RuntimeError("beneficiaire de capacite VFS non lance")
            cap_claim_pid = cap_claimed.group(1)
            send_command(monitor, "yield", proc)
            wait_for("vfscapclaim waiting backend", proc, before_cap_claim)
            before_cap_grant = len(log_text())
            send_command_until(monitor, "vfs-backend-grant %s" % cap_claim_pid,
                               "vfsserver backend grant request", proc)
            wait_for("vfs-backend-grant ok request", proc, before_cap_grant)
            wait_for("vfscapclaim backend granted", proc, before_cap_grant)
            before_full_status = len(log_text())
            send_command_until(monitor, "vfs-backend-status %s" % cap_claim_pid,
                               "vfsserver backend status request", proc)
            wait_for("vfs-backend-status ok rights full", proc, before_full_status)
            before_full_list = len(log_text())
            send_command_until(monitor, "vfs-backend-list", "vfsserver backend list request", proc)
            wait_for("vfs-backend-list ok count 1", proc, before_full_list)
            wait_for("vfs-backend-list pid %s rights full" % cap_claim_pid, proc, before_full_list)
            before_observe_current = len(log_text())
            send_command_until(monitor, "vfs-backend-observe 0", "vfsserver backend observe request", proc)
            wait_for("vfs-backend-observe ok generation 2 count 1", proc, before_observe_current)
            before_owner_after_cap = len(log_text())
            send_command_until(monitor, "service-find vfs", "service-find ok vfs %s" % server_pid, proc)
            before_cap_revoke = len(log_text())
            send_command_until(monitor, "vfs-backend-revoke %s" % cap_claim_pid,
                               "vfsserver backend revoke request", proc)
            wait_for("vfs-backend-revoke ok request", proc, before_cap_revoke)
            wait_for("vfscapclaim backend revoked", proc, before_cap_revoke)
            before_absent_status = len(log_text())
            send_command_until(monitor, "vfs-backend-status %s" % cap_claim_pid,
                               "vfsserver backend status request", proc)
            wait_for("vfs-backend-status: capacite absente ou refusee", proc, before_absent_status)
            before_empty_list = len(log_text())
            send_command_until(monitor, "vfs-backend-list", "vfsserver backend list request", proc)
            wait_for("vfs-backend-list ok count 0", proc, before_empty_list)
            before_observe_stale = len(log_text())
            send_command_until(monitor, "vfs-backend-observe 2", "vfsserver backend observe request", proc)
            wait_for("vfs-backend-observe stale generation 3", proc, before_observe_stale)
            send_command_until(monitor, "kill %s" % cap_claim_pid,
                               "Processus %s termine" % cap_claim_pid, proc)
            send_command_until(monitor, "service-find vfs", "service-find ok vfs %s" % server_pid, proc)
            before_release_claim = len(log_text())
            send_command_until(monitor, "spawn vfsreleaseclaim", "spawn ok pid", proc)
            release_claimed = re.search(r"spawn ok pid[\s\S]*?(\d+) vfsreleaseclaim", log_text()[before_release_claim:])
            if not release_claimed:
                raise RuntimeError("beneficiaire de liberation autonome non lance")
            release_claim_pid = release_claimed.group(1)
            send_command(monitor, "yield", proc)
            wait_for("vfsreleaseclaim waiting backend", proc, before_release_claim)
            before_release_grant = len(log_text())
            send_command_until(monitor, "vfs-backend-grant %s" % release_claim_pid,
                               "vfsserver backend grant request", proc)
            wait_for("vfs-backend-grant ok request", proc, before_release_grant)
            wait_for("vfsreleaseclaim backend granted", proc, before_release_grant)
            wait_for("vfsreleaseclaim backend self-released", proc, before_release_grant)
            before_release_status = len(log_text())
            send_command_until(monitor, "vfs-backend-status %s" % release_claim_pid,
                               "vfsserver backend status request", proc)
            wait_for("vfs-backend-status: capacite absente ou refusee", proc, before_release_status)
            # Le bénéficiaire reste coopératif après sa libération. Le terminer ici
            # ajouterait un événement enfant au endpoint shell déjà sollicité par le
            # scénario IPC différé qui suit et masquerait la réponse VFS concernée.
            before_read_claim = len(log_text())
            send_command_until(monitor, "spawn vfsreadclaim", "spawn ok pid", proc)
            read_claimed = re.search(r"spawn ok pid[\s\S]*?(\d+) vfsreadclaim", log_text()[before_read_claim:])
            if not read_claimed:
                raise RuntimeError("beneficiaire lecture seule VFS non lance")
            read_claim_pid = read_claimed.group(1)
            send_command(monitor, "yield", proc)
            wait_for("vfsreadclaim waiting read", proc, before_read_claim)
            before_read_grant = len(log_text())
            send_command_until(monitor, "vfs-backend-grant-read %s" % read_claim_pid,
                               "vfsserver backend scoped grant request", proc)
            wait_for("vfs-backend-grant-read ok request", proc, before_read_grant)
            wait_for("vfsreadclaim read-only enforced", proc, before_read_grant)
            before_read_status = len(log_text())
            send_command_until(monitor, "vfs-backend-status %s" % read_claim_pid,
                               "vfsserver backend status request", proc)
            wait_for("vfs-backend-status ok rights read", proc, before_read_status)
            before_read_list = len(log_text())
            send_command_until(monitor, "vfs-backend-list", "vfsserver backend list request", proc)
            wait_for("vfs-backend-list ok count 1", proc, before_read_list)
            wait_for("vfs-backend-list pid %s rights read" % read_claim_pid, proc, before_read_list)
            send_command_until(monitor, "kill %s" % read_claim_pid,
                               "Processus %s termine" % read_claim_pid, proc)
            send_command_until(monitor, "service-find vfs", "service-find ok vfs %s" % server_pid, proc)
            before_mutate_claim = len(log_text())
            send_command_until(monitor, "spawn vfsmutateclaim", "spawn ok pid", proc)
            mutate_claimed = re.search(r"spawn ok pid[\s\S]*?(\d+) vfsmutateclaim", log_text()[before_mutate_claim:])
            if not mutate_claimed:
                raise RuntimeError("beneficiaire mutation seule VFS non lance")
            mutate_claim_pid = mutate_claimed.group(1)
            send_command(monitor, "yield", proc)
            wait_for("vfsmutateclaim waiting mutate", proc, before_mutate_claim)
            before_mutate_grant = len(log_text())
            send_command_until(monitor, "vfs-backend-grant-mutate %s" % mutate_claim_pid,
                               "vfsserver backend scoped grant request", proc)
            wait_for("vfs-backend-grant-mutate ok request", proc, before_mutate_grant)
            wait_for("vfsmutateclaim mutate-only enforced", proc, before_mutate_grant)
            before_mutate_status = len(log_text())
            send_command_until(monitor, "vfs-backend-status %s" % mutate_claim_pid,
                               "vfsserver backend status request", proc)
            wait_for("vfs-backend-status ok rights mutate", proc, before_mutate_status)
            before_mutate_list = len(log_text())
            send_command_until(monitor, "vfs-backend-list", "vfsserver backend list request", proc)
            wait_for("vfs-backend-list ok count 1", proc, before_mutate_list)
            wait_for("vfs-backend-list pid %s rights mutate" % mutate_claim_pid, proc, before_mutate_list)
            send_command_until(monitor, "kill %s" % mutate_claim_pid,
                               "Processus %s termine" % mutate_claim_pid, proc)
            send_command_until(monitor, "service-find vfs", "service-find ok vfs %s" % server_pid, proc)
            before_initrd_list = len(log_text())
            send_command_until(monitor, "vfs-list initrd/", "vfsserver list request", proc)
            wait_for("vfs-list partiel count 4", proc, before_initrd_list)
            wait_for("hello.txt", proc, before_initrd_list)
            before_page_zero = len(log_text())
            send_command_until(monitor, "vfs-list-page initrd/ 0", "vfsserver list page request", proc)
            wait_for("vfs-list-page partiel count 4 next 4", proc, before_page_zero)
            before_page_last = len(log_text())
            send_command_until(monitor, "vfs-list-page initrd/ 4", "vfsserver list page request", proc)
            wait_for("vfs-list-page ok count 4 next end", proc, before_page_last)
            before_observe = len(log_text())
            send_command_until(monitor, "vfs-list-observe initrd/ 0 0", "vfsserver list observe request", proc)
            wait_for("vfs-list-observe partiel count 4 next 4 generation 1", proc, before_observe)
            before_initrd_subdir_list = len(log_text())
            send_command_until(monitor, "vfs-list initrd/bin/", "vfsserver list request", proc)
            wait_for("vfs-list partiel count 4", proc, before_initrd_subdir_list)
            wait_for("shell", proc, before_initrd_subdir_list)
            before_overlay_empty_list = len(log_text())
            send_command_until(monitor, "vfs-list overlay/", "vfsserver list request", proc)
            wait_for("vfs-list ok count 0", proc, before_overlay_empty_list)
            before_mkdir = len(log_text())
            send_command_until(monitor, "vfs-mkdir overlay/newdir", "vfsserver mkdir request", proc)
            wait_for("vfs-mkdir ok request", proc, before_mkdir)
            before_overlay_dir_list = len(log_text())
            send_command_until(monitor, "vfs-list overlay/", "vfsserver list request", proc)
            wait_for("newdir", proc, before_overlay_dir_list)
            before_rmdir = len(log_text())
            send_command_until(monitor, "vfs-rmdir overlay/newdir", "vfs-rmdir ok request", proc)
            before_overlay_after_rmdir = len(log_text())
            send_command_until(monitor, "vfs-list overlay/", "vfsserver list request", proc)
            wait_for("vfs-list ok count 0", proc, before_overlay_after_rmdir)
            before_missing_list = len(log_text())
            send_command_until(monitor, "vfs-list missing/", "vfsserver list outside mounts", proc)
            wait_for("vfs-list: repertoire hors montage", proc, before_missing_list)
            before_initial_stats = len(log_text())
            send_command_until(monitor, "vfs-stats", "vfsserver delegated vfs-stats", proc)
            wait_for("vfsvirtual format stats", proc, before_initial_stats)
            wait_for("reads=1", proc, before_initial_stats)
            wait_for("writes=0", proc, before_initial_stats)
            wait_for("removes=0", proc, before_initial_stats)
            wait_for("renames=0", proc, before_initial_stats)
            before_direct_write = len(log_text())
            send_command_until(monitor, "vfs-backend-write-probe note.txt denied",
                               "vfs-backend-write-probe denied", proc)
            send_command_until(monitor, "vfs-backend-remove-probe note.txt",
                               "vfs-backend-remove-probe denied", proc)
            before_direct_rename = len(log_text())
            send_command_until(monitor, "vfs-backend-rename-probe note.txt moved.txt",
                               "vfs-backend-rename-probe denied", proc)
            before_add_assets = len(log_text())
            send_command_until(monitor, "vfs-mount-add assets/ initrd",
                               "vfsserver mount added assets/ initrd", proc)
            wait_for("vfs-mount-add ok request", proc, before_add_assets)
            before_add_work = len(log_text())
            send_command_until(monitor, "vfs-mount-add work/ overlay",
                               "vfsserver mount added work/ overlay", proc)
            wait_for("vfs-mount-add ok request", proc, before_add_work)
            before_add_fat32 = len(log_text())
            send_command_until(monitor, "vfs-mount-add media32/ fat32",
                               "vfsserver mount added media32/ fat32", proc)
            wait_for("vfs-mount-add ok request", proc, before_add_fat32)
            before_add_fat16 = len(log_text())
            send_command_until(monitor, "vfs-mount-add media/ fat16",
                               "vfsserver mount added media/ fat16", proc)
            wait_for("vfs-mount-add ok request", proc, before_add_fat16)
            before_full = len(log_text())
            send_command_until(monitor, "vfs-mount-add full/ overlay",
                               "vfsserver mount add rc -62", proc)
            wait_for("vfs-mount-add: table de montages pleine", proc, before_full)
            before_protected = len(log_text())
            send_command_until(monitor, "vfs-mount-remove initrd/",
                               "vfsserver mount remove rc -60", proc)
            wait_for("vfs-mount-remove: montage protege ou refuse", proc, before_protected)
            before_mounts = len(log_text())
            send_command_until(monitor, "vfs-read vfs-mounts", "vfsserver delegated vfs-mounts", proc)
            wait_for("vfs-read ok", proc, before_mounts)
            wait_for("initrd/", proc, before_mounts)
            wait_for("assets/ ro", proc, before_mounts)
            wait_for("work/ rw", proc, before_mounts)
            wait_for("media32/ ro", proc, before_mounts)
            before_mount_page_zero = len(log_text())
            send_command_until(monitor, "vfs-list-page vfs-mounts 0", "vfsserver list page request", proc)
            wait_for("vfs-list-page partiel count 4 next 4", proc, before_mount_page_zero)
            wait_for("initrd/ ro", proc, before_mount_page_zero)
            before_mount_page_last = len(log_text())
            send_command_until(monitor, "vfs-list-page vfs-mounts 4", "vfsserver list page request", proc)
            wait_for("vfs-list-page ok count 4 next end", proc, before_mount_page_last)
            wait_for("media32/ ro", proc, before_mount_page_last)
            wait_for("media/ ro", proc, before_mount_page_last)
            before_mount_observe = len(log_text())
            send_command_until(monitor, "vfs-list-observe vfs-mounts 0 0", "vfsserver list observe request", proc)
            wait_for("vfs-list-observe partiel count 4 next 4 generation", proc, before_mount_observe)
            wait_for("initrd/ ro", proc, before_mount_observe)
            before_mount_stale = len(log_text())
            send_command_until(monitor, "vfs-list-observe vfs-mounts 0 1", "vfsserver list observe request", proc)
            wait_for("vfs-list-observe obsolete generation", proc, before_mount_stale)
            before_alias_read = len(log_text())
            send_command_until(monitor, "vfs-read assets/hello.txt", "vfs-read ok", proc)
            wait_for("Un autre fichier de demonstration.", proc, before_alias_read)
            before_fat16_list = len(log_text())
            send_command_until(monitor, "vfs-list media/", "vfsserver list request", proc)
            wait_for("vfs-list ok count 2", proc, before_fat16_list)
            wait_for("FATOK.TXT", proc, before_fat16_list)
            before_fat16_read = len(log_text())
            send_command_until(monitor, "vfs-read media/fatok.txt", "vfs-read ok", proc)
            wait_for("FAT16 fixture OK", proc, before_fat16_read)
            before_fat16_stat = len(log_text())
            send_command_until(monitor, "vfs-stat media/fatok.txt",
                               "vfs-stat ok size 17 flags file", proc)
            wait_for("vfs-stat ok size 17 flags file", proc, before_fat16_stat)
            before_fat32_list = len(log_text())
            send_command_until(monitor, "vfs-list media32/", "vfsserver list request", proc)
            wait_for("vfs-list ok count 1", proc, before_fat32_list)
            wait_for("FAT32OK.TXT", proc, before_fat32_list)
            before_fat32_read = len(log_text())
            send_command_until(monitor, "vfs-read media32/fat32ok.txt", "vfs-read ok", proc)
            wait_for("FAT32 secondary fixture OK", proc, before_fat32_read)
            before_fat32_stat = len(log_text())
            send_command_until(monitor, "vfs-stat media32/fat32ok.txt",
                               "vfs-stat ok size 27 flags file", proc)
            wait_for("vfs-stat ok size 27 flags file", proc, before_fat32_stat)
            before_outside = len(log_text())
            send_command_until(monitor, "vfs-read hello.txt",
                               "vfsserver path outside mounts", proc)
            wait_for("vfs-read: chemin hors montage", proc, before_outside)
            before_pending = len(log_text())
            send_command_until(monitor, "ipc-send 1 deferred", "ipc-send ok", proc)
            before_read = len(log_text())
            send_command_until(monitor, "vfs-read initrd/hello.txt", "vfs-read ok", proc)
            wait_for("vfs-read ok 35 request", proc, before_read)
            wait_for("Un autre fichier de demonstration.", proc, before_read)
            before_deferred_list = len(log_text())
            send_command_until(monitor, "vfs-list initrd/", "vfs-list partiel count", proc)
            wait_for("vfs-list partiel count 4", proc, before_deferred_list)
            wait_for("hello.txt", proc, before_deferred_list)
            before_receive = len(log_text())
            send_command_until(monitor, "ipc-recv", "ipc-recv from 1", proc, attempts=6)
            wait_for("type 0", proc, before_receive)
            wait_for("data deferred", proc, before_receive)
            before_virtual = len(log_text())
            send_command_until(monitor, "vfs-read vfs-info", "vfsserver delegated vfs-info", proc)
            wait_for("vfsvirtual read vfs-info", proc, before_virtual)
            wait_for("vfsserver ring3 policy", proc, before_virtual)
            before_virtual_mounts = len(log_text())
            send_command_until(monitor, "vfs-read vfs-mounts", "vfsserver delegated vfs-mounts", proc)
            wait_for("initrd/ ro", proc, before_virtual_mounts)
            wait_for("overlay/ rw", proc, before_virtual_mounts)
            wait_for("fat16/ ro", proc, before_virtual_mounts)
            wait_for("fat32/ ro", proc, before_virtual_mounts)
            if log_text()[before_virtual_mounts:].count("vfsvirtual format mount") < 4:
                raise RuntimeError("formatage sequentiel des montages incomplet")
            before_initrd_stat = len(log_text())
            send_command_until(monitor, "vfs-stat initrd/hello.txt", "vfs-stat ok size 35 flags file", proc)
            wait_for("vfs-stat ok size 35 flags file", proc, before_initrd_stat)
            before_readonly_write = len(log_text())
            send_command_until(monitor, "vfs-write initrd/no.txt denied",
                               "vfsserver write outside mounts", proc)
            wait_for("vfs-write: chemin hors montage ecriture", proc, before_readonly_write)
            before_readonly_remove = len(log_text())
            send_command_until(monitor, "vfs-remove initrd/no.txt",
                               "vfsserver remove outside mounts", proc)
            wait_for("vfs-remove: chemin hors montage ecriture", proc, before_readonly_remove)
            before_readonly_rename = len(log_text())
            send_command_until(monitor, "vfs-rename initrd/no.txt overlay/moved.txt",
                               "vfsserver rename outside mounts", proc)
            wait_for("vfs-rename: chemins hors montage ecriture", proc, before_readonly_rename)
            before_alias_write = len(log_text())
            send_command_until(monitor, "vfs-write work/alias.txt workok",
                               "vfs-write ok request", proc)
            before_alias_list = len(log_text())
            send_command_until(monitor, "vfs-list work/", "vfsserver list request", proc)
            wait_for("vfs-list ok count 1", proc, before_alias_list)
            wait_for("alias.txt", proc, before_alias_list)
            before_alias_written_read = len(log_text())
            send_command_until(monitor, "vfs-read work/alias.txt", "vfs-read ok", proc)
            wait_for("workok", proc, before_alias_written_read)
            before_alias_rename = len(log_text())
            send_command_until(monitor, "vfs-rename work/alias.txt work/renamed.txt",
                               "vfs-rename ok request", proc)
            before_alias_renamed_read = len(log_text())
            send_command_until(monitor, "vfs-read work/renamed.txt", "vfs-read ok", proc)
            wait_for("workok", proc, before_alias_renamed_read)
            before_alias_file_remove = len(log_text())
            send_command_until(monitor, "vfs-remove work/renamed.txt", "vfs-remove ok request", proc)
            before_alias_empty_list = len(log_text())
            send_command_until(monitor, "vfs-list work/", "vfsserver list request", proc)
            wait_for("vfs-list ok count 0", proc, before_alias_empty_list)
            before_alias_remove = len(log_text())
            send_command_until(monitor, "vfs-mount-remove work/",
                               "vfsserver mount removed work/", proc)
            wait_for("vfs-mount-remove ok request", proc, before_alias_remove)
            before_alias_revoked = len(log_text())
            send_command_until(monitor, "vfs-read work/alias.txt",
                               "vfs-read: chemin hors montage", proc)
            before_alias_missing = len(log_text())
            send_command_until(monitor, "vfs-mount-remove work/",
                               "vfs-mount-remove: montage absent", proc)
            before_write = len(log_text())
            send_command_until(monitor, "vfs-write overlay/note.txt vfsok",
                               "vfsserver write request", proc)
            wait_for("vfs-write ok request", proc, before_write)
            before_overlay_list = len(log_text())
            send_command_until(monitor, "vfs-list overlay/", "vfsserver list request", proc)
            wait_for("note.txt", proc, before_overlay_list)
            before_overlay_stat = len(log_text())
            send_command_until(monitor, "vfs-stat overlay/note.txt", "vfsserver stat request", proc)
            wait_for("vfs-stat ok size 5 flags file", proc, before_overlay_stat)
            before_written_read = len(log_text())
            send_command_until(monitor, "vfs-read overlay/note.txt", "vfs-read ok", proc)
            wait_for("vfsok", proc, before_written_read)
            before_rename = len(log_text())
            send_command_until(monitor, "vfs-rename overlay/note.txt overlay/moved.txt",
                               "vfs-rename ok request", proc)
            before_old_read = len(log_text())
            send_command_until(monitor, "vfs-read overlay/note.txt",
                               "vfs-read: lecture refusee ou fichier absent", proc)
            before_renamed_read = len(log_text())
            send_command_until(monitor, "vfs-read overlay/moved.txt", "vfs-read ok", proc)
            wait_for("vfsok", proc, before_renamed_read)
            before_remove = len(log_text())
            send_command_until(monitor, "vfs-remove overlay/moved.txt",
                               "vfs-remove ok request", proc)
            before_removed_read = len(log_text())
            send_command_until(monitor, "vfs-read overlay/moved.txt",
                               "vfs-read: lecture refusee ou fichier absent", proc)
            before_stat_outside = len(log_text())
            send_command_until(monitor, "vfs-stat hello.txt", "vfsserver stat outside mounts", proc)
            wait_for("vfs-stat: chemin hors montage", proc, before_stat_outside)
            before_final_stats = len(log_text())
            send_command_until(monitor, "vfs-stats", "vfsserver delegated vfs-stats", proc)
            wait_for("vfsvirtual format stats", proc, before_final_stats)
            wait_for("reads=1", proc, before_final_stats)
            wait_for("writes=", proc, before_final_stats)
            wait_for("removes=", proc, before_final_stats)
            wait_for("renames=", proc, before_final_stats)
            before_claim = len(log_text())
            send_command_until(monitor, "spawn vfsclaim", "spawn ok pid", proc)
            claimed = re.search(r"spawn ok pid[\s\S]*?(\d+) vfsclaim", log_text()[before_claim:])
            if not claimed:
                raise RuntimeError("beneficiaire VFS non lance")
            claim_pid = claimed.group(1)
            send_command(monitor, "yield", proc)
            wait_for("vfsclaim waiting vfs", proc, before_claim)
            before_grant = len(log_text())
            send_command_until(monitor, "vfs-grant %s" % claim_pid,
                               "vfs-grant sent %s" % claim_pid, proc)
            before_handoff = len(log_text())
            send_command_until(monitor, "yield", "yield ok", proc)
            wait_for("vfsserver grant vfs %s" % claim_pid, proc, before_grant)
            wait_for("vfsclaim backend granted", proc, before_grant)
            before_old_kill = len(log_text())
            send_command_until(monitor, "kill %s" % server_pid,
                               "Processus %s termine" % server_pid, proc)
            before_lookup = len(log_text())
            send_command_until(monitor, "service-find vfs", "service-find ok vfs %s" % claim_pid, proc)
            before_revoked_read = len(log_text())
            send_command_until(monitor, "vfs-read initrd/hello.txt",
                               "vfs-read: reponse VFS absente ou invalide", proc)
            before_new_kill = len(log_text())
            send_command_until(monitor, "kill %s" % claim_pid,
                               "Processus %s termine" % claim_pid, proc)
            before_missing = len(log_text())
            send_command_until(monitor, "vfs-read initrd/hello.txt",
                               "vfs-read: service vfs indisponible", proc)
            before_return = len(log_text())
            send_command_until(monitor, "rc", "rc ok 0", proc)
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
