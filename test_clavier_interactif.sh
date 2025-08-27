#!/bin/bash

# Test interactif du clavier AI-OS v6.0
echo "🔧 Test Interactif du Clavier - AI-OS v6.0"
echo "=========================================="
echo ""
echo "📋 Instructions de test :"
echo "1. Le système va démarrer dans QEMU"
echo "2. Attendez l'affichage du shell 'AI-OS>'"
echo "3. Tapez quelques caractères au clavier"
echo "4. Les caractères devraient s'afficher à l'écran"
echo "5. Appuyez sur ENTRÉE pour valider une commande"
echo "6. Tapez 'help' pour tester une commande complète"
echo "7. Tapez Ctrl+C dans le terminal pour quitter QEMU"
echo ""
echo "🚀 Lancement du test dans 3 secondes..."
sleep 3

echo "🎮 QEMU en cours de lancement..."
echo ""

# Compiler d'abord
make clean && make all

# Lancer QEMU sans timeout
qemu-system-i386 -kernel build/ai_os.bin -initrd my_initrd.tar \
    -nographic -serial stdio \
    -machine type=pc,accel=tcg \
    -device i8042 \
    -d int,guest_errors \
    -no-reboot -no-shutdown -monitor none

echo ""
echo "🏁 Test terminé."
