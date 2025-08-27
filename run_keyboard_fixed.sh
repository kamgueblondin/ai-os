#!/bin/bash

# AI-OS - Script de lancement optimisé pour QEMU
# Version corrigée avec driver clavier hybride

set -e

echo "=== AI-OS KEYBOARD SOLUTION - LAUNCH OPTIMISÉ ==="
echo "Driver: Hybride (interruption + polling fallback)"
echo "QEMU: Configuration optimisée pour émulation clavier"
echo ""

# Vérifier que le système est compilé
if [ ! -f "build/ai_os.bin" ] || [ ! -f "my_initrd.tar" ]; then
    echo "🔨 Compilation nécessaire..."
    
    # Nettoyer
    rm -rf build
    rm -f my_initrd.tar
    make clean
    
    # Appliquer la correction
    if [ -f "kernel/keyboard_ultimate.c" ]; then
        echo "   ✓ Application du driver clavier ultimate"
        cp kernel/keyboard_ultimate.c kernel/keyboard.c
    fi
    
    # Compiler
    make
    
    if [ $? -ne 0 ]; then
        echo "❌ ERREUR DE COMPILATION"
        exit 1
    fi
fi

echo "✅ Système AI-OS prêt"
echo "   Noyau: build/ai_os.bin ($(du -h build/ai_os.bin | cut -f1))"
echo "   Initrd: my_initrd.tar ($(du -h my_initrd.tar | cut -f1))"
echo ""

# Configuration QEMU optimisée basée sur les tests
echo "🚀 Lancement QEMU avec configuration optimisée..."
echo ""
echo "📝 INSTRUCTIONS D'UTILISATION:"
echo "   1. ⏳ Attendez l'apparition du prompt AI-OS"
echo "   2. 🖱️ Cliquez dans la fenêtre pour capturer le clavier"
echo "   3. ⌨️ Tapez des lettres simples pour tester"
echo "   4. ✅ Si ça marche: tapez 'help' pour voir les commandes"
echo "   5. 🔄 Le système supporte interruption + polling automatique"
echo "   6. ⏹️ Ctrl+Alt+G pour libérer la souris, fermez pour quitter"
echo ""
echo "⚡ Lancement dans 3 secondes..."
sleep 3

# Configuration optimale identifiée
exec qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -machine pc \
    -cpu pentium3 \
    -device i8042 \
    -device ps2-kbd,id=kbd \
    -vga std \
    -display gtk,zoom-to-fit=on \
    -chardev stdio,id=serial0 \
    -device isa-serial,chardev=serial0 \
    -monitor none \
    -no-reboot \
    -no-shutdown
