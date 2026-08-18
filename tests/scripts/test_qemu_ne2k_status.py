#!/usr/bin/env python3
"""Smoke QEMU du statut NIC dynamique avec un contrôleur NE2000 ISA."""
import os, socket, subprocess, time
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),"..","..")); LOG_DIR=os.path.join(ROOT,"test_logs")
LOG=os.path.join(LOG_DIR,"ne2k-status.log"); ERR=os.path.join(LOG_DIR,"ne2k-status.err"); MON=os.path.join(LOG_DIR,"ne2k-status-monitor.sock")

def text():
    try:
        with open(LOG,errors="replace") as f:return f.read()
    except OSError:return ""
def wait(needle,proc,timeout=20):
    end=time.time()+timeout
    while time.time()<end:
        if proc.poll() is not None: raise RuntimeError("QEMU stopped early")
        if needle in text(): return
        time.sleep(.1)
    raise RuntimeError("missing output: "+needle)
def monitor():
    end=time.time()+5
    while time.time()<end:
        if os.path.exists(MON):
            s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
            try:s.connect(MON);s.settimeout(.1);return s
            except OSError:s.close()
        time.sleep(.1)
    raise RuntimeError("monitor unavailable")
def keys(s,cmd):
    special={" ":"spc",".":"dot","-":"minus","_":"shift-minus"}
    for c in [special.get(x,x.lower()) for x in cmd]+["ret"]:
        s.sendall(("sendkey "+c+"\n").encode());time.sleep(.25)
def main():
    os.makedirs(LOG_DIR,exist_ok=True)
    for p in (LOG,ERR,MON):
        try:os.remove(p)
        except OSError:pass
    cmd=["qemu-system-i386","-kernel",os.path.join(ROOT,"build","ai_os.bin"),"-initrd",os.path.join(ROOT,"my_initrd.tar"),"-m","1024M","-display","none","-vga","none","-serial","file:"+LOG,"-monitor","unix:%s,server,nowait"%MON,"-machine","type=pc,accel=tcg","-netdev","user,id=n0","-device","ne2k_isa,netdev=n0","-no-reboot","-no-shutdown"]
    with open(ERR,"wb") as err:
        p=subprocess.Popen(cmd,stdout=err,stderr=err);s=None
        try:
            wait("(-.-)",p);s=monitor();time.sleep(.5);before=len(text());keys(s,"net-status json");end=time.time()+15
            while time.time()<end:
                if '"nic":"detected"' in text()[before:]:
                    keys(s,"ai-runtime")
                    wait("Session LLM noyau  : IDLE (NE2000 pret)",p)
                    wait("Bail DHCP noyau    : absent",p)
                    print("QEMU NE2000 status smoke passed.")
                    return 0
                if p.poll() is not None:raise RuntimeError("QEMU stopped while querying net-status")
                time.sleep(.1)
            raise RuntimeError("net-status did not report nic=detected; log tail: "+text()[-500:])
        finally:
            if s:s.close()
            if p.poll() is None:p.terminate();p.wait(timeout=3)
if __name__=="__main__":
    try:raise SystemExit(main())
    except Exception as e: print("NE2000 status smoke failed: "+str(e)); raise SystemExit(1)
