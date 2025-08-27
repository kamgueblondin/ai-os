#!/bin/bash

echo "=== TEST FINAL DU CLAVIER AI-OS ==="
echo "DIAGNOSTIC: Les interruptions IRQ1 fonctionnent !"
echo "PROBLÈME: QEMU console ne génère que des codes PS/2 de contrôle"
echo "SOLUTION: Test avec interface graphique pour vraie saisie clavier"
echo

# Recompilation propre
make clean
make

echo
echo "🚀 LANCEMENT QEMU GUI POUR TEST CLAVIER..."
echo "📋 INSTRUCTIONS:"
echo "1. Une fenêtre QEMU va s'ouvrir"
echo "2. Cliquez dans la fenêtre pour capturer le clavier"
echo "3. Tapez des touches pour tester les interruptions"
echo "4. Les logs s'afficheront dans ce terminal"
echo "5. Appuyez Ctrl+Alt+G pour libérer la souris"
echo

# Test avec interface graphique et logs série
qemu-system-i386 -kernel build/ai_os.bin -initrd my_initrd.tar \
    -m 256M \
    -vga std \
    -machine type=pc,accel=tcg \
    -device i8042 \
    -serial stdio \
    -rtc base=utc \
    -no-reboot
