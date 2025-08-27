#!/bin/bash

echo "=== Test Debug AI-OS ==="
echo "Compilation du système..."

# Nettoyer et recompiler
make clean
make all

if [ $? -ne 0 ]; then
    echo "ERREUR: Compilation échouée"
    exit 1
fi

echo "Lancement du système en mode debug..."

# Lancer QEMU avec logs détaillés
timeout 30s qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -nographic \
    -serial file:debug_output.log \
    -monitor stdio \
    -d int,cpu_reset,guest_errors \
    -D qemu_debug.log

echo "=== Logs de sortie ==="
if [ -f debug_output.log ]; then
    cat debug_output.log
else
    echo "Aucun log de sortie trouvé"
fi

echo "=== Logs QEMU Debug ==="
if [ -f qemu_debug.log ]; then
    head -50 qemu_debug.log
else
    echo "Aucun log QEMU trouvé"
fi

