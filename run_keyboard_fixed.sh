#!/bin/bash

# AI-OS - Script de lancement optimisé pour test clavier
# Version finale corrigée - Interruptions pures

set -e

echo "=== AI-OS KEYBOARD SOLUTION - VERSION FINALE ==="
echo "Driver: Interruptions pures (ZERO polling = ZERO touches fantômes)"
echo "QEMU: Configuration optimisée et testée"
echo ""

# Vérifier que le système est compilé
if [ ! -f "build/ai_os.bin" ] || [ ! -f "my_initrd.tar" ]; then
    echo "🔨 Compilation nécessaire..."
    
    # Nettoyer et compiler
    make clean
    make
    
    if [ $? -ne 0 ]; then
        echo "❌ ERREUR DE COMPILATION"
        exit 1
    fi
fi

echo "✅ Système AI-OS prêt (version corrigée)"
echo "   Noyau: build/ai_os.bin ($(du -h build/ai_os.bin | cut -f1))"
echo "   Initrd: my_initrd.tar ($(du -h my_initrd.tar | cut -f1))"
echo ""

echo "🎯 CORRECTIONS APPLIQUÉES:"
echo "   ✓ Makefile: Conflits -serial stdio résolus"
echo "   ✓ Driver clavier: Polling agressif supprimé"
echo "   ✓ Buffer: Gestion simplifiée et efficace"
echo "   ✓ QEMU: Configuration optimisée"
echo ""

echo "🚀 Lancement avec configuration finale optimisée..."
echo ""
echo "📝 INSTRUCTIONS D'UTILISATION:"
echo "   1. ⏳ Attendez l'apparition du prompt AI-OS (~5-10 sec)"
echo "   2. 🖱️ Cliquez dans la fenêtre pour capturer le clavier"
echo "   3. ⌨️ Tapez des lettres simples (a, b, c, etc.)"
echo "   4. ✅ Plus de caractères fantômes en boucle!"
echo "   5. 📝 Tapez 'help' pour voir les commandes disponibles"
echo "   6. ⏹️ Ctrl+Alt+G pour libérer la souris, fermez pour quitter"
echo ""
echo "⚡ Démarrage imminent..."
sleep 3

# Configuration QEMU finale testée et optimisée
exec qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -machine pc \
    -cpu pentium3 \
    -vga std \
    -display gtk,zoom-to-fit=on \
    -chardev stdio,id=serial0 \
    -device isa-serial,chardev=serial0 \
    -no-reboot \
    -no-shutdown
