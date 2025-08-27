#!/bin/bash

# AI-OS - Script de lancement simple et optimisé
# Version corrigée sans touches fantômes

set -e

echo "=== AI-OS - LANCEMENT SIMPLE ET STABLE ==="
echo "Driver: Clavier basé interruptions pures (pas de polling)"
echo "QEMU: Configuration optimisée et universelle"
echo ""

# Vérifier compilation
if [ ! -f "build/ai_os.bin" ] || [ ! -f "my_initrd.tar" ]; then
    echo "🔨 Compilation requise..."
    make clean && make
    if [ $? -ne 0 ]; then
        echo "❌ Erreur de compilation"
        exit 1
    fi
fi

echo "✅ AI-OS Prêt"
echo "   Noyau: build/ai_os.bin ($(du -h build/ai_os.bin | cut -f1))"
echo "   Initrd: my_initrd.tar ($(du -h my_initrd.tar | cut -f1))"
echo ""

echo "📝 INSTRUCTIONS:"
echo "   1. ⏳ Attendez le shell AI-OS (quelques secondes)"
echo "   2. 🖱️ Cliquez dans la fenêtre QEMU pour capturer le clavier"  
echo "   3. ⌨️ Tapez des lettres pour tester (plus de touches fantômes!)"
echo "   4. ✅ Tapez 'help' si ça marche"
echo "   5. 🔴 Ctrl+Alt+G pour libérer la souris, fermez pour quitter"
echo ""
echo "🚀 Lancement dans 2 secondes..."
sleep 2

# Configuration QEMU optimale et compatible
exec qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -display gtk \
    -chardev stdio,id=serial0 \
    -device isa-serial,chardev=serial0 \
    -no-reboot -no-shutdown
