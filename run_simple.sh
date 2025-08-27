#!/bin/bash

# AI-OS - Script de lancement simple et compatible
# Version finale avec configuration QEMU universelle

set -e

echo "=== AI-OS - LANCEMENT SIMPLIFIÉ ==="
echo "Driver: Clavier hybride (interruption + polling)"
echo "QEMU: Configuration universellement compatible"
echo ""

# Vérifier compilation
if [ ! -f "build/ai_os.bin" ] || [ ! -f "my_initrd.tar" ]; then
    echo "🔨 Compilation requise..."
    make clean && make
fi

echo "✅ AI-OS Prêt"
echo "   Noyau: build/ai_os.bin ($(du -h build/ai_os.bin | cut -f1))"
echo "   Initrd: my_initrd.tar ($(du -h my_initrd.tar | cut -f1))"
echo ""

echo "📝 INSTRUCTIONS:"
echo "   1. Attendez le shell AI-OS"
echo "   2. Cliquez dans la fenêtre QEMU"  
echo "   3. Tapez des lettres pour tester"
echo "   4. Tapez 'help' si ça marche"
echo "   5. Fermez la fenêtre pour quitter"
echo ""
echo "🚀 Lancement..."
sleep 2

# Configuration QEMU simple et compatible
qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -display gtk \
    -chardev stdio,id=serial0 \
    -device isa-serial,chardev=serial0 \
    -no-reboot -no-shutdown
