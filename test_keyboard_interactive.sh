#!/bin/bash

echo "=== Test du clavier AI-OS ==="
echo "Lancement de QEMU avec interface graphique..."
echo "Appuyez sur des touches pour tester le clavier"
echo "Utilisez Ctrl+Alt+G pour libérer la souris"
echo "Fermez la fenêtre QEMU pour terminer le test"

# Lancer QEMU avec interface graphique et logs série
qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -serial file:keyboard_test_interactive.log \
    -m 32M \
    -vga std

echo "Test terminé. Vérification des logs..."
if [ -f keyboard_test_interactive.log ]; then
    echo "=== Logs série ==="
    tail -20 keyboard_test_interactive.log
else
    echo "Aucun log généré"
fi

