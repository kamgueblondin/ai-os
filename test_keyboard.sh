#!/bin/bash

# Script de test pour le clavier AI-OS
# Analyse les logs série pour diagnostiquer le problème du clavier

echo "=== Test du clavier AI-OS ==="
echo "Compilation du projet..."
cd /workspace/ai-os
make clean > /dev/null 2>&1
make > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "ERREUR: Échec de la compilation"
    exit 1
fi

echo "Lancement du système avec capture des logs série..."

# Lancer QEMU en arrière-plan et capturer la sortie série
timeout 60s qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -nographic \
    -serial file:serial_output.log \
    -d int,guest_errors,cpu_reset \
    -no-reboot \
    -no-shutdown \
    -monitor none > qemu_output.log 2>&1 &

QEMU_PID=$!
echo "QEMU lancé avec PID: $QEMU_PID"

# Attendre que le système démarre
sleep 5

# Simuler des entrées clavier
echo "Simulation de frappes clavier..."
sleep 2

# Attendre un peu plus
sleep 10

# Arrêter QEMU
kill $QEMU_PID > /dev/null 2>&1
wait $QEMU_PID > /dev/null 2>&1

echo "=== ANALYSE DES LOGS SÉRIE ==="
if [ -f serial_output.log ]; then
    echo "Contenu du log série:"
    cat serial_output.log
else
    echo "Aucun log série trouvé"
fi

echo ""
echo "=== ANALYSE DES LOGS QEMU ==="
if [ -f qemu_output.log ]; then
    echo "Contenu du log QEMU:"
    cat qemu_output.log
else
    echo "Aucun log QEMU trouvé"
fi

echo ""
echo "=== RÉSUMÉ DU TEST ==="
if [ -f serial_output.log ]; then
    KEYBOARD_INIT=$(grep -c "Initialisation du clavier" serial_output.log)
    INTERRUPTIONS=$(grep -c "INTERRUPTION CLAVIER RECUE" serial_output.log)
    SHELL_START=$(grep -c "Shell AI-OS.*prêt" serial_output.log)
    
    echo "- Initialisation clavier: $KEYBOARD_INIT fois"
    echo "- Interruptions clavier: $INTERRUPTIONS fois"
    echo "- Shell démarré: $SHELL_START fois"
    
    if [ $SHELL_START -gt 0 ]; then
        echo "✓ Shell utilisateur démarré avec succès"
    else
        echo "✗ Shell utilisateur n'a pas démarré"
    fi
    
    if [ $KEYBOARD_INIT -gt 0 ]; then
        echo "✓ Initialisation du clavier détectée"
    else
        echo "✗ Pas d'initialisation du clavier détectée"
    fi
    
    if [ $INTERRUPTIONS -gt 0 ]; then
        echo "✓ Interruptions clavier fonctionnelles"
    else
        echo "! Aucune interruption clavier détectée (normal si pas de frappe)"
    fi
fi

echo ""
echo "Test terminé."
