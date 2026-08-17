#!/usr/bin/env python3
"""QEMU GTK screendumps: shell, help, scrollback, FAT16, ps, mem, net-status."""
from __future__ import print_function

import os
import socket
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
KERNEL = os.path.join(ROOT, "build", "ai_os.bin")
INITRD = os.path.join(ROOT, "my_initrd.tar")
DISK = os.path.join(ROOT, "test_logs", "gui-capture-overlay.img")
LOG_DIR = os.path.join(ROOT, "test_logs")
LOG = os.path.join(LOG_DIR, "gui-capture-serial.log")
ERR = os.path.join(LOG_DIR, "gui-capture-stderr.log")
MON_SOCK = os.path.join(LOG_DIR, "gui-capture-monitor.sock")
SHOT_DIR = "/opt/cursor/artifacts/screenshots"
KEY_DELAY = 0.65
BOOT_TIMEOUT = 90.0


def log_text():
    try:
        with open(LOG, "r", errors="replace") as handle:
            return handle.read()
    except OSError:
        return ""


def wait_for(proc, needle, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise RuntimeError("QEMU stopped; tail:\n%s" % log_text()[-2000:])
        if needle in log_text():
            time.sleep(0.4)
            return
        time.sleep(0.15)
    raise RuntimeError("timeout %r; tail:\n%s" % (needle, log_text()[-2000:]))


def monitor_connect():
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if os.path.exists(MON_SOCK):
            client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                client.connect(MON_SOCK)
                client.settimeout(0.2)
                try:
                    client.recv(4096)
                except socket.timeout:
                    pass
                return client
            except OSError:
                client.close()
        time.sleep(0.1)
    raise RuntimeError("monitor unavailable")


def mon(client, line):
    client.sendall((line + "\n").encode("ascii"))
    time.sleep(0.15)
    try:
        client.recv(4096)
    except socket.timeout:
        pass


def send_command(client, command):
    aliases = {" ": "spc", "-": "minus", ".": "dot", "/": "slash"}
    for char in command:
        mon(client, "sendkey %s" % aliases.get(char, char.lower()))
        time.sleep(KEY_DELAY)
    time.sleep(KEY_DELAY)
    mon(client, "sendkey ret")


def ppm_to_png(path):
    data = open(path, "rb").read()
    if data[:2] != b"P6":
        return
    parts = data.split(b"\n", 3)
    wh = parts[1].split()
    w, h = int(wh[0]), int(wh[1])
    raw = parts[3]
    import struct
    import zlib

    def chunk(tag, payload):
        crc = zlib.crc32(tag + payload) & 0xFFFFFFFF
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)

    rows = b"".join(b"\x00" + raw[y * w * 3:(y + 1) * w * 3] for y in range(h))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as handle:
        handle.write(png)


def screendump(client, name):
    path = os.path.join(SHOT_DIR, name)
    mon(client, "screendump %s" % path)
    time.sleep(0.6)
    if not os.path.isfile(path) or os.path.getsize(path) < 100:
        raise RuntimeError("screendump failed: %s" % path)
    ppm_to_png(path)
    print("saved", path, os.path.getsize(path))


def prepare_disk():
    os.makedirs(LOG_DIR, exist_ok=True)
    subprocess.run([
        sys.executable,
        os.path.join(ROOT, "tests", "scripts", "make_fat16_image.py"),
        "--image", DISK,
    ], check=True)


def terminate(proc):
    if proc is None or proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=4)
    except subprocess.TimeoutExpired:
        proc.kill()


def main():
    os.makedirs(LOG_DIR, exist_ok=True)
    os.makedirs(SHOT_DIR, exist_ok=True)
    prepare_disk()
    for path in (LOG, ERR, MON_SOCK):
        try:
            os.remove(path)
        except OSError:
            pass

    proc = None
    monitor = None
    try:
        with open(ERR, "wb") as err:
            proc = subprocess.Popen([
                "qemu-system-i386", "-cpu", "pentium3",
                "-kernel", KERNEL, "-initrd", INITRD,
                "-m", "1024M", "-vga", "std", "-display", "gtk",
                "-serial", "file:" + LOG,
                "-monitor", "unix:%s,server,nowait" % MON_SOCK,
                "-drive", "file=%s,format=raw,if=ide,cache=writethrough" % DISK,
                "-machine", "type=pc,accel=tcg",
                "-no-reboot", "-no-shutdown",
            ], cwd=ROOT, stdout=err, stderr=err)
            wait_for(proc, "(-.-)", BOOT_TIMEOUT)
            wait_for(proc, "SYS_GETS: Debut", BOOT_TIMEOUT)
            monitor = monitor_connect()
            time.sleep(0.8)
            screendump(monitor, "01-shell.png")

            send_command(monitor, "help")
            wait_for(proc, "ligne lue: help", 25)
            time.sleep(0.8)
            screendump(monitor, "02-help-bottom.png")

            mon(monitor, "sendkey pgup")
            time.sleep(0.5)
            mon(monitor, "sendkey pgup")
            time.sleep(0.8)
            screendump(monitor, "03-help-pageup.png")

            mon(monitor, "sendkey pgdn")
            time.sleep(0.5)
            mon(monitor, "sendkey pgdn")
            time.sleep(0.8)
            screendump(monitor, "04-help-pagedown.png")

            send_command(monitor, "ls")
            wait_for(proc, "Initrd / VFS", 20)
            time.sleep(0.6)
            screendump(monitor, "05-ls.png")

            send_command(monitor, "fat16-list")
            wait_for(proc, "ligne lue: fat16-list", 20)
            time.sleep(0.6)
            screendump(monitor, "06-fat16-list.png")

            send_command(monitor, "fat16-cat fatok.txt")
            wait_for(proc, "ligne lue: fat16-cat", 25)
            time.sleep(0.6)
            screendump(monitor, "07-fat16-cat.png")

            send_command(monitor, "net-status")
            wait_for(proc, "ligne lue: net-status", 20)
            time.sleep(0.6)
            screendump(monitor, "08-net-status.png")

            send_command(monitor, "ps")
            wait_for(proc, "ligne lue: ps", 20)
            time.sleep(0.6)
            screendump(monitor, "09-ps.png")

            send_command(monitor, "mem")
            wait_for(proc, "ligne lue: mem", 20)
            time.sleep(0.6)
            screendump(monitor, "10-mem.png")

            send_command(monitor, "ai-runtime")
            wait_for(proc, "Runtime IA bare-metal", 25)
            time.sleep(0.6)
            screendump(monitor, "11-ai-runtime.png")
        print("GUI capture done")
        return 0
    finally:
        if monitor is not None:
            monitor.close()
        terminate(proc)


if __name__ == "__main__":
    sys.exit(main())
