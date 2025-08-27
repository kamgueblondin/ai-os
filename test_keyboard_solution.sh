#!/bin/bash

echo "=== Test de la Correction du Clavier AI-OS ==="
echo "Ce script lance AI-OS avec interface graphique pour tester le clavier"
echo ""
echo "Instructions :"
echo "1. Une fenêtre QEMU va s'ouvrir"
echo "2. Tapez des caractères pour tester le clavier"
echo "3. Essayez : a, b, c, Enter, Espace, etc."
echo "4. Tapez 'help' puis Enter pour voir les commandes"
echo "5. Appuyez Ctrl+C dans ce terminal pour arrêter"
echo ""
read -p "Appuyez sur Entrée pour lancer le test graphique..."

# Lancer avec interface graphique pour test interactif
cd /workspace/ai-os
qemu-system-i386 -kernel build/ai_os.bin -initrd my_initrd.tar \
    -m 256M -vga std \
    -machine type=pc,accel=tcg \
    -device i8042 \
    -serial stdio \
    -rtc base=utc -no-reboot

echo ""
echo "=== Test terminé ==="
